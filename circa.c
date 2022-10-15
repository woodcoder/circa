#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

struct response_t {
  char *memory;
  size_t size;
};
 
static size_t
write_response(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct response_t *mem = (struct response_t *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

int
main(int argc, char* argv[])
{
  CURL *curl;
  CURLcode res;
  struct response_t response;
 
  response.memory = malloc(1);  /* will be grown as needed by the realloc above */
  response.size = 0;    /* no data at this point */
 
  curl_global_init(CURL_GLOBAL_DEFAULT);
 
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL,
      "https://carbon-aware-api.azurewebsites.net/emissions/forecasts/current?location=eastus&dataStartAt=2022-10-15T18%3A00%3A00Z&dataEndAt=2022-10-15T22%3A00%3A00Z&windowSize=30");

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
 
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
    }
    else {
      printf("%lu bytes retrieved\n", (unsigned long)response.size);
    }
    curl_easy_cleanup(curl);
  }

  free(response.memory);

  curl_global_cleanup();
 
  return 0;
}
