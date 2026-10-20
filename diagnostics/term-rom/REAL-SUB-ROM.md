# Pure /Term protocol test using the genuine FM-11 display ROM

This is the replacement for the synthetic keyboard-display test.
It does not use the diagnostic sub.asm. The **original FM-11 display
ROM** runs in the sub CPU, while a minimal 6809 main ROM reproduces
the NitrOS-9 fm11console.asm hardware transactions.

Build ONLY this test:

    make -C diagnostics/term-rom real-sub

Run the existing SDL emulator:

    ./build/fm11-sdl --main-rom diagnostics/term-rom/build/term-rom/main-real-sub.bin --rom external/roms/display6809.bin --ank-rom /path/to/ank.bin

The display ROM path must refer to the genuine FM-11 ROM. The ANK
ROM must be the real font, too. If display6809.bin is not at the
example path, substitute the real file. At the monitor enter c to run.

The diagnostic main ROM uses the exact driver-level control path:
  * Poll FD05 bit 7 for READY.
  * Write 80 to FD05 and await HALT acknowledgement.
  * Clear FC80, FC81.
  * Write command 12 (output) or 2D (INKEY 2) at FC82.
  * Output character at FC83, or clear FC83/FC84 for input.
  * Release HALT by writing 00 to FD05.
  * Await the next READY status before examining FC83/FC84.

Unlike the OS driver, the diagnostic *does not report an input character*
when FC83=00 and FC84=00. It nevertheless records every raw answer, so
we can determine whether the pure ROM+emulator generates phantom keys
or whether a no-key response is misinterpreted by the OS driver.

The test prints a banner and TEST> prompt, then echoes printable ASCII
and handles Enter and Backspace. No disks, SCF or OS-9 are required.

Use the emulator monitor to pause and inspect:

    fm11> stop
    fm11> m FC90 10
    fm11> m FCA0 64

Memory diagnostics:
  FC90 command number (12 or 2D)
  FC91 last FD05 status
  FC92 FC83 returned by INKEY 2
  FC93 FC84 returned by INKEY 2
  FC94 completed input polls (low 8 bits, wraps)
  FC95 input polls with nonzero ASCII result (low 8 bits, wraps)
  FC96 protocol failure phase (0 = none, 1 READY, 2 HALT, 3 DONE)
  FC97 next ring slot
  FCA0..FCDF 16 entries of 4 bytes (FC83, FC84, FD05, poll-count)

NOTE: This program has not yet been assembled or executed in the
developer environment; the initial result must be treated as a hardware
test outcome, not a proven fix.
