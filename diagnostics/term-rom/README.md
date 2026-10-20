# FM-11 display and keyboard isolation ROMs

These are standalone 6809 ROMs for an initial hardware-path baseline.
They are NOT yet the extracted NitrOS-9 /Term routines.

Build from repository root:

    make -C diagnostics/term-rom

Requires lwasm. No CMake test targets are involved.

Output:
  diagnostics/term-rom/build/term-rom/main.bin (4096 bytes)
  diagnostics/term-rom/build/term-rom/sub.bin (20480 bytes)

Launch SDL with the two ROMs and an existing real ANK font ROM:

    ./build/fm11-sdl --main-rom diagnostics/term-rom/build/term-rom/main.bin --rom diagnostics/term-rom/build/term-rom/sub.bin --ank-rom /path/to/ank.bin

Enter c in the SDL monitor to run the main CPU.

Expected text:
  FM-11 DISPLAY / INPUT ISOLATION
  NO KEY SHOULD GENERATE EVENTS

The sub ROM initializes CRTC, clears text RAM, and sends READY.
The main ROM waits for FD05 READY and sends characters using
shared FC80/FC81, with FC80 cleared by the sub CPU for ACK.

Keyboard status can be inspected with:
  fm11> stop
  fm11> m FC90 5

FC90 = last keyboard status byte; FC91 = last key byte;
FC92 = number of received user key events (modulo 256);
FC84 = 77 when the display banner has finished.
The boot-time internal code 0050 is excluded from the count.
FC92 should remain zero if no user key is pressed.

Limitation: this first ROM tests the main keyboard IRQ route,
not the complete /Term input protocol. Actual NitrOS-9 /Term
input/output routines must be transplanted into this harness
in a follow-up stage before judging driver equivalence.
