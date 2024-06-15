#include <gtest/gtest.h>
#include <thread>

#include "divi.hpp"

namespace divi_std {
class jthread {
public:
  template <class T, class... A>
  explicit jthread(T &&t, A &&...a)
      : t_{std::forward<T>(t), std::forward<A>(a)...} {}

  jthread(const jthread &) = delete;
  jthread &operator=(const jthread &) = delete;

  jthread(jthread &&j) noexcept : t_{std::move(j.t_)} {}
  jthread &operator=(jthread &&j) noexcept {
    t_ = std::move(j.t_);
    return *this;
    ;
  }
  ~jthread() {
    if (t_.joinable()) { t_.join(); }
  }

private:
  std::thread t_;
};
} // namespace divi_std

TEST(PlanPlaygroundGPTest, Explain) {
  std::string text = "GET key\nPUT key value\nGET key";

  auto text_plan = divi::TextPlan::MakeFromLRaw(text);

  ASSERT_EQ(text_plan->Explain(),
            "op:GET args:[key]\nop:PUT args:[key, value]\nop:GET args:[key]");

  auto logical_plan = text_plan->ToLogicalPlan();

  ASSERT_EQ(logical_plan.Explain(), "GET:key\nPUT:key,value\nGET:key");

  divi::Bucket bucket;
  std::vector<divi::Result> results = logical_plan.Execute(bucket);

  ASSERT_EQ(bucket.Dump(), "s_map:\nkey=value");

  std::vector<std::string> outputs;
  outputs.reserve(results.size());
  std::transform(results.cbegin(), results.cend(), std::back_inserter(outputs),
                 [](const divi::Result &r) { return std::string{r.view()}; });

  ASSERT_EQ(outputs, (std::vector<std::string>{"", "$ok", "value"}));
}

TEST(PlanPlaygroundCITest, Explain) {
  std::string text = "COUNT key\nINC key\nINC key";

  auto text_plan = divi::TextPlan::MakeFromLRaw(text);

  ASSERT_EQ(text_plan->Explain(),
            "op:COUNT args:[key]\nop:INC args:[key]\nop:INC args:[key]");

  auto logical_plan = text_plan->ToLogicalPlan();

  ASSERT_EQ(logical_plan.Explain(), "COUNT:key\nINC:key\nINC:key");

  divi::Bucket bucket;
  std::vector<divi::Result> results = logical_plan.Execute(bucket);

  ASSERT_EQ(bucket.Dump(), "c_map:\nkey=2");

  std::vector<std::string> outputs;
  outputs.reserve(results.size());
  std::transform(results.cbegin(), results.cend(), std::back_inserter(outputs),
                 [](const divi::Result &r) { return std::string{r.view()}; });

  ASSERT_EQ(outputs, (std::vector<std::string>{"0", "1", "2"}));
}

TEST(PlanSafeThreadedTest, Inc) {
  std::string text = "INC key\nINC key\nINC key\nINC key";
  const std::size_t count = std::count(text.cbegin(), text.cend(), '\n') + 1;
  ASSERT_EQ(count, 4);

  divi::Bucket bucket;
  auto text_plan = divi::TextPlan::MakeFromLRaw(text);

  const std::size_t threads_size = 20;
  std::vector<std::vector<divi::Result>> threads_results;
  threads_results.resize(threads_size);
  {
    std::vector<divi_std::jthread> threads;
    threads.reserve(threads_size);

    for (std::size_t i = 0; i < threads_size; i++) {
      threads.emplace_back(
          [&](const std::size_t i) {
            threads_results[i] = text_plan->ToLogicalPlan().Execute(bucket);
          },
          i);
    }
  }

  std::vector<std::size_t> results;
  results.reserve(threads_size * count);

  for (const auto &thread_results : threads_results) {
    for (const auto &thread_result : thread_results) {
      results.emplace_back(std::stoul(std::string{thread_result.view()}));
    }
  }

  std::sort(results.begin(), results.end());

  ASSERT_EQ(results,
            (std::vector<std::size_t>{
                1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
                49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
                65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
            }));
}
