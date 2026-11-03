cmake_minimum_required(VERSION 3.20)
project(attack_lab_bft)

set(CMAKE_CXX_STANDARD 20)

add_executable(attack_lab_bft main.cpp
)

add_compile_options(-Wall -Wextra -Wpedantic)

# LIBSODIUM
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

# GTest - Test
include(FetchContent)
FetchContent_Declare(GTest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.18.0)
FetchContent_MakeAvailable(GTest)

enable_testing()
add_executable(unit_tests tests/test_setup.cpp)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main PkgConfig::SODIUM)
include(GoogleTest)
gtest_discover_tests(unit_tests)

# TOOLS
add_executable(tools tools/hello_sign.cpp)
target_link_libraries(tools PRIVATE PkgConfig::SODIUM)

