#include <errno.h>
#include <float.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <jansson.h>

#define ISO8601_URL_FORMAT "%4d-%02d-%02dT%02d%%3A%02d%%3A%02dZ"
#define ISO8601_URL_SIZE 26
#define URL_FORMAT                                                             \
  "%s/emissions/forecasts/current?location=%s&dataEndAt=%s&windowSize=%d"
#define URL_SIZE 256

#define ISO8601_CLI_FORMAT "%4d-%02d-%02dT%02d:%02d:%02dZ"
#define ISO8601_CLI_SIZE 22
#define CLI_FORMAT "%s emissions-forecasts -l %s -e %s -w %d"
#define CLI_SIZE 256

typedef struct Params_t {
  bool cli;
  char *url;
  char *location;
  int window;
  int hours;
  size_t command;
} params_t;

// gracefully attempt to load config from a file, silently fail if it
// doesn't exist, warn if it exists, but is not readable
void load_config(const char *path, params_t *params) {
  char line_buffer[255];
  FILE *fp;
  fp = fopen(path, "r");
  if (fp == NULL) {
    if (errno != ENOENT) {
      // if anything other than file not found (e.g. permissions), warn
      fprintf(stderr, "Warning: ignoring error accessing %s '%s'\n", path,
              strerror(errno));
    }
  } else {
    printf("Loading %s\n", path);
    while (fgets(line_buffer, 255, fp)) {
      // strip trailing CRs and LFs
      line_buffer[strcspn(line_buffer, "\r\n")] = 0;
      // ignore blank lines and comments
      if (strnlen(line_buffer, 255) == 0 || line_buffer[0] == '#') {
        continue;
      }

      char key[255];
      char value[255];
      sscanf(line_buffer, "%s %s", key, value);

      if (strcmp(key, "url") == 0) {
        free(params->url);
        params->url = strdup(value);
      }
      if (strcmp(key, "location") == 0) {
        free(params->location);
        params->location = strdup(value);
      }
    }
    fclose(fp);
  }
}

void home_path(char *file, char *path) {
  const char *homedir;
  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }
  strcpy(path, homedir);
  strcat(path, "/.circa/config");
}

void default_args(params_t *params) {
  // copy strings so they can be freed if overridden by config files
  params->cli = false;
  params->url = strdup("https://carbon-aware-api.azurewebsites.net");
  params->location = strdup("eastus");
  params->window = 30;

  load_config("/etc/circa.conf", params);

  char user_config[255];
  home_path("/.circa/config", user_config);
  load_config(user_config, params);
}

bool parse_url_prefix(char *url_str, params_t *params) {
  size_t len = strnlen(url_str, URL_SIZE);
  if (len > 148) {
    // URL_SIZE - 18 (location) - ISO8601_URL_SIZE - 3 (window) - 61 (path)
    return false;
  }
  params->url = url_str;
  return true;
}

bool parse_location(char *location_str, params_t *params) {
  size_t len = strnlen(location_str, URL_SIZE);
  if (len > 18) {
    return false;
  }
  params->location = location_str;
  return true;
}

bool parse_window_duration(char *duration_str, params_t *params) {
  char *p;
  errno = 0;
  long conv = strtol(duration_str, &p, 10);

  if (errno != 0 || *p != '\0' || conv > 6 * 60 || conv < 1) {
    fprintf(stderr,
            "Error: estimated duration %s must be between 1 and 360 minutes (6 "
            "hours)\n",
            duration_str);
    return false;
  }

  params->window = (int)conv;
  return true;
}

bool parse_hours(char *hours_str, params_t *params) {
  char *p;
  errno = 0;
  long conv = strtol(hours_str, &p, 10);

  if (errno != 0 || *p != '\0' || conv > 24 || conv < 1) {
    fprintf(stderr, "Error: hours %s must be between 1 and 24\n", hours_str);
    return false;
  }
  params->hours = (int)conv;
  return true;
}

