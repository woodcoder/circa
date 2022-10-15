#include <stdio.h>
#include <curl/curl.h>

int
main(int argc, char* argv[])
{
  CURL *curl;
  CURLcode res;
 
  curl_global_init(CURL_GLOBAL_DEFAULT);
 
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL,
      "https://carbon-aware-api.azurewebsites.net/emissions/forecasts/current?location=eastus&dataStartAt=2022-10-15T18%3A00%3A00Z&dataEndAt=2022-10-15T22%3A00%3A00Z&windowSize=30");
    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
 
    curl_easy_cleanup(curl);
  }
 
  curl_global_cleanup();
 
  return 0;
}
