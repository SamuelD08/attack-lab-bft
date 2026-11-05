cmake_minimum_required(VERSION 3.20)
project(attack_lab_bft)

set(CMAKE_CXX_STANDARD 20)

add_executable(attack_lab_bft
        src/main.cpp
)

add_compile_options(-Wall -Wextra -Wpedantic)

# Libraries
add_library(attacklab src/kv/kv_store.cpp
        src/protocol/message.cpp
        src/protocol/message.h
        src/crypto/signer.h
        src/crypto/fakeSigner.cpp
        src/crypto/fakeSigner.h
        src/sim/simulator.cpp
        src/sim/deliveryPolicy.h
        src/sim/simPolicy.cpp
        src/sim/simPolicy.h
)
target_include_directories(attacklab PUBLIC src)

# LIBSODIUM
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

# GTest - Test
include(FetchContent)
FetchContent_Declare(GTest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.18.0)
FetchContent_MakeAvailable(GTest)

enable_testing()
add_executable(unit_tests
        tests/test_setup.cpp
        tests/test_kv.cpp
        tests/test_message.cpp
        tests/test_fake_signer.cpp
        tests/test_simulator.cpp
        tests/test_delivery_policy.cpp
        tests/test_node.h
)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main PkgConfig::SODIUM attacklab)
include(GoogleTest)
gtest_discover_tests(unit_tests)

# TOOLS
add_executable(tools tools/hello_sign.cpp)
target_link_libraries(tools PRIVATE PkgConfig::SODIUM)

