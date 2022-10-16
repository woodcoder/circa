#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <jansson.h>

#define ISO8601_URL_FORMAT "%d-%02d-%02dT%02d%%3A%02d%%3A%02dZ"
#define ISO8601_URL_SIZE 24
#define URL_FORMAT                                                             \
  "https://carbon-aware-api.azurewebsites.net/emissions/forecasts/"            \
  "current?location=%s&dataEndAt=%s&windowSize=%d"
#define URL_SIZE 256

typedef struct Params_t {
  int hours;
  int window;
  size_t command;
} params_t;

bool parse_args(int argc, char *argv[], params_t *params) {
  if (argc < 4) {
    return false;
  }

  size_t i = 1;
  char *p;
  int hours;

  errno = 0;
  long conv = strtol(argv[i], &p, 10);

  if (errno != 0 || *p != '\0' || conv > 24 || conv < 1) {
    fprintf(stderr, "Error: hours %s must be between 1 and 24\n", argv[i]);
    return false;
  } else {
    params->hours = conv;
  }
  i++;

  params->window = 30;

  params->command = i;
  return true;
}

void print_usage(char *name) {
  printf("Usage: %s HOURS [COMMAND [ARG]...]\n", name);
  printf("\
Run COMMAND at somepoint in the next few HOURS (between 1 and 24) when\n\
local (%s) carbon intensity is at its lowest.  Assumes command will\n\
complete within %d minute window.\n",
         "eastus", 30);
}

void format_url(params_t *params, char *url) {
  time_t now;
  struct tm *time_tm;
  char end[ISO8601_URL_SIZE];

  time(&now);
  // add time this way isn't necessarily portable!
  now = now + params->hours * 60 * 60;
  time_tm = gmtime(&now);
  snprintf(end, ISO8601_URL_SIZE, ISO8601_URL_FORMAT,
           time_tm->tm_year + 1900, // years from 1900
           time_tm->tm_mon + 1,     // 0-based
           time_tm->tm_mday,        // 1-based
           time_tm->tm_hour, time_tm->tm_min, 0);

  printf("Requesting %d min window before %02d:%02d UTC\n", params->window,
         time_tm->tm_hour, time_tm->tm_min);

  snprintf(url, URL_SIZE, URL_FORMAT, "eastus", end, params->window);
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
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->text = ptr;
  memcpy(&(mem->text[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->text[mem->size] = 0;

  return realsize;
}

void call_api(char *url, void (*extra_data)(response_t *, void *), void *data) {
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
      extra_data(&response, data);
    }
    curl_easy_cleanup(curl);
  }

  free(response.text);

  curl_global_cleanup();
}

void parse_response(response_t *response, void *wait_seconds) {
  size_t i;
  json_t *root;
  json_error_t error;

  printf("Carbon Aware API returned %lu bytes\n",
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
      fprintf(stderr, "error: optimalDataPoints is not an array\n");
      json_decref(root);
      return;
    }

    // just look at the first for now
    optimal = json_array_get(optimals, 0);
    if (!json_is_object(optimal)) {
      fprintf(stderr,
              "error: forecast %d optimalDataPoints %d is not an object\n",
              (int)i, (int)0);
      json_decref(root);
      return;
    }

    timestamp = json_object_get(optimal, "timestamp");
    if (!json_is_string(timestamp)) {
      fprintf(stderr,
              "error: forecast %d optimalDataPoints %d timestamp is not a "
              "string\n",
              (int)i, (int)0);
      json_decref(root);
      return;
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
    optimal_time = timegm(&timestamp_tm); // or _mkgmtime() on windows
    time(&now);
    *(double *)wait_seconds = difftime(optimal_time, now);
  }
}

int check_errors() {
  int exit_status =
      errno == ENOENT ? 127 : 126; // EXIT_ENOENT : EXIT_CANNOT_INVOKE;
  if (errno != 0) {
    fprintf(stderr, "%s\n", strerror(errno));
  }
  return exit_status;
}

int main(int argc, char *argv[]) {
  params_t params;
  bool success = parse_args(argc, argv, &params);

  if (!success) {
    print_usage(argv[0]);
    return 1;
  }

  char url[URL_SIZE];
  format_url(&params, url);

  double wait_seconds = -DBL_MAX;
  call_api(url, parse_response, &wait_seconds);

  printf("Sleeping for %.2f minutes\n", wait_seconds / 60);

  if (wait_seconds > 0) {
    sleep(wait_seconds);
  }

  // replace this program with the command
  execvp(argv[params.command], &argv[params.command]);

  return check_errors();
}
