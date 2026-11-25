cmake_minimum_required(VERSION 3.16)
project(fm11_emulator LANGUAGES CXX)

# Interpreter-heavy workloads are prohibitively slow without optimization.
# Preserve explicit Debug/RelWithDebInfo selections by the caller.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

# Link-time optimization is opt-in so baseline builds remain unchanged.
# CMake IPO sets the appropriate compiler and linker flags for supported
# toolchains, including all the core library and executable targets below.
option(FM11_ENABLE_LTO "Enable link-time optimization (IPO)" OFF)
if(FM11_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT fm11_lto_supported OUTPUT fm11_lto_error
                        LANGUAGES CXX)
    if(NOT fm11_lto_supported)
        message(FATAL_ERROR "FM11_ENABLE_LTO requires IPO support: ${fm11_lto_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

option(FM11_BUILD_TESTS "Build portable-core smoke tests" ON)

add_library(fm11_core STATIC
    src/core/Machine.cpp
    src/core/MachineConfig.cpp
    src/core/Sim09.cpp
    src/core/SimZ80.cpp
    src/boards/MainZ80Board.cpp
    src/boards/Main6809Board.cpp
    src/boards/Main8088Bus.cpp
    src/boards/Main8088Board.cpp
)

target_include_directories(fm11_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boards
)

target_compile_features(fm11_core PUBLIC cxx_std_17)

# Hardware-reference CPU instruction tests: no SDL or test data is needed
# for the executable itself. Feed SingleStepTests V2 data with Python.
add_executable(fm11-8088-singlestep tests/main8088_singlestep.cpp)
target_link_libraries(fm11-8088-singlestep PRIVATE fm11_core)

add_executable(fm11-v20-8080-diag tests/mainv20_8080_diag.cpp)
target_link_libraries(fm11-v20-8080-diag PRIVATE fm11_core)

# Worker lifecycle is portable; CPU/PTM threads are enabled in later stages.
find_package(Threads REQUIRED)
target_link_libraries(fm11_core PUBLIC Threads::Threads)

target_compile_options(fm11_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(UNIX)
    add_executable(fm11-8088-romtrace src/headless/romtrace8088.cpp)
    target_link_libraries(fm11-8088-romtrace PRIVATE fm11_core)
    add_executable(fm11-display-romtrace src/headless/romtrace_display.cpp)
    target_link_libraries(fm11-display-romtrace PRIVATE fm11_core)
    add_executable(fm11-cpm80 src/headless/cpm80.cpp)
    target_link_libraries(fm11-cpm80 PRIVATE fm11_core)
    add_executable(fm11-headless src/headless/main.cpp)
    target_link_libraries(fm11-headless PRIVATE fm11_core)
    add_executable(fm11-inkey-test src/headless/inkey_test.cpp)
    target_link_libraries(fm11-inkey-test PRIVATE fm11_core)
    add_executable(fm11-romboot src/headless/romboot.cpp)
    target_link_libraries(fm11-romboot PRIVATE fm11_core)
endif()

# SDL2 is optional. Headless targets and existing regression tests remain
# buildable on systems without the development package.
option(FM11_BUILD_SDL "Build SDL2 window frontend when SDL2 is available" ON)
if(FM11_BUILD_SDL)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_executable(host_image_sync tests/host_image_sync.cpp)
        target_include_directories(host_image_sync PRIVATE src/frontend)
        target_link_libraries(host_image_sync PRIVATE Threads::Threads)
        if(FM11_BUILD_TESTS)
            add_test(NAME host_image_sync COMMAND host_image_sync)
        endif()
        add_executable(fm11-sdl src/frontend/sdl_main.cpp src/frontend/OpenGlRenderer.cpp)
        target_link_libraries(fm11-sdl PRIVATE fm11_core)
        if(FM11_BUILD_TESTS)
            add_test(NAME sdl_help COMMAND fm11-sdl --help)
        endif()
        if(TARGET SDL2::SDL2)
            target_link_libraries(fm11-sdl PRIVATE SDL2::SDL2)
        else()
            target_include_directories(fm11-sdl PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(fm11-sdl PRIVATE ${SDL2_LIBRARIES})
        endif()
    else()
        message(STATUS "SDL2 not found: fm11-sdl disabled (install libsdl2-dev)")
    endif()
endif()

if(FM11_BUILD_TESTS)
    add_test(NAME mainv20_8080_diag_smoke COMMAND fm11-v20-8080-diag --self-test)
    if(UNIX)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME main8088_singlestep_smoke
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/main8088_singlestep_smoke.py
                         $<TARGET_FILE:fm11-8088-singlestep>)
        add_test(NAME mainv20_singlestep_smoke
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/mainv20_singlestep_smoke.py
                         $<TARGET_FILE:fm11-8088-singlestep>)
        add_test(NAME mainv20_nibble_flag_mask
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/mainv20_nibble_flag_mask.py)
        add_test(NAME main8088_singlestep_flag_mask
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/main8088_singlestep_flag_mask.py)
        add_test(NAME main8088_singlestep_divide_stack
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/main8088_singlestep_divide_stack.py
                         $<TARGET_FILE:fm11-8088-singlestep>)
        add_test(NAME extract_roms
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/extract_roms.py
                         ${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_roms.py)
        add_test(NAME display_romtrace
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_romtrace.py
                         $<TARGET_FILE:fm11-display-romtrace>)
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

    add_executable(machine_serial_timing tests/machine_serial_timing.cpp)
    target_link_libraries(machine_serial_timing PRIVATE fm11_core)
    add_test(NAME machine_serial_timing COMMAND machine_serial_timing)

    add_executable(machine_system_timing tests/machine_system_timing.cpp)
    target_link_libraries(machine_system_timing PRIVATE fm11_core)
    add_test(NAME machine_system_timing COMMAND machine_system_timing)

    add_executable(machine_z80_rom_boot tests/machine_z80_rom_boot.cpp)
    target_link_libraries(machine_z80_rom_boot PRIVATE fm11_core)
    add_test(NAME machine_z80_rom_boot COMMAND machine_z80_rom_boot)
    add_executable(main_ram_capacity tests/main_ram_capacity.cpp)
    target_link_libraries(main_ram_capacity PRIVATE fm11_core)
    add_test(NAME main_ram_capacity COMMAND main_ram_capacity)
    add_executable(machine_dipsw_config tests/machine_dipsw_config.cpp)
    target_link_libraries(machine_dipsw_config PRIVATE fm11_core)
    add_test(NAME machine_dipsw_config COMMAND machine_dipsw_config)
    if(UNIX)
        add_executable(serial_backend tests/serial_backend.cpp)
        target_link_libraries(serial_backend PRIVATE fm11_core)
        add_test(NAME serial_backend COMMAND serial_backend)
    endif()
    add_executable(machine_config tests/machine_config.cpp)
    target_link_libraries(machine_config PRIVATE fm11_core)
    add_test(NAME machine_config COMMAND machine_config)

    add_executable(realtime_ptm tests/realtime_ptm.cpp)
    target_link_libraries(realtime_ptm PRIVATE fm11_core)
    add_test(NAME realtime_ptm COMMAND realtime_ptm)
    add_executable(realtime_display_cpu tests/realtime_display_cpu.cpp)
    target_link_libraries(realtime_display_cpu PRIVATE fm11_core)
    add_test(NAME realtime_display_cpu COMMAND realtime_display_cpu)
    add_executable(parallel_cpu_display tests/parallel_cpu_display.cpp)
    target_link_libraries(parallel_cpu_display PRIVATE fm11_core)
    add_test(NAME parallel_cpu_display COMMAND parallel_cpu_display)

    add_executable(worker_group tests/worker_group.cpp)
    target_include_directories(worker_group PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
    target_link_libraries(worker_group PRIVATE Threads::Threads)
    add_test(NAME worker_group COMMAND worker_group)

    add_executable(cpu_speed_meter tests/cpu_speed_meter.cpp)
    target_include_directories(cpu_speed_meter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend)
    add_test(NAME cpu_speed_meter COMMAND cpu_speed_meter)

    add_executable(sdl_cpu_pacer tests/sdl_cpu_pacer.cpp)
    target_include_directories(sdl_cpu_pacer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend)
    add_test(NAME sdl_cpu_pacer COMMAND sdl_cpu_pacer)

    add_executable(mainz80_cycles tests/mainz80_cycles.cpp)
    target_link_libraries(mainz80_cycles PRIVATE fm11_core)
    add_test(NAME mainz80_cycles COMMAND mainz80_cycles)
    add_executable(mainz80_handoff tests/mainz80_handoff.cpp)
    target_link_libraries(mainz80_handoff PRIVATE fm11_core)
    add_test(NAME mainz80_handoff COMMAND mainz80_handoff)
    add_executable(machine_shared_ram tests/machine_shared_ram.cpp)
    target_link_libraries(machine_shared_ram PRIVATE fm11_core)
    add_test(NAME machine_shared_ram COMMAND machine_shared_ram)
    add_executable(main6809_dma_irq_gating tests/main6809_dma_irq_gating.cpp)
    target_link_libraries(main6809_dma_irq_gating PRIVATE fm11_core)
    add_test(NAME main6809_dma_irq_gating COMMAND main6809_dma_irq_gating)

    add_executable(main6809_dma_address_wrap tests/main6809_dma_address_wrap.cpp)
    target_link_libraries(main6809_dma_address_wrap PRIVATE fm11_core)
    add_test(NAME main6809_dma_address_wrap COMMAND main6809_dma_address_wrap)

    add_executable(main6809_dma_decrement tests/main6809_dma_decrement.cpp)
    target_link_libraries(main6809_dma_decrement PRIVATE fm11_core)
    add_test(NAME main6809_dma_decrement COMMAND main6809_dma_decrement)

    add_executable(main6809_dma_register_advance tests/main6809_dma_register_advance.cpp)
    target_link_libraries(main6809_dma_register_advance PRIVATE fm11_core)
    add_test(NAME main6809_dma_register_advance COMMAND main6809_dma_register_advance)

    add_executable(main6809_dma_multi_ack tests/main6809_dma_multi_ack.cpp)
    target_link_libraries(main6809_dma_multi_ack PRIVATE fm11_core)
    add_test(NAME main6809_dma_multi_ack COMMAND main6809_dma_multi_ack)

    add_executable(main6809_dma_status_ack tests/main6809_dma_status_ack.cpp)
    target_link_libraries(main6809_dma_status_ack PRIVATE fm11_core)
    add_test(NAME main6809_dma_status_ack COMMAND main6809_dma_status_ack)

    add_executable(fdc_command_decode tests/fdc_command_decode.cpp)
    target_link_libraries(fdc_command_decode PRIVATE fm11_core)
    add_test(NAME fdc_command_decode COMMAND fdc_command_decode)
    add_executable(fdc_read_address tests/fdc_read_address.cpp)
    target_link_libraries(fdc_read_address PRIVATE fm11_core)
    add_test(NAME fdc_read_address COMMAND fdc_read_address)
    add_executable(timed_floppy_seek tests/timed_floppy_seek.cpp)
    target_link_libraries(timed_floppy_seek PRIVATE fm11_core)
    add_test(NAME timed_floppy_seek COMMAND timed_floppy_seek)
    add_executable(timed_floppy_rotation tests/timed_floppy_rotation.cpp)
    target_link_libraries(timed_floppy_rotation PRIVATE fm11_core)
    add_test(NAME timed_floppy_rotation COMMAND timed_floppy_rotation)
    add_executable(timed_floppy_rom_status tests/timed_floppy_rom_status.cpp)
    target_link_libraries(timed_floppy_rom_status PRIVATE fm11_core)
    add_test(NAME timed_floppy_rom_status COMMAND timed_floppy_rom_status)
    add_executable(timed_floppy_byte_drq tests/timed_floppy_byte_drq.cpp)
    target_link_libraries(timed_floppy_byte_drq PRIVATE fm11_core)
    add_test(NAME timed_floppy_byte_drq COMMAND timed_floppy_byte_drq)
    add_executable(timed_floppy_write_track tests/timed_floppy_write_track.cpp)
    target_link_libraries(timed_floppy_write_track PRIVATE fm11_core)
    add_test(NAME timed_floppy_write_track COMMAND timed_floppy_write_track)
    add_executable(floppy_multi_sector tests/floppy_multi_sector.cpp)
    target_link_libraries(floppy_multi_sector PRIVATE fm11_core)
    add_test(NAME floppy_multi_sector COMMAND floppy_multi_sector)
    add_executable(fdc_b3_boundaries tests/fdc_b3_boundaries.cpp)
    target_link_libraries(fdc_b3_boundaries PRIVATE fm11_core)
    add_test(NAME fdc_b3_boundaries COMMAND fdc_b3_boundaries)
    add_executable(timed_floppy_force_interrupt tests/timed_floppy_force_interrupt.cpp)
    target_link_libraries(timed_floppy_force_interrupt PRIVATE fm11_core)
    add_test(NAME timed_floppy_force_interrupt COMMAND timed_floppy_force_interrupt)
    add_executable(fast_floppy_force_interrupt tests/fast_floppy_force_interrupt.cpp)
    target_link_libraries(fast_floppy_force_interrupt PRIVATE fm11_core)
    add_test(NAME fast_floppy_force_interrupt COMMAND fast_floppy_force_interrupt)


    add_executable(main8088_cycles tests/main8088_cycles.cpp)
    target_link_libraries(main8088_cycles PRIVATE fm11_core)
    add_test(NAME main8088_cycles COMMAND main8088_cycles)
    add_executable(main8088_ea_timing tests/main8088_ea_timing.cpp)
    target_link_libraries(main8088_ea_timing PRIVATE fm11_core)
    add_test(NAME main8088_ea_timing COMMAND main8088_ea_timing)
    add_executable(main8088_dynamic_timing tests/main8088_dynamic_timing.cpp)
    target_link_libraries(main8088_dynamic_timing PRIVATE fm11_core)
    add_test(NAME main8088_dynamic_timing COMMAND main8088_dynamic_timing)
    add_executable(main8088_jcc_timing_matrix tests/main8088_jcc_timing_matrix.cpp)
    target_link_libraries(main8088_jcc_timing_matrix PRIVATE fm11_core)
    add_test(NAME main8088_jcc_timing_matrix COMMAND main8088_jcc_timing_matrix)
    add_executable(main8088_shift_timing_matrix tests/main8088_shift_timing_matrix.cpp)
    target_link_libraries(main8088_shift_timing_matrix PRIVATE fm11_core)
    add_test(NAME main8088_shift_timing_matrix COMMAND main8088_shift_timing_matrix)
    add_executable(main8088_group_timing tests/main8088_group_timing.cpp)
    target_link_libraries(main8088_group_timing PRIVATE fm11_core)
    add_test(NAME main8088_group_timing COMMAND main8088_group_timing)
    add_executable(main8088_segment_mov_timing_matrix tests/main8088_segment_mov_timing_matrix.cpp)
    target_link_libraries(main8088_segment_mov_timing_matrix PRIVATE fm11_core)
    add_test(NAME main8088_segment_mov_timing_matrix COMMAND main8088_segment_mov_timing_matrix)
    add_executable(main8088_test_xchg_timing_matrix tests/main8088_test_xchg_timing_matrix.cpp)
    target_link_libraries(main8088_test_xchg_timing_matrix PRIVATE fm11_core)
    add_test(NAME main8088_test_xchg_timing_matrix COMMAND main8088_test_xchg_timing_matrix)
    add_executable(main8088_far_return_timing tests/main8088_far_return_timing.cpp)
    target_link_libraries(main8088_far_return_timing PRIVATE fm11_core)
    add_test(NAME main8088_far_return_timing COMMAND main8088_far_return_timing)
    add_executable(main8088_string_timing tests/main8088_string_timing.cpp)
    target_link_libraries(main8088_string_timing PRIVATE fm11_core)
    add_test(NAME main8088_string_timing COMMAND main8088_string_timing)
    add_executable(main8088_rep_zero_prefix_timing tests/main8088_rep_zero_prefix_timing.cpp)
    target_link_libraries(main8088_rep_zero_prefix_timing PRIVATE fm11_core)
    add_test(NAME main8088_rep_zero_prefix_timing COMMAND main8088_rep_zero_prefix_timing)
    add_executable(main8088_alu_group_timing tests/main8088_alu_group_timing.cpp)
    target_link_libraries(main8088_alu_group_timing PRIVATE fm11_core)
    add_test(NAME main8088_alu_group_timing COMMAND main8088_alu_group_timing)
    add_executable(main8088_group1_reference_matrix tests/main8088_group1_reference_matrix.cpp)
    target_link_libraries(main8088_group1_reference_matrix PRIVATE fm11_core)
    add_test(NAME main8088_group1_reference_matrix COMMAND main8088_group1_reference_matrix)
    add_executable(main8088_fixed_timing_matrix tests/main8088_fixed_timing_matrix.cpp)
    target_link_libraries(main8088_fixed_timing_matrix PRIVATE fm11_core)
    add_test(NAME main8088_fixed_timing_matrix COMMAND main8088_fixed_timing_matrix)
    add_executable(main8088_salc_timing tests/main8088_salc_timing.cpp)
    target_link_libraries(main8088_salc_timing PRIVATE fm11_core)
    add_test(NAME main8088_salc_timing COMMAND main8088_salc_timing)
    add_executable(main8088_group3_reference_matrix tests/main8088_group3_reference_matrix.cpp)
    target_link_libraries(main8088_group3_reference_matrix PRIVATE fm11_core)
    add_test(NAME main8088_group3_reference_matrix COMMAND main8088_group3_reference_matrix)
    add_executable(main8088_alu_direction_matrix tests/main8088_alu_direction_matrix.cpp)
    target_link_libraries(main8088_alu_direction_matrix PRIVATE fm11_core)
    add_test(NAME main8088_alu_direction_matrix COMMAND main8088_alu_direction_matrix)
    add_executable(main8088_interrupt_cycles tests/main8088_interrupt_cycles.cpp)
    target_link_libraries(main8088_interrupt_cycles PRIVATE fm11_core)
    add_test(NAME main8088_interrupt_cycles COMMAND main8088_interrupt_cycles)
    add_executable(main8088_mul_timing tests/main8088_mul_timing.cpp)
    target_link_libraries(main8088_mul_timing PRIVATE fm11_core)
    add_test(NAME main8088_mul_timing COMMAND main8088_mul_timing)
    add_executable(main8088_signed_div_timing tests/main8088_signed_div_timing.cpp)
    target_link_libraries(main8088_signed_div_timing PRIVATE fm11_core)
    add_test(NAME main8088_signed_div_timing COMMAND main8088_signed_div_timing)
    add_executable(main8088_missing_timing tests/main8088_missing_timing.cpp)
    target_link_libraries(main8088_missing_timing PRIVATE fm11_core)
    add_test(NAME main8088_missing_timing COMMAND main8088_missing_timing)
    add_executable(main8088_esc_timing tests/main8088_esc_timing.cpp)
    target_link_libraries(main8088_esc_timing PRIVATE fm11_core)
    add_test(NAME main8088_esc_timing COMMAND main8088_esc_timing)

    add_executable(display_kanji_rom tests/display_kanji_rom.cpp)
    target_link_libraries(display_kanji_rom PRIVATE fm11_core)
    add_test(NAME display_kanji_rom COMMAND display_kanji_rom)
    add_executable(display_control tests/display_control.cpp)
    target_link_libraries(display_control PRIVATE fm11_core)
    add_test(NAME display_control COMMAND display_control)

    # This probe requires the user's unchanged original display ROM, so it
    # is deliberately not registered as a hermetic CTest.
    add_executable(display_real_rom_probe tests/display_real_rom_probe.cpp)
    target_link_libraries(display_real_rom_probe PRIVATE fm11_core)

    add_executable(display_binary_real_protocol tests/display_binary_real_protocol.cpp)
    target_link_libraries(display_binary_real_protocol PRIVATE fm11_core)
    add_test(NAME display_binary_real_protocol
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_real_protocol.py
                     $<TARGET_FILE:display_binary_real_protocol>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_text_protocol tests/display_binary_text_protocol.cpp)
    target_link_libraries(display_binary_text_protocol PRIVATE fm11_core)
    add_test(NAME display_binary_text_protocol
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_protocol.py
                     $<TARGET_FILE:display_binary_text_protocol>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_sub_io_irq tests/display_sub_io_irq.cpp)
    target_link_libraries(display_sub_io_irq PRIVATE fm11_core)
    add_test(NAME display_sub_io_irq COMMAND display_sub_io_irq)

    add_executable(display_keyboard_queue tests/display_keyboard_queue.cpp)
    target_link_libraries(display_keyboard_queue PRIVATE fm11_core)
    add_test(NAME display_keyboard_queue COMMAND display_keyboard_queue)

    add_executable(display_io_read_clear tests/display_io_read_clear.cpp)
    target_link_libraries(display_io_read_clear PRIVATE fm11_core)
    add_test(NAME display_io_read_clear COMMAND display_io_read_clear)

    add_executable(display_afe0_handshake tests/display_afe0_handshake.cpp)
    target_link_libraries(display_afe0_handshake PRIVATE fm11_core)
    add_test(NAME display_afe0_handshake COMMAND display_afe0_handshake)

    add_executable(display_binary_text_scroll tests/display_binary_text_scroll.cpp)
    target_link_libraries(display_binary_text_scroll PRIVATE fm11_core)
    foreach(mode IN ITEMS top scroll)
        add_test(NAME display_binary_text_scroll_${mode}
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_scroll.py
                         $<TARGET_FILE:display_binary_text_scroll>
                         ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py
                         ${mode})
    endforeach()

    add_executable(display_binary_layers tests/display_binary_layers.cpp)
    target_link_libraries(display_binary_layers PRIVATE fm11_core)
    foreach(mode IN ITEMS text graphics both)
        add_test(NAME display_binary_layers_${mode}
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_layers.py
                         $<TARGET_FILE:display_binary_layers>
                         ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py
                         ${mode})
    endforeach()

    add_executable(display_binary_text_overlay tests/display_binary_text_overlay.cpp)
    target_link_libraries(display_binary_text_overlay PRIVATE fm11_core)
    add_test(NAME display_binary_text_overlay
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_overlay.py
                     $<TARGET_FILE:display_binary_text_overlay>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_text_attributes tests/display_binary_text_attributes.cpp)
    target_link_libraries(display_binary_text_attributes PRIVATE fm11_core)
    add_test(NAME display_binary_text_attributes
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_attributes.py
                     $<TARGET_FILE:display_binary_text_attributes>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_text_modes tests/display_binary_text_modes.cpp)
    target_link_libraries(display_binary_text_modes PRIVATE fm11_core)
    foreach(columns IN ITEMS 40 80)
        add_test(NAME display_binary_text_${columns}col
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_modes.py
                         $<TARGET_FILE:display_binary_text_modes>
                         ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py
                         ${columns})
    endforeach()

    add_executable(display_binary_text_stage1 tests/display_binary_text_stage1.cpp)
    target_link_libraries(display_binary_text_stage1 PRIVATE fm11_core)
    add_test(NAME display_binary_text_stage1
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_text_stage1.py
                     $<TARGET_FILE:display_binary_text_stage1>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_text_composition tests/display_text_composition.cpp)
    target_link_libraries(display_text_composition PRIVATE fm11_core)
    add_test(NAME display_text_composition COMMAND display_text_composition)

    add_executable(display_binary_raster tests/display_binary_raster.cpp)
    target_link_libraries(display_binary_raster PRIVATE fm11_core)
    add_test(NAME display_binary_raster
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_raster.py
                     $<TARGET_FILE:display_binary_raster>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_pattern tests/display_binary_pattern.cpp)
    target_link_libraries(display_binary_pattern PRIVATE fm11_core)
    add_test(NAME display_binary_pattern
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_pattern.py
                     $<TARGET_FILE:display_binary_pattern>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_afe7 tests/display_binary_afe7.cpp)
    target_link_libraries(display_binary_afe7 PRIVATE fm11_core)
    add_test(NAME display_binary_afe7
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_afe7.py
                     $<TARGET_FILE:display_binary_afe7>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_events tests/display_binary_events.cpp)
    target_link_libraries(display_binary_events PRIVATE fm11_core)
    add_test(NAME display_binary_events
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_events.py
                     $<TARGET_FILE:display_binary_events>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_attention tests/display_binary_attention.cpp)
    target_link_libraries(display_binary_attention PRIVATE fm11_core)
    add_test(NAME display_binary_attention
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_attention.py
                     $<TARGET_FILE:display_binary_attention>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_halt tests/display_binary_halt.cpp)
    target_link_libraries(display_binary_halt PRIVATE fm11_core)
    add_test(NAME display_binary_halt
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_halt.py
                     $<TARGET_FILE:display_binary_halt>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_executable(display_binary_integration tests/display_binary_integration.cpp)
    target_link_libraries(display_binary_integration PRIVATE fm11_core)
    add_test(NAME display_binary_integration
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_integration.py
                     $<TARGET_FILE:display_binary_integration>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py)

    add_test(NAME display_binary_handshake
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/display_binary_integration.py
                     $<TARGET_FILE:display_binary_integration>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_display_test_roms.py
                     --handshake)

    add_executable(display_cpu6809 tests/display_cpu6809.cpp)
    target_link_libraries(display_cpu6809 PRIVATE fm11_core)
    add_test(NAME display_cpu6809 COMMAND display_cpu6809)

    add_executable(machine_fd05_handoff tests/machine_fd05_handoff.cpp)
    target_link_libraries(machine_fd05_handoff PRIVATE fm11_core)
    add_test(NAME machine_fd05_handoff COMMAND machine_fd05_handoff)

    add_executable(machine_ptm_time tests/machine_ptm_time.cpp)
    target_link_libraries(machine_ptm_time PRIVATE fm11_core)
    add_test(NAME machine_ptm_time COMMAND machine_ptm_time)

    add_executable(machine_floppy_shared tests/machine_floppy_shared.cpp)
    target_link_libraries(machine_floppy_shared PRIVATE fm11_core)
    add_test(NAME machine_floppy_shared COMMAND machine_floppy_shared)

    add_executable(main8088_fdd_dma tests/main8088_fdd_dma.cpp)
    target_link_libraries(main8088_fdd_dma PRIVATE fm11_core)
    add_test(NAME main8088_fdd_dma COMMAND main8088_fdd_dma)

    add_executable(main8088_dma_m2m_block tests/main8088_dma_m2m_block.cpp)
    target_link_libraries(main8088_dma_m2m_block PRIVATE fm11_core)
    add_test(NAME main8088_dma_m2m_block COMMAND main8088_dma_m2m_block)

    add_executable(main8088_dma_m2m_other_channels tests/main8088_dma_m2m_other_channels.cpp)
    target_link_libraries(main8088_dma_m2m_other_channels PRIVATE fm11_core)
    add_test(NAME main8088_dma_m2m_other_channels COMMAND main8088_dma_m2m_other_channels)

    add_executable(main8088_dma_memory_to_memory tests/main8088_dma_memory_to_memory.cpp)
    target_link_libraries(main8088_dma_memory_to_memory PRIVATE fm11_core)
    add_test(NAME main8088_dma_memory_to_memory COMMAND main8088_dma_memory_to_memory)

    add_executable(main8088_dma_dreq_polarity tests/main8088_dma_dreq_polarity.cpp)
    target_link_libraries(main8088_dma_dreq_polarity PRIVATE fm11_core)
    add_test(NAME main8088_dma_dreq_polarity COMMAND main8088_dma_dreq_polarity)

    add_executable(main8088_dma_dreq_modes tests/main8088_dma_dreq_modes.cpp)
    target_link_libraries(main8088_dma_dreq_modes PRIVATE fm11_core)
    add_test(NAME main8088_dma_dreq_modes COMMAND main8088_dma_dreq_modes)

    add_executable(main8088_dma_cascade tests/main8088_dma_cascade.cpp)
    target_link_libraries(main8088_dma_cascade PRIVATE fm11_core)
    add_test(NAME main8088_dma_cascade COMMAND main8088_dma_cascade)

    add_executable(main8088_dma_master_clear tests/main8088_dma_master_clear.cpp)
    target_link_libraries(main8088_dma_master_clear PRIVATE fm11_core)
    add_test(NAME main8088_dma_master_clear COMMAND main8088_dma_master_clear)

    add_executable(main8088_dma_priority tests/main8088_dma_priority.cpp)
    target_link_libraries(main8088_dma_priority PRIVATE fm11_core)
    add_test(NAME main8088_dma_priority COMMAND main8088_dma_priority)

    add_executable(main8088_pic_auto_eoi tests/main8088_pic_auto_eoi.cpp)
    target_link_libraries(main8088_pic_auto_eoi PRIVATE fm11_core)
    add_test(NAME main8088_pic_auto_eoi COMMAND main8088_pic_auto_eoi)

    add_executable(main8088_pic_priority tests/main8088_pic_priority.cpp)
    target_link_libraries(main8088_pic_priority PRIVATE fm11_core)
    add_test(NAME main8088_pic_priority COMMAND main8088_pic_priority)





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

    add_executable(mainv20_8080_timing tests/mainv20_8080_timing.cpp)
    target_link_libraries(mainv20_8080_timing PRIVATE fm11_core)
    add_test(NAME mainv20_8080_timing COMMAND mainv20_8080_timing)
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

    add_executable(main8088_carry_stack_boundaries tests/main8088_carry_stack_boundaries.cpp)
    target_link_libraries(main8088_carry_stack_boundaries PRIVATE fm11_core)
    add_test(NAME main8088_carry_stack_boundaries COMMAND main8088_carry_stack_boundaries)
    add_executable(main8088_decimal_adjust tests/main8088_decimal_adjust.cpp)
    target_link_libraries(main8088_decimal_adjust PRIVATE fm11_core)
    add_test(NAME main8088_decimal_adjust COMMAND main8088_decimal_adjust)
    add_executable(main8088_undocumented_jcc tests/main8088_undocumented_jcc.cpp)
    target_link_libraries(main8088_undocumented_jcc PRIVATE fm11_core)
    add_test(NAME main8088_undocumented_jcc COMMAND main8088_undocumented_jcc)
    add_executable(main8088_segment_mov_aliases tests/main8088_segment_mov_aliases.cpp)
    target_link_libraries(main8088_segment_mov_aliases PRIVATE fm11_core)
    add_test(NAME main8088_segment_mov_aliases COMMAND main8088_segment_mov_aliases)
    add_executable(main8088_undocumented_ret tests/main8088_undocumented_ret.cpp)
    target_link_libraries(main8088_undocumented_ret PRIVATE fm11_core)
    add_test(NAME main8088_undocumented_ret COMMAND main8088_undocumented_ret)
    add_executable(main8088_mov_imm_aliases tests/main8088_mov_imm_aliases.cpp)
    target_link_libraries(main8088_mov_imm_aliases PRIVATE fm11_core)
    add_test(NAME main8088_mov_imm_aliases COMMAND main8088_mov_imm_aliases)
    add_executable(main8088_singlestep_io tests/main8088_singlestep_io.cpp)
    target_link_libraries(main8088_singlestep_io PRIVATE fm11_core)
    add_test(NAME main8088_singlestep_io COMMAND main8088_singlestep_io)
    add_executable(main8088_divide_quirks tests/main8088_divide_quirks.cpp)
    target_link_libraries(main8088_divide_quirks PRIVATE fm11_core)
    add_test(NAME main8088_divide_quirks COMMAND main8088_divide_quirks)
    add_executable(main8088_ff_push_alias tests/main8088_ff_push_alias.cpp)
    target_link_libraries(main8088_ff_push_alias PRIVATE fm11_core)
    add_test(NAME main8088_ff_push_alias COMMAND main8088_ff_push_alias)
    add_executable(mainv20_test1 tests/mainv20_test1.cpp)
    target_link_libraries(mainv20_test1 PRIVATE fm11_core)
    add_test(NAME mainv20_test1 COMMAND mainv20_test1)
    add_executable(mainv20_decode_aliases tests/mainv20_decode_aliases.cpp)
    target_link_libraries(mainv20_decode_aliases PRIVATE fm11_core)
    add_test(NAME mainv20_decode_aliases COMMAND mainv20_decode_aliases)
    add_executable(mainv20_extended_arithmetic tests/mainv20_extended_arithmetic.cpp)
    target_link_libraries(mainv20_extended_arithmetic PRIVATE fm11_core)
    add_test(NAME mainv20_extended_arithmetic COMMAND mainv20_extended_arithmetic)
    add_executable(mainv20_immediate_shift tests/mainv20_immediate_shift.cpp)
    target_link_libraries(mainv20_immediate_shift PRIVATE fm11_core)
    add_test(NAME mainv20_immediate_shift COMMAND mainv20_immediate_shift)
    add_executable(mainv20_rep_prefixes tests/mainv20_rep_prefixes.cpp)
    target_link_libraries(mainv20_rep_prefixes PRIVATE fm11_core)
    add_test(NAME mainv20_rep_prefixes COMMAND mainv20_rep_prefixes)
    add_executable(mainv20_bound tests/mainv20_bound.cpp)
    target_link_libraries(mainv20_bound PRIVATE fm11_core)
    add_test(NAME mainv20_bound COMMAND mainv20_bound)
    add_executable(main8088_ipl_handoff tests/main8088_ipl_handoff.cpp)
    target_link_libraries(main8088_ipl_handoff PRIVATE fm11_core)
    add_test(NAME main8088_ipl_handoff COMMAND main8088_ipl_handoff)
    add_executable(main8088_bs_shared_devices tests/main8088_bs_shared_devices.cpp)
    target_link_libraries(main8088_bs_shared_devices PRIVATE fm11_core)
    add_test(NAME main8088_bs_shared_devices COMMAND main8088_bs_shared_devices)
    add_executable(mainv20_ins_ax_alias tests/mainv20_ins_ax_alias.cpp)
    target_link_libraries(mainv20_ins_ax_alias PRIVATE fm11_core)
    add_test(NAME mainv20_ins_ax_alias COMMAND mainv20_ins_ax_alias)
    add_executable(main8088_undocumented_far_ret tests/main8088_undocumented_far_ret.cpp)
    target_link_libraries(main8088_undocumented_far_ret PRIVATE fm11_core)
    add_test(NAME main8088_undocumented_far_ret COMMAND main8088_undocumented_far_ret)

    add_executable(main8088_arithmetic_flags tests/main8088_arithmetic_flags.cpp)
    target_link_libraries(main8088_arithmetic_flags PRIVATE fm11_core)
    add_test(NAME main8088_arithmetic_flags COMMAND main8088_arithmetic_flags)

    add_executable(main8088_segment_boundary tests/main8088_segment_boundary.cpp)
    target_link_libraries(main8088_segment_boundary PRIVATE fm11_core)
    add_test(NAME main8088_segment_boundary COMMAND main8088_segment_boundary)

    add_executable(mainv20_extension_timing tests/mainv20_extension_timing.cpp)
    target_link_libraries(mainv20_extension_timing PRIVATE fm11_core)
    add_test(NAME mainv20_extension_timing COMMAND mainv20_extension_timing)
    add_executable(mainv20_native_timing tests/mainv20_native_timing.cpp)
    target_link_libraries(mainv20_native_timing PRIVATE fm11_core)
    add_test(NAME mainv20_native_timing COMMAND mainv20_native_timing)
    add_executable(mainv20_bcd_boundaries tests/mainv20_bcd_boundaries.cpp)
    target_link_libraries(mainv20_bcd_boundaries PRIVATE fm11_core)
    add_test(NAME mainv20_bcd_boundaries COMMAND mainv20_bcd_boundaries)


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

    # CP/M-86 CBIOS: CMP BX,80 / JAE track-range regression.
    add_executable(main8088_cmp_jae tests/main8088_cmp_jae.cpp)
    target_link_libraries(main8088_cmp_jae PRIVATE fm11_core)
    add_test(NAME main8088_cmp_jae COMMAND main8088_cmp_jae)


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

    add_executable(usart8251_clocked tests/usart8251_clocked.cpp)
    target_link_libraries(usart8251_clocked PRIVATE fm11_core)
    add_test(NAME usart8251_clocked COMMAND usart8251_clocked)
    add_executable(usart8251_registers tests/usart8251_registers.cpp)
    target_link_libraries(usart8251_registers PRIVATE fm11_core)
    add_test(NAME usart8251_registers COMMAND usart8251_registers)
    add_executable(main6809_serial_console tests/main6809_serial_console.cpp)
    target_link_libraries(main6809_serial_console PRIVATE fm11_core)
    add_test(NAME main6809_serial_console COMMAND main6809_serial_console)

    add_executable(main6809_floppy_read tests/main6809_floppy_read.cpp)
    target_link_libraries(main6809_floppy_read PRIVATE fm11_core)
    add_test(NAME main6809_floppy_read COMMAND main6809_floppy_read)

    add_executable(main6809_floppy_write tests/main6809_floppy_write.cpp)
    target_link_libraries(main6809_floppy_write PRIVATE fm11_core)
    add_test(NAME main6809_floppy_write COMMAND main6809_floppy_write)

    add_executable(mdc_cpu_dma tests/mdc_cpu_dma.cpp)
    target_link_libraries(mdc_cpu_dma PRIVATE fm11_core)
    add_test(NAME mdc_cpu_dma COMMAND mdc_cpu_dma)

    add_executable(mdc_hdd tests/mdc_hdd.cpp)
    target_link_libraries(mdc_hdd PRIVATE fm11_core)
    add_test(NAME mdc_hdd COMMAND mdc_hdd)
    add_executable(mdc_command_state tests/mdc_command_state.cpp)
    target_link_libraries(mdc_command_state PRIVATE fm11_core)
    add_test(NAME mdc_command_state COMMAND mdc_command_state)
    add_executable(mdc_timed_dma tests/mdc_timed_dma.cpp)
    target_link_libraries(mdc_timed_dma PRIVATE fm11_core)
    add_test(NAME mdc_timed_dma COMMAND mdc_timed_dma)
    add_executable(beeper_device tests/beeper_device.cpp)
    target_link_libraries(beeper_device PRIVATE fm11_core)
    add_test(NAME beeper_device COMMAND beeper_device)
    add_executable(mdc_b4_errors tests/mdc_b4_errors.cpp)
    target_link_libraries(mdc_b4_errors PRIVATE fm11_core)
    add_test(NAME mdc_b4_errors COMMAND mdc_b4_errors)

    add_executable(floppy_four_drives tests/floppy_four_drives.cpp)
    target_link_libraries(floppy_four_drives PRIVATE fm11_core)
    add_test(NAME floppy_four_drives COMMAND floppy_four_drives)
    add_executable(floppy_write_track tests/floppy_write_track.cpp)
    target_link_libraries(floppy_write_track PRIVATE fm11_core)
    add_test(NAME floppy_write_track COMMAND floppy_write_track)

    add_executable(main6809_floppy_d88 tests/main6809_floppy_d88.cpp)
    target_link_libraries(main6809_floppy_d88 PRIVATE fm11_core)
    add_test(NAME main6809_floppy_d88 COMMAND main6809_floppy_d88)
    # Stage 1-D: Z80 instruction fetch, prefixes, block and interrupt timing.
    add_executable(mainz80_opcode_timing_coverage tests/mainz80_opcode_timing_coverage.cpp)
    target_link_libraries(mainz80_opcode_timing_coverage PRIVATE fm11_core)
    add_test(NAME mainz80_opcode_timing_coverage COMMAND mainz80_opcode_timing_coverage)

    add_executable(mainz80_timing_boundaries tests/mainz80_timing_boundaries.cpp)
    target_link_libraries(mainz80_timing_boundaries PRIVATE fm11_core)
    add_test(NAME mainz80_timing_boundaries COMMAND mainz80_timing_boundaries)

    # Stage 1-A: portable reference/status-aware CPU cycle regression.
    add_executable(cpu_timing_matrix tests/cpu_timing_matrix.cpp)
    target_link_libraries(cpu_timing_matrix PRIVATE fm11_core)
    add_test(NAME cpu_timing_matrix COMMAND cpu_timing_matrix
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/cpu_timing_cases.csv)

    # Stage 1-B: complete Motorola Appendix C opcode map and Appendix F
    # indexed-postbyte legality/cycle audit for the MC6809 personality.
    add_test(NAME mc6809_opcode_coverage
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mc6809_opcode_coverage.py)
    add_executable(cpu6809_indexed_postbytes tests/cpu6809_indexed_postbytes.cpp)
    target_link_libraries(cpu6809_indexed_postbytes PRIVATE fm11_core)
    add_test(NAME cpu6809_indexed_postbytes COMMAND cpu6809_indexed_postbytes)

    # Stage 1-C: HD6309 emulation/native reference cycle regression.
    add_test(NAME hd6309_opcode_coverage
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_hd6309_opcode_coverage.py)

    add_executable(cpu6309_math_results tests/cpu6309_math_results.cpp)
    target_link_libraries(cpu6309_math_results PRIVATE fm11_core)
    add_test(NAME cpu6309_math_results COMMAND cpu6309_math_results)

    add_executable(cpu6309_tfm_interrupt tests/cpu6309_tfm_interrupt.cpp)
    target_link_libraries(cpu6309_tfm_interrupt PRIVATE fm11_core)
    add_test(NAME cpu6309_tfm_interrupt COMMAND cpu6309_tfm_interrupt)

    add_executable(cpu6309_stack_masks tests/cpu6309_stack_masks.cpp)
    target_link_libraries(cpu6309_stack_masks PRIVATE fm11_core)
    add_test(NAME cpu6309_stack_masks COMMAND cpu6309_stack_masks)

    add_executable(cpu6309_math_variable_cycles tests/cpu6309_math_variable_cycles.cpp)
    target_link_libraries(cpu6309_math_variable_cycles PRIVATE fm11_core)
    add_test(NAME cpu6309_math_variable_cycles COMMAND cpu6309_math_variable_cycles)

    add_executable(cpu6309_cycle_reference tests/cpu6309_cycle_reference.cpp)
    target_link_libraries(cpu6309_cycle_reference PRIVATE fm11_core)
    add_test(NAME cpu6309_cycle_reference COMMAND cpu6309_cycle_reference)

    add_executable(cpu6309_indexed_cycles tests/cpu6309_indexed_cycles.cpp)
    target_link_libraries(cpu6309_indexed_cycles PRIVATE fm11_core)
    add_test(NAME cpu6309_indexed_cycles COMMAND cpu6309_indexed_cycles)

    add_executable(cpu6809_opcode_cycles tests/cpu6809_opcode_cycles.cpp)
    target_link_libraries(cpu6809_opcode_cycles PRIVATE fm11_core)
    add_test(NAME cpu6809_opcode_cycles COMMAND cpu6809_opcode_cycles
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mc6809_reference.csv)

    # Keep completed CPU-component regressions available without requiring
    # all of them during ongoing display/boot subsystem development.
    # The full suite remains accessible with plain 'ctest'.
    get_property(fm11_registered_tests DIRECTORY PROPERTY TESTS)
    foreach(fm11_test IN LISTS fm11_registered_tests)
        if(fm11_test MATCHES "^(display_|machine_|main6809_|sdl_help$)")
            set_tests_properties(${fm11_test} PROPERTIES LABELS "boot")
        else()
            set_tests_properties(${fm11_test} PROPERTIES LABELS "extended")
        endif()
    endforeach()
endif()
