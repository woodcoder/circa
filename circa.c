#include <errno.h>
#include <float.h>
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

struct response_t {
  char *text;
  size_t size;
};

static size_t write_response(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  struct response_t *mem = (struct response_t *)userp;

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

int main(int argc, char *argv[]) {
  char end[ISO8601_URL_SIZE];
  int window = 30;
  struct tm *time_tm;
  char url[URL_SIZE];
  CURL *curl;
  CURLcode res;
  struct response_t response;
  time_t now;
  double wait_seconds = -DBL_MAX;

  if (argc < 4) {
    printf("Usage: %s HOURS [COMMAND [ARG]...]\n", argv[0]);
    printf("\
Run COMMAND at somepoint in the next few HOURS (between 1 and 24) when\n\
local (%s) carbon intensity is at its lowest.  Assumes command will\n\
complete within %d minute window.\n",
           "eastus", 30);
    return 1;
  }

  size_t i = 1;
  char *p;
  int hours;

  errno = 0;
  long conv = strtol(argv[i], &p, 10);

  if (errno != 0 || *p != '\0' || conv > 24 || conv < 1) {
    fprintf(stderr, "error: hours %s must be between 1 and 24\n", argv[i]);
    return 1;
  } else {
    hours = conv;
  }
  i++;

  time(&now);
  // add time this way isn't necessarily portable!
  now = now + hours * 60 * 60;
  time_tm = gmtime(&now);
  snprintf(end, ISO8601_URL_SIZE, ISO8601_URL_FORMAT,
           time_tm->tm_year + 1900, // years from 1900
           time_tm->tm_mon + 1,     // 0-based
           time_tm->tm_mday,        // 1-based
           time_tm->tm_hour, time_tm->tm_min, 0);

  printf("Requesting %d min window before %02d:%02d UTC\n", window,
         time_tm->tm_hour, time_tm->tm_min);

  snprintf(url, URL_SIZE, URL_FORMAT, "eastus", end, window);

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
      size_t i;
      json_t *root;
      json_error_t error;

      printf("Carbon Aware API returned %lu bytes\n",
             (unsigned long)response.size);
      root = json_loads(response.text, 0, &error);

      if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
      }

      if (!json_is_array(root)) {
        fprintf(stderr, "error: root is not an array\n");
        fprintf(stderr, "RESPONSE\n%s\n", response.text);
        json_decref(root);
        return 1;
      }

      for (i = 0; i < json_array_size(root); i++) {
        json_t *data, *optimals, *optimal, *timestamp;
        const char *timestamp_text;

        data = json_array_get(root, i);
        if (!json_is_object(data)) {
          fprintf(stderr, "error: forecast data %d is not an object\n", (int)i);
          json_decref(root);
          return 1;
        }

        optimals = json_object_get(data, "optimalDataPoints");
        if (!json_is_array(optimals)) {
          fprintf(stderr, "error: optimalDataPoints is not an array\n");
          json_decref(root);
          return 1;
        }

        // just look at the first for now
        optimal = json_array_get(optimals, 0);
        if (!json_is_object(optimal)) {
          fprintf(stderr,
                  "error: forecast %d optimalDataPoints %d is not an object\n",
                  (int)i, (int)0);
          json_decref(root);
          return 1;
        }

        timestamp = json_object_get(optimal, "timestamp");
        if (!json_is_string(timestamp)) {
          fprintf(stderr,
                  "error: forecast %d optimalDataPoints %d timestamp is not a "
                  "string\n",
                  (int)i, (int)0);
          json_decref(root);
          return 1;
        }

        timestamp_text = json_string_value(timestamp);
        printf("Optimal timestamp %s\n", timestamp_text);

        // assume fixed GMT iso8601 format
        int y, M, d, h, m, s;
        sscanf(timestamp_text, "%d-%d-%dT%d:%d:%d+00:00", &y, &M, &d, &h, &m,
               &s);
        printf("Optimal time %02i:%02i UTC\n", h, m);

        struct tm timestamp_tm = {.tm_year = y - 1900, // years from 1900
                                  .tm_mon = M - 1,     // 0-based
                                  .tm_mday = d,        // 1-based
                                  .tm_hour = h,
                                  .tm_min = m,
                                  .tm_sec = s};
        time_t optimal_time;
        optimal_time = timegm(&timestamp_tm); // or _mkgmtime() on windows
        time(&now);
        wait_seconds = difftime(optimal_time, now);
        printf("Sleeping for %.2f minutes\n", wait_seconds / 60);
      }
    }
    curl_easy_cleanup(curl);
  }

  free(response.text);

  curl_global_cleanup();

  if (wait_seconds > 0) {
    sleep(wait_seconds);
  }

  // replace this program with the command
  execvp(argv[i], &argv[i]);

  int exit_status =
      errno == ENOENT ? 127 : 126; // EXIT_ENOENT : EXIT_CANNOT_INVOKE;
  if (errno != 0) {
    fprintf(stderr, "%s\n", strerror(errno));
  }
  return exit_status;
}
