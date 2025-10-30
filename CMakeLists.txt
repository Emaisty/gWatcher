cmake_minimum_required(VERSION 3.10)
project(gwatch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gwatch src/main.cpp)


enable_testing()
find_package(GTest REQUIRED)

file(COPY tests/test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(gwatch_test tests/tests.cpp)

target_link_libraries(gwatch_test PRIVATE GTest::gtest_main)

add_test(NAME gwatch_test COMMAND gwatch_test)