bool parse_args(int argc, char *argv[], params_t *params) {
  default_args(params);

  if (argc < 2) {
    // we need hours, at least
    return false;
  }

  size_t i = 1;
  while (i < argc && argv[i][0] == '-') {
    bool (*parse_value)(char *, params_t *params) = NULL;
    switch (argv[i][1]) {
    case 'u':
      parse_value = parse_url_prefix;
      break;
    case 'l':
      parse_value = parse_location;
      break;
    case 'd':
      parse_value = parse_window_duration;
      break;
    default:
      // unexpected switch or null
      return false;
    }
    if (parse_value) {
      i++;
      if (i == argc) {
        return false;
      }
      if (!parse_value(argv[i], params)) {
        return false;
      }
    }
    i++;
  }

  if (i == argc) {
    return false;
  }
  if (!parse_hours(argv[i], params)) {
    return false;
  }
  i++;

  // check to see if URL is path to carbon aware cli executable
  if (params->url[0] != 'h') {
    struct stat sb;
    if (stat(params->url, &sb) == 0 && sb.st_mode & S_IXUSR) {
      params->cli = true;
    }
  }

  // anything else is the optional command and its params
  params->command = i;
  return true;
}

void print_usage(char *name, params_t *params) {
  printf("OVERVIEW: Circa - carbon nice scripting\n\n");
  printf("USAGE: %s [option ...] hours [command [argument ...]]\n\n", name);
  printf("DESCRIPTION:\n\
Run COMMAND at somepoint in the next few HOURS (between 1 and 24) when local\n\
(default %s) carbon intensity is at its lowest. Assumes command will complete\n\
complete within a default %d minute duration window. If no command is supplied,\n\
the program will just block until the best time.\n\n",
         params->location, params->window);
  printf("OPTIONS:\n\
  -l <location>     specify location to check for carbon intensity\n\
  -d <duration>     estimated window of runtime of command/task in minutes\n\
  -u <api url>      url prefix of Carbon Aware API server to consult\n");
}

void format_params(params_t *params, size_t iso8601_size, char *iso8601_format,
                   size_t len, char *format, char *param_str) {
  time_t now;
  struct tm *time_tm;
  char end[iso8601_size];
  int size;

  time(&now);
  // add time this way isn't necessarily portable!
  now = now + params->hours * 60 * 60;
  time_tm = gmtime(&now);
  size = snprintf(end, iso8601_size, iso8601_format,
                  time_tm->tm_year + 1900, // years from 1900
                  time_tm->tm_mon + 1,     // 0-based
                  time_tm->tm_mday,        // 1-based
                  time_tm->tm_hour, time_tm->tm_min, 0);

  if (size > iso8601_size) {
    // date string was truncated which should never happen
    fprintf(stderr, "Warning: end date corrupted (iso8601 date truncated)\n");
  }

  printf("Requesting %d min duration window before %02d:%02d UTC in %s\n",
         params->window, time_tm->tm_hour, time_tm->tm_min, params->location);

  snprintf(param_str, len, format, params->url, params->location, end,
           params->window);
}

typedef struct Response_t {
  char *text;
  size_t size;
} response_t;

