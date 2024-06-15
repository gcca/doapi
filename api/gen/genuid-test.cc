#include <cstddef>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "genuid.hpp"

namespace std {
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
    if (t_.joinable()) {
      t_.join();
    }
  }

private:
  std::thread t_;
};
} // namespace std

TEST(GenUID, MultiGeneration) {
  genuid::InitParameters();

  constexpr std::size_t uids_size = 10000;

  std::vector<std::size_t> uids;
  uids.resize(uids_size);

  {
    std::vector<std::jthread> gen_threads;
    gen_threads.reserve(uids_size);

    for (std::size_t i = 0; i < uids_size; i++) {
      gen_threads.emplace_back(
          [&uids](const std::size_t i) { uids[i] = genuid::GenerateUID(); }, i);
    }
  }

  std::unordered_set<std::size_t> counter;
  for (auto uid : uids) {
    ASSERT_EQ(counter.count(uid), 0) << ">>> counter.size=" << counter.size()
                                     << " uids.size=" << uids.size();

    counter.insert(uid);
  }
}
