⟦untrusted:ad193d41⟧cmake_minimum_required(VERSION 3.20)
project(PMF VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(pmf-core STATIC
    src/core/identity.cpp
    src/core/workspace.cpp
    src/core/managed.cpp
    src/core/windows_ui.cpp
)
target_include_directories(pmf-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(pmf-core PUBLIC shell32 ole32 uuid bcrypt)
endif()

add_executable(pmf-loader src/loader/main.cpp)
target_link_libraries(pmf-loader PRIVATE pmf-core)

add_executable(pmf-unloader src/unloader/main.cpp)
target_link_libraries(pmf-unloader PRIVATE pmf-core)

enable_testing()
add_executable(pmf-tests tests/identity_tests.cpp)
target_link_libraries(pmf-tests PRIVATE pmf-core)
add_test(NAME pmf-identity COMMAND pmf-tests)
⟦/untrusted:ad193d41⟧
if(WIN32)
    add_executable(pmf-managed-tests tests/managed_tests.cpp)
    target_link_libraries(pmf-managed-tests PRIVATE pmf-core)
    add_test(NAME pmf-managed-lifecycle COMMAND pmf-managed-tests)
endif()
