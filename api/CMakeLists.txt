cmake_minimum_required(VERSION 3.25)
project(doapi VERSION 0.1.0 LANGUAGES CXX)

find_package(Boost 1.85 COMPONENTS system REQUIRED)

add_subdirectory(gen)
add_executable(doapi doapi.cc)
target_include_directories(doapi PUBLIC ${Boost_INCLUDE_DIRS} ../common)
target_link_libraries(doapi PRIVATE genuid Boost::system curl)
