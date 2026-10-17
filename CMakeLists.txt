cmake_minimum_required(VERSION 3.16)
project(fm11_emulator LANGUAGES CXX)

option(FM11_BUILD_TESTS "Build portable-core smoke tests" ON)

add_library(fm11_core STATIC
    src/core/Machine.cpp
    src/core/Sim09.cpp
    src/core/SimZ80.cpp
    src/boards/Main6809Board.cpp
)

target_include_directories(fm11_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boards
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

    add_executable(machine_smoke tests/machine_smoke.cpp)
    target_link_libraries(machine_smoke PRIVATE fm11_core)
    add_test(NAME machine_smoke COMMAND machine_smoke)

    add_executable(cpu6809_personality tests/6809_personality.cpp)
    target_link_libraries(cpu6809_personality PRIVATE fm11_core)
    add_test(NAME cpu6809_personality COMMAND cpu6809_personality)

    add_executable(main6809_memory tests/main6809_memory.cpp)
    target_link_libraries(main6809_memory PRIVATE fm11_core)
    add_test(NAME main6809_memory COMMAND main6809_memory)

    add_executable(main6809_mmr tests/main6809_mmr.cpp)
    target_link_libraries(main6809_mmr PRIVATE fm11_core)
    add_test(NAME main6809_mmr COMMAND main6809_mmr)

    add_executable(main6809_map tests/main6809_map.cpp)
    target_link_libraries(main6809_map PRIVATE fm11_core)
    add_test(NAME main6809_map COMMAND main6809_map)

    add_executable(main6809_system_io tests/main6809_system_io.cpp)
    target_link_libraries(main6809_system_io PRIVATE fm11_core)
    add_test(NAME main6809_system_io COMMAND main6809_system_io)

    add_executable(main6809_irq tests/main6809_irq.cpp)
    target_link_libraries(main6809_irq PRIVATE fm11_core)
    add_test(NAME main6809_irq COMMAND main6809_irq)

    add_executable(main6809_irq_delivery tests/main6809_irq_delivery.cpp)
    target_link_libraries(main6809_irq_delivery PRIVATE fm11_core)
    add_test(NAME main6809_irq_delivery COMMAND main6809_irq_delivery)

    add_executable(main6809_ptm tests/main6809_ptm.cpp)
    target_link_libraries(main6809_ptm PRIVATE fm11_core)
    add_test(NAME main6809_ptm COMMAND main6809_ptm)

    add_executable(main6809_ptm_clock tests/main6809_ptm_clock.cpp)
    target_link_libraries(main6809_ptm_clock PRIVATE fm11_core)
    add_test(NAME main6809_ptm_clock COMMAND main6809_ptm_clock)

    add_executable(main6809_cpu_timing tests/main6809_cpu_timing.cpp)
    target_link_libraries(main6809_cpu_timing PRIVATE fm11_core)
    add_test(NAME main6809_cpu_timing COMMAND main6809_cpu_timing)

    add_executable(main6809_immediate_timing tests/main6809_immediate_timing.cpp)
    target_link_libraries(main6809_immediate_timing PRIVATE fm11_core)
    add_test(NAME main6809_immediate_timing COMMAND main6809_immediate_timing)

    add_executable(main6809_address_timing tests/main6809_address_timing.cpp)
    target_link_libraries(main6809_address_timing PRIVATE fm11_core)
    add_test(NAME main6809_address_timing COMMAND main6809_address_timing)
endif()
