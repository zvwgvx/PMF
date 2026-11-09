cmake_minimum_required(VERSION 3.20)
project(PMF VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pmf-core STATIC src/core/identity.cpp)
target_include_directories(pmf-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(pmf-loader src/loader/main.cpp)
target_link_libraries(pmf-loader PRIVATE pmf-core)

add_executable(pmf-unloader src/unloader/main.cpp)
target_link_libraries(pmf-unloader PRIVATE pmf-core)

enable_testing()
add_executable(pmf-tests tests/identity_tests.cpp)
target_link_libraries(pmf-tests PRIVATE pmf-core)
add_test(NAME pmf-identity COMMAND pmf-tests)
