add_library(genuid OBJECT genuid.cpp)

option(DISABLE_TESTS "Disable tests" OFF)
if(NOT DISABLE_TESTS)
  find_package(benchmark REQUIRED)
  add_executable(genuid-bench genuid-bench.cc)
  target_link_libraries(genuid-bench benchmark::benchmark_main genuid)

  find_package(GTest REQUIRED)
  add_executable(genuid-test genuid-test.cc)
  target_link_libraries(genuid-test GTest::gtest_main genuid)
  enable_testing()
  gtest_discover_tests(genuid-test)
endif()
