#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>

#include <curl/curl.h>

#define CROW_DISABLE_STATIC_DIR
#include <crow_all.h>

#include "gen/genuid.hpp"

std::size_t wcallback(char *contents, std::size_t size, std::size_t nmemb,
                      std::string *s) {
  std::size_t tsize = size * nmemb;
  s->append(contents, tsize);
  return tsize;
}

void InitEnv() {
  std::string data;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "localhost:8080/do");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                     "COUNT doapiserv\nINC doapiserv");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wcallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      std::cerr << "InitEnv failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();

  crow::json::rvalue jsondata = crow::json::load(data);

  setenv("DATACENTER_ID", "0", 1);
  setenv("MACHINE_ID", jsondata["results"][0].s().begin(), 1);
}

int main() {
  std::cout << "Creating app" << std::endl;

  InitEnv();

  crow::SimpleApp app;
  genuid::InitParameters();

  CROW_ROUTE(app, "/")([]() { return "🙂"; });

  CROW_ROUTE(app, "/genuid")
  ([]() { return crow::json::wvalue{{"uid", genuid::GenerateUID()}}; });

  CROW_ROUTE(app, "/genuid/<int>")
  ([](std::size_t num) {
    if (num > 4096) {
      return crow::response(
          crow::status::BAD_REQUEST,
          crow::json::wvalue{
              {"errors", crow::json::wvalue::list{"Exceeded 1000 limit"}}});
    }

    crow::json::wvalue::list uids;
    uids.reserve(num);
    std::generate_n(std::back_inserter(uids), num, genuid::GenerateUID);

    return crow::response(crow::status::OK, crow::json::wvalue{{"uids", uids}});
  });

  app.port(8000).multithreaded().run();

  return EXIT_SUCCESS;
}
