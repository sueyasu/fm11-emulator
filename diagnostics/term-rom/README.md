# FM-11 keyboard/display diagnostic ROM

Build from repository root (requires lwasm):

    make -C diagnostics/term-rom

ROMs:

    diagnostics/term-rom/build/term-rom/main.bin
    diagnostics/term-rom/build/term-rom/sub.bin

Run with the existing SDL emulator and a real ANK ROM:

    ./build/fm11-sdl --main-rom diagnostics/term-rom/build/term-rom/main.bin --rom diagnostics/term-rom/build/term-rom/sub.bin --ank-rom /path/to/ank.bin

Enter c in the SDL monitor to start. Type printable ASCII into the SDL
display window. The diagnostic ROM echoes characters immediately. Enter
moves to the next row and prints TEST>; Backspace erases one character.
This is a minimal shell-like echo, NOT the extracted NitrOS-9 /Term driver.

Use the monitor to pause and inspect guest-side diagnostics:

    fm11> stop
    fm11> m FC90 5
    fm11> m FCA0 64

Memory layout:

* FC84: 77 when startup banner/prompt finished.
* FC90: last raw keyboard event status (FD00).
* FC91: last raw keyboard event data (FD01).
* FC92: count of accepted user events (low byte, wraps).
* FC93: count of ALL raw keyboard notifications (low byte, wraps).
* FC94: next trace ring slot, 0 through 31.
* FCA0..FCDF: 32 two-byte ring entries (status, data).

The trace is in emulated memory, not a host-side log file. Compare the
event count before/after typing a short string. With no keyboard activity
the raw count should not continuously rise. The trace deliberately retains
nonprintable and internal event codes before input filtering.

The startup token 0050 and host 'P' share the same simple 16-bit code in
this minimal pipeline; this ROM currently filters 0050 from display and
the accepted count, so uppercase P will not echo. This is a diagnostic
limitation to resolve when integrating the actual /Term input routine.

Important: current ROM tests the main-CPU FD02/FD03/FD00/FD01 keyboard
notification path, and a deterministic shared RAM display mailbox. It
does not yet reproduce the full OS-9 /Term driver IRQ and I/O behavior.
