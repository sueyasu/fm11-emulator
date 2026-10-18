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

    add_executable(mainv20_carry_repeat tests/mainv20_carry_repeat.cpp)
    target_link_libraries(mainv20_carry_repeat PRIVATE fm11_core)
    add_test(NAME mainv20_carry_repeat COMMAND mainv20_carry_repeat)

    add_executable(mainv20_io_strings tests/mainv20_io_strings.cpp)
    target_link_libraries(mainv20_io_strings PRIVATE fm11_core)
    add_test(NAME mainv20_io_strings COMMAND mainv20_io_strings)

    add_executable(mainv20_imul_frame tests/mainv20_imul_frame.cpp)
    target_link_libraries(mainv20_imul_frame PRIVATE fm11_core)
    add_test(NAME mainv20_imul_frame COMMAND mainv20_imul_frame)

    add_executable(mainv20_stack_extensions tests/mainv20_stack_extensions.cpp)
    target_link_libraries(mainv20_stack_extensions PRIVATE fm11_core)
    add_test(NAME mainv20_stack_extensions COMMAND mainv20_stack_extensions)

    add_executable(mainv20_bitfield tests/mainv20_bitfield.cpp)
    target_link_libraries(mainv20_bitfield PRIVATE fm11_core)
    add_test(NAME mainv20_bitfield COMMAND mainv20_bitfield)

    add_executable(mainv20_8080_call_aliases tests/mainv20_8080_call_aliases.cpp)
    target_link_libraries(mainv20_8080_call_aliases PRIVATE fm11_core)
    add_test(NAME mainv20_8080_call_aliases COMMAND mainv20_8080_call_aliases)

    add_executable(mainv20_8080_psw_roundtrip tests/mainv20_8080_psw_roundtrip.cpp)
    target_link_libraries(mainv20_8080_psw_roundtrip PRIVATE fm11_core)
    add_test(NAME mainv20_8080_psw_roundtrip COMMAND mainv20_8080_psw_roundtrip)

    add_executable(mainv20_8080_branch_aliases tests/mainv20_8080_branch_aliases.cpp)
    target_link_libraries(mainv20_8080_branch_aliases PRIVATE fm11_core)
    add_test(NAME mainv20_8080_branch_aliases COMMAND mainv20_8080_branch_aliases)

    add_executable(mainv20_8080_ana_ac tests/mainv20_8080_ana_ac.cpp)
    target_link_libraries(mainv20_8080_ana_ac PRIVATE fm11_core)
    add_test(NAME mainv20_8080_ana_ac COMMAND mainv20_8080_ana_ac)

    add_executable(mainv20_8080_nested_calln tests/mainv20_8080_nested_calln.cpp)
    target_link_libraries(mainv20_8080_nested_calln PRIVATE fm11_core)
    add_test(NAME mainv20_8080_nested_calln COMMAND mainv20_8080_nested_calln)

    add_executable(mainv20_8080_nop_aliases tests/mainv20_8080_nop_aliases.cpp)
    target_link_libraries(mainv20_8080_nop_aliases PRIVATE fm11_core)
    add_test(NAME mainv20_8080_nop_aliases COMMAND mainv20_8080_nop_aliases)

    add_executable(mainv20_8080_retem_flags tests/mainv20_8080_retem_flags.cpp)
    target_link_libraries(mainv20_8080_retem_flags PRIVATE fm11_core)
    add_test(NAME mainv20_8080_retem_flags COMMAND mainv20_8080_retem_flags)

    add_executable(mainv20_8080_auxcarry tests/mainv20_8080_auxcarry.cpp)
    target_link_libraries(mainv20_8080_auxcarry PRIVATE fm11_core)
    add_test(NAME mainv20_8080_auxcarry COMMAND mainv20_8080_auxcarry)

    add_executable(mainv20_8080_calln tests/mainv20_8080_calln.cpp)
    target_link_libraries(mainv20_8080_calln PRIVATE fm11_core)
    add_test(NAME mainv20_8080_calln COMMAND mainv20_8080_calln)

    add_executable(mainv20_8080_irq tests/mainv20_8080_irq.cpp)
    target_link_libraries(mainv20_8080_irq PRIVATE fm11_core)
    add_test(NAME mainv20_8080_irq COMMAND mainv20_8080_irq)

    add_executable(mainv20_8080_daa tests/mainv20_8080_daa.cpp)
    target_link_libraries(mainv20_8080_daa PRIVATE fm11_core)
    add_test(NAME mainv20_8080_daa COMMAND mainv20_8080_daa)

    add_executable(mainv20_8080_misc tests/mainv20_8080_misc.cpp)
    target_link_libraries(mainv20_8080_misc PRIVATE fm11_core)
    add_test(NAME mainv20_8080_misc COMMAND mainv20_8080_misc)

    add_executable(mainv20_8080_io tests/mainv20_8080_io.cpp)
    target_link_libraries(mainv20_8080_io PRIVATE fm11_core)
    add_test(NAME mainv20_8080_io COMMAND mainv20_8080_io)

    add_executable(mainv20_8080_control tests/mainv20_8080_control.cpp)
    target_link_libraries(mainv20_8080_control PRIVATE fm11_core)
    add_test(NAME mainv20_8080_control COMMAND mainv20_8080_control)

    add_executable(mainv20_8080_alu tests/mainv20_8080_alu.cpp)
    target_link_libraries(mainv20_8080_alu PRIVATE fm11_core)
    add_test(NAME mainv20_8080_alu COMMAND mainv20_8080_alu)

    add_executable(mainv20_8080_data tests/mainv20_8080_data.cpp)
    target_link_libraries(mainv20_8080_data PRIVATE fm11_core)
    add_test(NAME mainv20_8080_data COMMAND mainv20_8080_data)

    add_executable(mainv20_8080_transition tests/mainv20_8080_transition.cpp)
    target_link_libraries(mainv20_8080_transition PRIVATE fm11_core)
    add_test(NAME mainv20_8080_transition COMMAND mainv20_8080_transition)

    add_executable(main8088_stack_wrap tests/main8088_stack_wrap.cpp)
    target_link_libraries(main8088_stack_wrap PRIVATE fm11_core)
    add_test(NAME main8088_stack_wrap COMMAND main8088_stack_wrap)

    add_executable(main8088_rotate_carry tests/main8088_rotate_carry.cpp)
    target_link_libraries(main8088_rotate_carry PRIVATE fm11_core)
    add_test(NAME main8088_rotate_carry COMMAND main8088_rotate_carry)

    add_executable(main8088_shift_flags tests/main8088_shift_flags.cpp)
    target_link_libraries(main8088_shift_flags PRIVATE fm11_core)
    add_test(NAME main8088_shift_flags COMMAND main8088_shift_flags)

    add_executable(main8088_carry_flags tests/main8088_carry_flags.cpp)
    target_link_libraries(main8088_carry_flags PRIVATE fm11_core)
    add_test(NAME main8088_carry_flags COMMAND main8088_carry_flags)

    add_executable(main8088_arithmetic_flags tests/main8088_arithmetic_flags.cpp)
    target_link_libraries(main8088_arithmetic_flags PRIVATE fm11_core)
    add_test(NAME main8088_arithmetic_flags COMMAND main8088_arithmetic_flags)

    add_executable(main8088_segment_boundary tests/main8088_segment_boundary.cpp)
    target_link_libraries(main8088_segment_boundary PRIVATE fm11_core)
    add_test(NAME main8088_segment_boundary COMMAND main8088_segment_boundary)

    add_executable(mainv20_bcd_boundaries tests/mainv20_bcd_boundaries.cpp)
    target_link_libraries(mainv20_bcd_boundaries PRIVATE fm11_core)
    add_test(NAME mainv20_bcd_boundaries COMMAND mainv20_bcd_boundaries)

    add_executable(mainv20_bcd_string tests/mainv20_bcd_string.cpp)
    target_link_libraries(mainv20_bcd_string PRIVATE fm11_core)
    add_test(NAME mainv20_bcd_string COMMAND mainv20_bcd_string)

    add_executable(mainv20_nibble_rotate tests/mainv20_nibble_rotate.cpp)
    target_link_libraries(mainv20_nibble_rotate PRIVATE fm11_core)
    add_test(NAME mainv20_nibble_rotate COMMAND mainv20_nibble_rotate)

    add_executable(mainv20_bitops tests/mainv20_bitops.cpp)
    target_link_libraries(mainv20_bitops PRIVATE fm11_core)
    add_test(NAME mainv20_bitops COMMAND mainv20_bitops)

    add_executable(main8088_esc_popcs tests/main8088_esc_popcs.cpp)
    target_link_libraries(main8088_esc_popcs PRIVATE fm11_core)
    add_test(NAME main8088_esc_popcs COMMAND main8088_esc_popcs)

    add_executable(main8088_control_transfer tests/main8088_control_transfer.cpp)
    target_link_libraries(main8088_control_transfer PRIVATE fm11_core)
    add_test(NAME main8088_control_transfer COMMAND main8088_control_transfer)

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
