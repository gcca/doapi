#include <algorithm>
#include <cstdint>
#include <iostream>

#define CROW_DISABLE_STATIC_DIR
#include <crow_all.h>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/flags/usage_config.h>

#include "divi.hpp"

ABSL_FLAG(std::uint16_t, port, 8000, "App port");

/**
 * DiVI is an in-memory storage over HTTP. Is used to sync or coordinate
 * information through the cluster nodes.
 *
 * Examples:
 *
 * Key-value operations:
 * >>> printf 'GET key\nPUT key value\nGET key' | curl --data-binary @- localhost:8000/do
 * ... {"results":["","$ok","value"]}
 *
 * Counter operations:
 * >>> printf 'COUNT key\nCOUNT key' | curl --data-binary @- localhost:8000/do
 * ... {"results":["0","1"]}
 *
 * Each operations returns an output appended to results fields in response.
 */
int main(int argc, char *argv[]) {
  std::cout << ("\n    _/_/_/    _/  _/      _/  _/_/_/\n"
                "   _/    _/      _/      _/    _/\n"
                "  _/    _/  _/  _/      _/    _/\n"
                " _/    _/  _/    _/  _/      _/\n"
                "_/_/_/    _/      _/      _/_/_/")
            << std::endl
            << "\nWelcome to DiVI." << std::endl;

  absl::FlagsUsageConfig usage_config;
  usage_config.normalize_filename = [](absl::string_view filename) {
    return std::string(filename.substr(filename.rfind("/") + 1));
  };
  usage_config.version_string = []() { return "DiVI 0.1.0"; };
  absl::SetFlagsUsageConfig(usage_config);
  absl::SetProgramUsageMessage("");
  absl::ParseCommandLine(argc, argv);

  divi::Bucket SPOF_bucket;
  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([]() { return "😎"; });

  CROW_ROUTE(app, "/do").methods("POST"_method)([&](const crow::request &req) {
    std::string body = req.body;
    auto text_plan = divi::TextPlan::MakeFromLRaw(body);
    auto results = text_plan->ToLogicalPlan().Execute(SPOF_bucket);

    crow::json::wvalue::list json_results;
    json_results.reserve(results.size());
    std::transform(results.cbegin(), results.cend(),
                   std::back_inserter(json_results), [](const divi::Result &r) {
                     return crow::json::wvalue{std::string{r.view()}};
                   });

    return crow::json::wvalue{{"results", json_results}};
  });

  app.port(absl::GetFlag(FLAGS_port)).multithreaded().run();

  return EXIT_SUCCESS;
}
