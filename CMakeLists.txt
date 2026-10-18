cmake_minimum_required(VERSION 3.16)
project(fm11_emulator LANGUAGES CXX)

option(FM11_BUILD_TESTS "Build portable-core smoke tests" ON)

add_library(fm11_core STATIC
    src/core/Machine.cpp
    src/core/Sim09.cpp
    src/core/SimZ80.cpp
    src/boards/Main6809Board.cpp
    src/boards/Main8088Bus.cpp
    src/boards/Main8088Board.cpp
)

target_include_directories(fm11_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boards
)

target_compile_features(fm11_core PUBLIC cxx_std_17)

target_compile_options(fm11_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(UNIX)
    add_executable(fm11-8088-romtrace src/headless/romtrace8088.cpp)
    target_link_libraries(fm11-8088-romtrace PRIVATE fm11_core)
    add_executable(fm11-headless src/headless/main.cpp)
    target_link_libraries(fm11-headless PRIVATE fm11_core)
endif()

if(FM11_BUILD_TESTS)
    if(UNIX)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME headless_pty_monitor
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/headless_pty_monitor.py
                         $<TARGET_FILE:fm11-headless>)
        add_test(NAME headless_boottrack
                 COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/headless_boottrack.sh
                         $<TARGET_FILE:fm11-headless>)
    endif()
    enable_testing()

    add_test(NAME main8088_opcode_inventory
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/main8088_opcode_inventory.py
                     ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/Main8088Board.cpp)

    add_executable(main8088_cpu tests/main8088_cpu.cpp)
    target_link_libraries(main8088_cpu PRIVATE fm11_core)
    add_test(NAME main8088_cpu COMMAND main8088_cpu)

    add_executable(main8088_bus tests/main8088_bus.cpp)
    target_link_libraries(main8088_bus PRIVATE fm11_core)
    add_test(NAME main8088_bus COMMAND main8088_bus)

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

    add_executable(main6809_branch_timing tests/main6809_branch_timing.cpp)
    target_link_libraries(main6809_branch_timing PRIVATE fm11_core)
    add_test(NAME main6809_branch_timing COMMAND main6809_branch_timing)

    add_executable(main6809_stack_timing tests/main6809_stack_timing.cpp)
    target_link_libraries(main6809_stack_timing PRIVATE fm11_core)
    add_test(NAME main6809_stack_timing COMMAND main6809_stack_timing)

    add_executable(main6809_interrupt_timing tests/main6809_interrupt_timing.cpp)
    target_link_libraries(main6809_interrupt_timing PRIVATE fm11_core)
    add_test(NAME main6809_interrupt_timing COMMAND main6809_interrupt_timing)

    add_executable(main6809_firq_nmi_timing tests/main6809_firq_nmi_timing.cpp)
    target_link_libraries(main6809_firq_nmi_timing PRIVATE fm11_core)
    add_test(NAME main6809_firq_nmi_timing COMMAND main6809_firq_nmi_timing)

    add_executable(cpu6309_interrupt_timing tests/cpu6309_interrupt_timing.cpp)
    target_link_libraries(cpu6309_interrupt_timing PRIVATE fm11_core)
    add_test(NAME cpu6309_interrupt_timing COMMAND cpu6309_interrupt_timing)

    add_executable(cpu6809_full_cycle_db tests/cpu6809_full_cycle_db.cpp)
    target_link_libraries(cpu6809_full_cycle_db PRIVATE fm11_core)
    add_test(NAME cpu6809_full_cycle_db COMMAND cpu6809_full_cycle_db)

    add_executable(main6809_serial_console tests/main6809_serial_console.cpp)
    target_link_libraries(main6809_serial_console PRIVATE fm11_core)
    add_test(NAME main6809_serial_console COMMAND main6809_serial_console)

    add_executable(main6809_floppy_read tests/main6809_floppy_read.cpp)
    target_link_libraries(main6809_floppy_read PRIVATE fm11_core)
    add_test(NAME main6809_floppy_read COMMAND main6809_floppy_read)

    add_executable(main6809_floppy_write tests/main6809_floppy_write.cpp)
    target_link_libraries(main6809_floppy_write PRIVATE fm11_core)
    add_test(NAME main6809_floppy_write COMMAND main6809_floppy_write)

    add_executable(main6809_floppy_d88 tests/main6809_floppy_d88.cpp)
    target_link_libraries(main6809_floppy_d88 PRIVATE fm11_core)
    add_test(NAME main6809_floppy_d88 COMMAND main6809_floppy_d88)
endif()
