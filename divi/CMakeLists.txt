cmake_minimum_required(VERSION 3.29)
project(divi VERSION 0.1.0 LANGUAGES CXX)

add_executable(divi divi.cc)
target_include_directories(divi PRIVATE ../common)
target_link_libraries(divi PRIVATE
  absl_flags_commandlineflag
  absl_flags_commandlineflag_internal
  absl_flags_config
  absl_flags_internal
  absl_flags_marshalling
  absl_flags_parse
  absl_flags_private_handle_accessor
  absl_flags_program_name
  absl_flags_reflection
  absl_flags_usage
  absl_flags_usage_internal
)

option(DISABLE_TESTS "Disable tests" OFF)
if(NOT DISABLE_TESTS)
  find_package(GTest REQUIRED)
  add_executable(divi-test divi-test.cc)

  target_link_libraries(divi-test GTest::gtest_main)
  enable_testing()
  gtest_discover_tests(divi-test)
endif()
