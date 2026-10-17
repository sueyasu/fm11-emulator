cmake_minimum_required(VERSION 3.16)
project(fm11_emulator LANGUAGES CXX)

option(FM11_BUILD_TESTS "Build portable-core smoke tests" ON)

add_library(fm11_core STATIC
    src/core/Sim09.cpp
    src/core/SimZ80.cpp
)

target_include_directories(fm11_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

target_compile_features(fm11_core PUBLIC cxx_std_17)

target_compile_options(fm11_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(FM11_BUILD_TESTS)
    enable_testing()
    add_executable(core_smoke tests/core_smoke.cpp)
    target_link_libraries(core_smoke PRIVATE fm11_core)
    add_test(NAME core_smoke COMMAND core_smoke)
endif()