static size_t write_response(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  response_t *mem = (response_t *)userp;

  char *ptr = realloc(mem->text, mem->size + realsize + 1);
  if (!ptr) {
    // out of memory!
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->text = ptr;
  memcpy(&(mem->text[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->text[mem->size] = 0;

  return realsize;
}

void call_api(char *url, void (*extract_data)(response_t *, void *),
              void *data) {
  CURL *curl;
  CURLcode res;
  response_t response;

  response.text = malloc(1); /* will be grown as needed by the realloc above */
  response.size = 0;         /* no data at this point */

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    } else {
      extract_data(&response, data);
    }
    curl_easy_cleanup(curl);
  }

  free(response.text);

  curl_global_cleanup();
}

void call_cli(char *cmd, void (*extract_data)(response_t *, void *),
              void *data) {
  const size_t BUFSIZE = 255;
  response_t response;

  response.text = malloc(1); // will be grown as needed by the realloc below
  response.size = 0;         // no data at this point

  char buf[BUFSIZE];
  FILE *fp;

  if ((fp = popen(cmd, "r")) == NULL) {
    fprintf(stderr, "Error opening pipe!\n");
    return;
  }

  while (fgets(buf, BUFSIZE, fp) != NULL) {
    size_t read = strnlen(buf, BUFSIZE);
    write_response(buf, sizeof(char), read, &response);
  }

  if (pclose(fp)) {
    fprintf(stderr, "Command not found or exited with error status\n");
    return;
  }

  extract_data(&response, data);

  free(response.text);
}

void parse_response(response_t *response, void *wait_seconds) {
  size_t i;
  json_t *root;
  json_error_t error;

  printf("Carbon Aware SDK returned %lu bytes\n",
         (unsigned long)response->size);
  root = json_loads(response->text, 0, &error);

  if (!root) {
    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    return;
  }

  if (!json_is_array(root)) {
    fprintf(stderr, "error: root is not an array\n");
    fprintf(stderr, "RESPONSE\n%s\n", response->text);
    json_decref(root);
    return;
  }

  for (i = 0; i < json_array_size(root); i++) {
    json_t *data, *optimals, *optimal, *timestamp;
    const char *timestamp_text;

    data = json_array_get(root, i);
    if (!json_is_object(data)) {
      fprintf(stderr, "error: forecast data %d is not an object\n", (int)i);
      json_decref(root);
      return;
    }

    optimals = json_object_get(data, "optimalDataPoints");
    if (!json_is_array(optimals)) {
      // might be cli and in capitals
      optimals = json_object_get(data, "OptimalDataPoints");
      if (!json_is_array(optimals)) {
        fprintf(stderr, "error: optimalDataPoints is not an array\n");
        json_decref(root);
        return;
      }
    }

    // just look at the first for now
    optimal = json_array_get(optimals, 0);
    if (!json_is_object(optimal)) {
      fprintf(stderr,
              "error: forecast %d optimalDataPoints %d is not an object\n",
              (int)i, (int)0);
      fprintf(stderr, "RESPONSE\n%s\n", response->text);
      json_decref(root);
      return;
    }

    timestamp = json_object_get(optimal, "timestamp");
    if (!json_is_string(timestamp)) {
      // might be cli with different name
      timestamp = json_object_get(optimal, "Time");
      if (!json_is_string(timestamp)) {
        fprintf(stderr,
                "error: forecast %d optimalDataPoints %d timestamp is not a "
                "string\n",
                (int)i, (int)0);
        json_decref(root);
        return;
      }
    }

    timestamp_text = json_string_value(timestamp);
    printf("Optimal timestamp %s\n", timestamp_text);

    // assume fixed GMT iso8601 format
    int y, M, d, h, m, s;
    sscanf(timestamp_text, "%d-%d-%dT%d:%d:%d+00:00", &y, &M, &d, &h, &m, &s);
    printf("Optimal time %02i:%02i UTC\n", h, m);

    struct tm timestamp_tm = {.tm_year = y - 1900, // years from 1900
                              .tm_mon = M - 1,     // 0-based
                              .tm_mday = d,        // 1-based
                              .tm_hour = h,
                              .tm_min = m,
                              .tm_sec = s};
    time_t now, optimal_time;
    optimal_time = timegm(&timestamp_tm);
    time(&now);
    *(double *)wait_seconds = difftime(optimal_time, now);
  }
}

int check_errors() {
  int exit_status =
      errno == ENOENT ? 127 : 126; // EXIT_ENOENT : EXIT_CANNOT_INVOKE;
  if (errno != 0) {
    fprintf(stderr, "Error: problem running command '%s'\n", strerror(errno));
  }
  return exit_status;
}

int main(int argc, char *argv[]) {
  params_t params;
  if (!parse_args(argc, argv, &params)) {
    print_usage(argv[0], &params);
    return 1;
  }

  double wait_seconds = -DBL_MAX;
  if (params.cli) {
    char cli[CLI_SIZE];
    format_params(&params, ISO8601_CLI_SIZE, ISO8601_CLI_FORMAT, CLI_SIZE,
                  CLI_FORMAT, cli);
    call_cli(cli, parse_response, &wait_seconds);
  } else {
    char url[URL_SIZE];
    format_params(&params, ISO8601_URL_SIZE, ISO8601_URL_FORMAT, URL_SIZE,
                  URL_FORMAT, url);
    call_api(url, parse_response, &wait_seconds);
  }

  if (wait_seconds > 5 * 60) {
    printf("Sleeping for %.2f minutes\n", wait_seconds / 60);
    sleep(wait_seconds);
  }

  if (params.command >= argc) {
    // no command, just return when ready
    return 0;
  }

  // replace this program with the command
  execvp(argv[params.command], &argv[params.command]);

  return check_errors();
}
