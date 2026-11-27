## Redistributable FM-11 8088/V20 compatible boot ROM (initial version)

`roms/compatible/boot8088.asm` implements an independent, 8086-instruction-
set, **4 KiB** reset ROM for the FM-11 8088 CPU card and NEC V20 in native
8086 mode. No original Fujitsu ROM binary is embedded. The source is
assembled with NASM (automatically when NASM is installed):

```sh
cmake -S . -B build
cmake --build build --target fm11-compatible-boot8088
# output: build/roms/compatible/boot8088.bin
```

The ROM reads FD23 boot-device DIP bits (active-low), attempts the enabled
devices in priority order **2D FDD0 -> 2HD FDD0 -> MDC HDD0**, and
transfers the corresponding 512-byte disk IPL to physical `07C00h` using
the FM-11 controllers and 8237 DMA. It then jumps to `07C0:0000`,
the entry point required by the CP/M-86 and CP/M-80 8088/V20 IPLs:

- 2D: T0/H0/S1-S2 (2 x 256 bytes, 8237 channel 0)
- 2HD: T0/H0/S1-S4 (4 x 128 bytes, channel 1)
- HDD: LBA0 and LBA1 (2 x 256 bytes, channel 2, MDC binary-LBA mode)

At reset the ROM also enables the **main-board text display** through
`FDA0=03h`. This is essential even when the display sub-CPU successfully
processes commands: the SDL renderer otherwise suppresses the ANK text layer.
The `compatible_boot8088` test checks this initialization explicitly.

No memory test, bubble-memory boot, or Fujitsu firmware calls are included.
Any enabled boot source that fails is skipped in favor of the next enabled
source; if none succeeds, the CPU halts. The ROM does not implement a
full Fujitsu firmware interface, and should be used only with an IPL that
does not expect such services.

A CLI override can select the ROM without changing the user's config file:

```sh
./build/fm11-sdl --8088-rom build/roms/compatible/boot8088.bin \
    --config /path/to/8088-fm11emu.conf
# or headless:
./build/fm11-sdl --no-gui --8088-rom build/roms/compatible/boot8088.bin \
    --config /path/to/8088-fm11emu.conf
```

Set `[DIPSW] main_cpu = 8088` and install the 8088 main card.
V20 users may set `[main_8088] cpu = v20`. A display sub-CPU ROM is
still needed for the normal GUI path; this ROM only replaces the
**main CPU's reset firmware**.

When NASM is installed, the automated CTest
`compatible_boot8088` runs the generated ROM as real 8088 and V20
instructions against synthetic 2D, native FM-11 2HD D88, and MDC HDD
media; it also tests 2D-absent/2HD-present fallback. The test checks
the complete 512-byte IPL image and reset-vector far jump. The real
CP/M-86 and CP/M-80 disk images should additionally be smoke-tested
manually; booting those OSs is not claimed until confirmed.

```sh
ctest --test-dir build -R '^compatible_boot8088$' --output-on-failure
```

## fm11-sdl --no-gui: full-system headless execution

The normal `fm11-sdl` frontend now accepts `--no-gui`. This uses the
**same** hardware initialization, CPU execution worker, display-sub-CPU
worker, real-time PTM, serial backend, FDD/HDD images, host image
write-through and final sync as the graphical mode. It skips all SDL
initialization, SDL input/event polling, GL/software rendering, Dear ImGui,
file picker and audio output. The FM-11 display CPU still executes, even
though its framebuffer is not shown.

Example:

```sh
./build/fm11-sdl --no-gui --config fm11emu.conf --speed 1
./build/fm11-sdl --no-gui --monitor --config fm11emu.conf
```

Serial endpoints remain configured through `[serial] serial0 = pty`,
`none` or `/dev/ttyUSB0`. The selected PTY name appears on stdout.
When `--monitor` is enabled, its existing Stage 3 monitor uses stdin;
the guest serial console is independent. With no monitor, stdin is not
consumed. Ctrl+C (SIGINT) and SIGTERM stop workers and invoke the same
host-image final sync used by GUI shutdown. No desktop session,
DISPLAY/WAYLAND_DISPLAY, or sound device is required.

`--run-for-ms N` is a headless-only finite-run option intended primarily
for integration testing. Without it, the emulator continues until a
termination signal or the monitor requests a stop. Graphical tests
(`--test-pattern`, `--test-kanji` and `--beep-test`) cannot run under
`--no-gui` and are rejected explicitly.

The old `fm11-headless` remains a separate diagnostic runner and is
unchanged. The new `fm11-sdl --no-gui` is the fully configured FM-11
runtime. The binary is still linked against SDL2, but **does not
initialize SDL** in this mode; SDL2 build/runtime libraries are still
required. A completely SDL-free executable can be built later if needed.

Run the new automated integration test (synthetic main/display ROMs,
unavailable SDL video driver, timed shutdown, monitor, SIGTERM) and all
existing tests:

```sh
cmake --build build -j$(nproc)
ctest --test-dir build -R '^sdl_no_gui_integration$' --output-on-failure
ctest --test-dir build --output-on-failure
```

For manual boot tests, use the same proven NitrOS-9 L1/L2, CP/M-86 and
CP/M-80 config+disk images as the GUI mode; connect the guest serial PTY
with a terminal application. Perform a file write and clean shutdown to
verify persistence. Headless mode intentionally does not emulate a host
viewer for the guest's local CRT or a keyboard input source.

## Configurable main RAM (128 KiB to 1 MiB)

The SDL frontend accepts an optional `[memory]` section in
`fm11emu.conf`:

```ini
[memory]
ram_kib = 512
```

`ram_kib` accepts **128, 256, 384, 512, 640, 768, 896, or 1024**.
The default (if omitted) is **1024 KiB**, retaining the previous
behavior. Values outside this range or not divisible by 128 are
rejected with a configuration error.

The setting applies to both main CPU cards (6809/6309 and 8088/V20)
and the optional Z80 card. All boards keep a stable 1 MiB backing
allocation and mirror physical RAM addresses above the selected
installed memory size back onto the populated RAM. The 6809's
4 KiB MMR page mappings, direct RAM access, and floppy/HDD DMA use
the same decoder. The 8088's 20-bit CPU and 8237 DMA accesses also
mirror, but **the upper 4 KiB reset-ROM read overlay remains active**
at FF000h-FFFFFh. The 6809's on-card FCxx/FDxx I/O and its boot ROM
window remain separate from the main RAM size.

The 6809 MMR's 8 KiB-pair memory alias probe used by NitrOS-9 Level 2
can thereby detect the RAM size without changing the NitrOS-9 kernel.
At non-power-of-two sizes (384/640/768/896 KiB), missing RAM addresses
wrap at the configured physical limit rather than applying a bit mask.
This is the chosen emulator memory-decoding model, not a verified
transistor-level simulation of every original RAM board.

`ctest -R '(machine_config|main_ram_capacity)' --output-on-failure`
checks all eight sizes, invalid settings, MMR page mapping, Z80 bank
mirrors, 8088 ROM shadow and DMA transfer behavior.

**CP/M-86** still uses a fixed 480 KiB TPA in its CBIOS; that is a
separate next step. This change only adds configurable hardware RAM to
the emulator.

# FM-11 Emulator

## Configured floppy and hard-disk images

The SDL frontend accepts up to **four 2D FDDs** and **four 2HD FDDs**
simultaneously, independently of which main CPU (6809 or 8088) is active.
Their physical selectors are `FD1D & 3` (2D) and `FD35 & 3` (2HD).

Add the following sections to `~/.config/fm11emu/fm11emu.conf`
(or to the file passed with `--config`):

```ini
[floppy_2d]
drive0 = /path/to/system.d88
drive1 = /path/to/data.d88
# drive2 = /path/to/other.d88
# drive3 = /path/to/backup.d88

[floppy_2hd]
# drive0 = /path/to/hd0.raw
# drive1 = /path/to/hd1.raw
# drive2 = /path/to/hd2.raw
# drive3 = /path/to/hd3.raw

[hdd]
# image = /path/to/m2233b.img
```

2D accepts 327,680-byte raw images or 2D D88 images with a complete
40-cylinder, double-sided, 16-sector layout. 2HD accepts native 1,025,024-byte raw images (77 cylinders, two sides,
26 sectors of 256 bytes), including D88 images whose first track uses
26 FM sectors of 128 bytes; the older 1,261,568-byte 8x1024-byte layout
remains supported. HDD accepts one raw MDC image with 256-byte records; geometry
is inferred from the source emulator's supported disk-size table.

Each configured image is independently opened read/write and written
through on completed sector transfers using `pwrite` and `fdatasync`.
A filesystem sync failure is reported as an FDC/MDC write failure.
WRITE TRACK (`F0h`) is now supported on both controllers, following
the original FM-11 `CMainCpu::Fm11WrtTrack` format stream:
2D uses 6,400 DMA bytes on channel 0; 2HD uses 10,900 bytes on
channel 1. The stream's `FE` address marks and `FB` data marks define
the sector IDs and payloads. The emulator validates the complete track,
rebuilds D88 sector headers and offsets if needed, and immediately
persists the resulting image with `fdatasync`. Incomplete or invalid
track descriptions return a controller write error rather than silently
discarding or partially replacing sectors. This supports existing
40x2x16x256 2D and 77x2x26x256 native 2HD geometry (including
128-byte FM sectors on the 2HD first side-track); a D88 image still
must be mountable before formatting.

Do not mount the same image into more than one FDD slot. Keep backups
of important disk images before write tests.

Legacy `--floppy2d FILE` and `--floppy-d88 FILE` override the 2D
slot selected with `--floppy-drive 0..3`; other configured drives
remain mounted.

MDC HDD support provides the FD C0-C3 command interface, DMA-backed
read/write and the original disk-type table. ROM boot and OS-level
HDD formatting remain to be verified separately.

## FM-11 BS (8088-only main CPU)

The FM-11 BS has an 8088 main CPU card and a separate 6809 display
subsystem; it does not require a 6809 **main CPU** card. The SDL frontend
mounts floppy media and connects the serial PTY to the shared FM-11
system devices, independently of the installed main CPU card.

Use `config/fm11bs.conf.example` with the original 8088 boot ROM and
a disk containing its own IPL:

```sh
./build-no-lto/fm11-sdl --config config/fm11bs.conf.example \
  --floppy-d88 ../fm11-cpm86/build/fm11_cpm86_boot_2d.d88 \
  --floppy-drive 0
```

ROM paths for both main CPU cards and the display/ANK subsystems are
provided by `rom` and `ank_rom` keys in the config file. For a temporary
boot override use `--main-rom FILE`, which targets the **selected** main CPU.
Use `--6809-rom FILE` or `--8088-rom FILE` to override one specific CPU
card instead. Explicit `--main-rom` takes precedence for the selected CPU;
`--rom FILE` continues to override the display-subsystem ROM.

No ROM options are needed for the example configuration. The display subsystem
still loads its own ROM through the independent `--rom` option.

To verify the common-device setup:

```sh
cmake -S . -B build-no-lto -DFM11_BUILD_TESTS=ON -DFM11_ENABLE_LTO=OFF
cmake --build build-no-lto --target main8088_bs_shared_devices
ctest --test-dir build-no-lto -R '^main8088_bs_shared_devices
Linux port and ongoing development of the FM-11 emulator.

This first revision establishes a portable CMake build for the existing 6809/6309 and Z80 CPU cores. The Windows MFC/Direct3D frontend is intentionally not part of the Linux build.

## Optional link-time optimization (LTO)

LTO is **off by default**. To compare the same Release build with and without
interprocedural optimization, use separate build directories:

```sh
cmake -S . -B build-normal -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=OFF
cmake --build build-normal -j$(nproc)
ctest --test-dir build-normal --output-on-failure

cmake -S . -B build-lto -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=ON
cmake --build build-lto -j$(nproc)
ctest --test-dir build-lto --output-on-failure
```

`FM11_ENABLE_LTO=ON` checks whether CMake's C++ IPO support is available and
stops configuration with a diagnostic if it is not. IPO is then enabled for
the emulator core, frontends, and tests. No CPU emulation logic or scheduling
behavior changes. Benchmark `fm11-sdl --speed 0 --cpu-speed --renderer gl`
under identical host conditions to assess whether LTO improves throughput.

## Installed CPU cards and main-CPU DIP selection

`fm11-sdl` reads CPU hardware settings from the first available file:
`--config FILE` (explicit), `~/.config/fm11emu/fm11emu.conf`, or
`/etc/fm11emu.conf`. With no file it defaults to the original 6809-only
configuration. A supplied but invalid file produces a line-numbered error.

Copy `config/fm11emu.conf.example` and edit its values. The independent
`[main_6809]` and `[main_8088]` `installed` values describe physically
fitted cards; `[main] active = 6809|8088` models the FM-11 EX DIP switch.
Selecting either card leaves the other fitted and reset, but inactive.
The `[main_6809] cpu` accepts `6809|6309`; `[main_8088] cpu`
accepts `8088|v20`. All `clock_hz` settings are in Hz.

The separate `[optional_cpu] type = none|z80` controls the optional
Z80 card, selected via FD05 relative to the DIP-selected main card.
The display CPU supports 6809 only at present; a `6309` setting is
rejected rather than silently ignored.

CPU ROM and media file paths remain runtime parameters: `--main-rom`
loads the 6809 card firmware, and `--8088-rom` loads the 8088/V20 card
firmware. Specify the selected card's ROM to start its CPU. For instance:

```sh
./build/fm11-sdl --config config/fm11emu.conf.example \
    --main-rom external/roms/boot6809.bin \
    --8088-rom external/roms/boot8088.bin --speed 0
```

`--speed` remains a runtime multiplier of the configured physical CPU
clock. The independent display CPU still observes its 8 MHz runtime cap.
The old `--z80` and `--z80-clock` flags are replaced by config settings.

## External ROM images (not committed)

Original emulator firmware is supplied as C byte arrays. Extract them once
from the source ZIP or an unpacked `Fm11/` directory:

```sh
python3 tools/extract_roms.py /path/to/fm11_x64.zip
# Or: python3 tools/extract_roms.py /path/to/Fm11/
```

This writes the following files under `external/roms/`, which is excluded
by `.gitignore` and **must not be committed**:

| File | Size | Reference array |
|---|---:|---|
| `boot6809.bin` | 4096 | `BootRom.h:BootRom` |
| `boot8088.bin` | 4096 | `BootRom86.h:BootRom86` |
| `display6809.bin` | 20480 | `SubRom.h:SubRom` |
| `ank.bin` | 4096 | `CharRom.h:AnkRom` |
| `kanji.bin` | 131072 | `KanRom.h:KanRom` |

The final emulator will use ROM dumps supplied by the user from their own
machine, in the same binary file format and directory layout. ROM filenames
and sizes are validated, but ROM contents are never embedded in the binary.
The C++ core offers `loadDisplayRomFile` on `Machine` and
`loadBootRomFile` on the 6809 and 8088 adapters. The diagnostic runner
defaults to `external/roms/display6809.bin`:

```sh
./build/fm11-display-romtrace --instructions 100000
# Or override: --rom /somewhere/display6809.bin
```

The headless OS boottrack diagnostic currently uses a synthetic boot ROM,
not the extracted original boot firmware. The font ROMs are staged for the
later display/keyboard frontend, not consumed by current rendering code.

## CRTC hardware text cursor

The Linux SDL frontend composites the FM-11 display-CPU CRTC cursor
in both the OpenGL shader and SDL software renderer. R10/R11 select
the cursor's raster-line range and steady/hidden/16-frame/32-frame
blink modes; R14/R15 and the display sub-CPU AFE0 bit 5 select its
character address. The cursor inverts the completed text/graphics
pixels and is not written into character RAM. Blink phase is derived
from host display time (32 ms ticks) so changing guest CPU speed does
not change its blink period. Both renderers use the same device cursor
state and the existing 4 KiB character RAM wrap / R13-latched
display start. The `display_text_composition` regression exercises
shape, both blink speeds, hidden mode and AFE0 cursor addressing.

Guest ROM behavior and hardware-faithful blink timing should also be
checked on a running CP/M-86 and NitrOS-9 local console.

## Mounted disk image synchronization (SDL frontend)

All 2D/2HD floppy and MDC HDD sector writes still call `pwrite()`
**immediately**, including D88 sector payload writes. WRITE TRACK
replacements also call `pwrite()` and `ftruncate()` immediately. This
keeps the mounted host file updated during guest execution rather than
only when the emulator exits.

To avoid forcing the storage device to commit every 128/256-byte
transaction, a dedicated host thread wakes every **100 ms** and invokes
`fdatasync()` only for images modified since their previous successful
sync. The thread does not hold the image write lock during `fdatasync`,
so normal CPU emulation can continue. Writes arriving during a sync
remain dirty and are included in a later pass.

On normal exit, after the emulator CPU/device workers stop, a final
`fdatasync()` runs for all dirty images before returning from main.
Errors are logged, leave the image dirty for retry, and make a failed
final sync return a nonzero process status. No full-image rewrite
is performed on exit.

**Durability note:** guest sector write completion means `pwrite()`
succeeded, *not* that the host storage has been synchronized. The
periodic sync improves normal-case performance but a host crash or
power loss can still lose changes since the last completed sync,
including data in the guest OS's own write-back cache. A normal
emulator exit flushes host file changes but cannot force a guest OS
to empty its own unflushed buffers.

## 200-line CRT presentation

The FM-11 640x200 video mode is displayed within the same 640x400
screen aperture as the 400-line mode. Each source line is repeated
vertically (nearest-neighbor) so a 200-line display no longer appears
half-height or excessively wide. Both the OpenGL and SDL software
renderers use integer scaling, a 640x400 presentation rectangle,
and a 960x600 initial window. Neither the emulated video mode nor
the underlying VRAM changes.

Use `--scanlines` to darken every second duplicated source line,
approximating the gaps between CRT scanlines. The effect applies only
to 200-line mode and is deliberately optional; without this flag
all doubled rows have identical brightness. The normal `F1` mode
switch still selects 200/400 emulated scanout lines, and the aspect
ratio remains the same in both modes. Use `--renderer gl` and
`--renderer software` to compare presentation backends.

## SDL auxiliary windows and desktop attention

The independent Settings and file-selection SDL windows use
`SDL_ShowWindow()` but do not call `SDL_RaiseWindow()`. Some Linux
window managers (including GNOME/Mutter) treat explicit focus-stealing
requests as demands for attention and show a desktop notification when
the request cannot be granted. The UI must not request attention merely
because a dialog was opened. This change does not use D-Bus or a system
notification service; focus and stacking are left to the window manager.

Manual test on GNOME/X11 and GNOME/Wayland, where available: open and
close Settings repeatedly, open the independent file picker from both
Settings and the CRT File menu, then check that no "window is ready"
or attention notification appears. Check that both windows remain
clickable and can be brought forward normally using the window manager.

## Stage 4-D: final integrated regression coverage

Stage 4 is functionally complete. The CRT menu, Settings window, and
independent file chooser keep their accepted layout and interaction.
No additional styling changes are made in Stage 4-D.

The new `PendingMachineSettings` model is **used by the Settings GUI
itself** and exercised by the headless `settings_apply_state` CTest.
It verifies that:

- closing an uncommitted editor session discards only un-Apply'ed values;
- Apply stages CPU/RAM/HDD choices but never touches the running machine;
- an invalid Apply cannot overwrite an earlier valid staged selection;
- an unsuccessful Reset retains the applied selection for a retry;
- successful Reset clears the pending flag;
- FDD hot-swap/eject updates the next-reset media paths without losing
  unrelated pending CPU/RAM settings.

Earlier tests `live_floppy_mounts`, `host_image_sync`, `worker_group`,
`machine_config`, and `machine_config_save` continue to exercise the
media persistence, worker lifecycle, and configuration parts.

Run the focused suite and then all tests:

```sh
git pull
cmake -S . -B build -DFM11_SETTINGS_GUI=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(settings_apply_state|live_floppy_mounts|host_image_sync|worker_group|machine_config|machine_config_save)

The Settings editor's **Apply** button validates and stages the edited
hardware configuration for the next **Settings > Reset machine**. On
successful Apply the Settings editor closes immediately; the guest does not
reset or otherwise change until Reset is requested. An invalid configuration
keeps the editor open and reports the failure.

Clicking **Close**, the window manager's **X**, or toggling the editor off
with **Configuration... / Ctrl+F10** discards any changes not yet applied.
Reopening the editor starts from the last applied configuration, not from
the abandoned form. **Discard edits** also restores that applied baseline
without closing the window. A previously staged Apply survives cancelling a
later editing session and remains pending for Reset.

**Save user configuration** is independent: it writes the edited values
to the XDG file but does not Apply or Reset them. Live FDD exchanges remain
immediate and update the matching displayed/staged drive paths.

## Stage 4-C: GUI reset and apply configuration

The SDL **Settings** menu now has two separate commands:

- **Reset machine**: reset the current emulator hardware, keeping its
  current CPU cards, RAM, ROM selections and live-mounted disk images.
  Pending edits are **not applied**.
- **Apply settings & Reset**: use all currently edited fields from the
  Settings dialog to construct a new Machine (CPU cards, 6809/6309/V20,
  memory, display CPU and clocks, boot ROMs, DIP switches, serial/PTM
  timing, HDD and FDD image paths). The existing SDL CRT, renderer,
  menus and independent settings/file-picker windows are retained.

The Settings editor provides matching **Reset only** and
**Apply & Reset** buttons. The running CPU worker, display CPU and
real-time PTM are parked before a normal reset, and fully stopped before
machine reconstruction. Previously paused emulation remains paused;
previously running emulation resumes. Serial and audio backends are
rebound to the newly configured machine.

Apply is deliberately **separate from Save user configuration**: the
user's XDG INI file is not modified until Save is explicitly selected.
The currently running image paths are also preserved on a plain Reset,
including FDDs hot-swapped via Stage 4-B. A new Apply & Reset mounts
the image paths from the edited GUI state.

Before reconstructing, the frontend synchronizes outstanding host disk
writes. If validation or new-machine setup fails, it restores the
previous machine, configuration, mounted images and worker execution
state rather than discarding the running guest. An error dialog
explains the failure. CPU debugger hooks are reattached to a new
Machine; CPU debug breakpoints are not automatically migrated to the
new machine after an Apply & Reset.

The `worker_group` regression additionally checks explicit stop,
rearm and restart, including parking the new worker. Run all CTest
tests and smoke-test both `--renderer gl` and
`--renderer software` with NitrOS-9, CP/M-80 and CP/M-86 boot disks.
Suggested manual check: first use Reset machine with a mounted FDD,
then change RAM and main CPU type (with an appropriate ROM and installed
card), Apply & Reset, check the rebooted guest and serial endpoint,
and confirm the saved user configuration is unchanged until Save.

## Stage 4-B: live FDD media exchange

The SDL File menu's **2D floppy** and **2HD floppy** drive submenus now
mount/replace/eject their selected images immediately, across all four
physical drives on each controller. File selection itself is asynchronous;
only the short media swap uses the existing CPU, sub-CPU and PTM pause
mechanism. The emulator resumes its original running/paused state.

The FDC refuses an exchange during a selected-drive seek, timed read/write,
WRITE TRACK, or unfinished shared PIO/DMA transfer. A rejected exchange
shows a GUI error and leaves the current image and its displayed path
unchanged. Retry after the drive finishes I/O. A malformed image, a
read-only/unwritable host image or a duplicate mounted filename is likewise
rejected before the live media is touched. Aliases to the same canonical
file are detected.

All floppy sector and WRITE TRACK host writes now follow the *current*
per-drive image, instead of the startup image captured in the callback.
The old image is explicitly fdatasync'ed before detach; a failed sync
blocks the exchange. The 100-ms background host sync continues. Eject
disconnects the selected medium without changing other drives or the HDD.

**Important:** Host synchronization does not flush the *guest OS's* own
disk cache. Before exchanging a disk that the guest has written, close
open files or invoke the guest's normal flush/unmount procedure. The GUI
does not automatically save these changes to a configuration file;
**Save user configuration** remains a separate explicit action. The
HDD image path still changes only after Stage 4-C reset/apply support.

Regression test:
```sh
cmake --build build -j$(nproc)
ctest --test-dir build -R 'live_floppy_mounts|host_image_sync|floppy' --output-on-failure
ctest --test-dir build --output-on-failure
```

Manual check: boot from a 2D disk, use File > 2D floppy > Drive 1 to
mount another image, verify the guest can read it, then eject it and
verify not-ready behavior. Repeat for 2HD. Verify writes after
replacement reach the new host image and not the old one, including
after closing and restarting the emulator. Busy-drive exchanges should
be rejected and may be retried after disk activity ends.

## Stage 4-A usability update: larger controls and standalone chooser

The CRT File/Settings/Help menu and Settings editor now use larger fonts
(19-pixel ImGui default fonts) and more generous widget spacing. The
menu strip height is 40 pixels, outside the unmodified FM-11 640x400
image. It works with both OpenGL and software SDL renderer backends.

ImGuiFileDialog now runs inside a **third, separately managed SDL window**
rather than as a popup embedded in either the CRT or Settings window.
The chooser initially measures 1024x740 with a minimum of 850x620,
and its ImGui dialog is sized to fill the window. File-selection events
are polled in the existing SDL event loop, so displaying it never
requires a blocking GTK/NFD dialog or pauses emulated CPU workers.

The Settings editor's former Disks tab is removed. The File menu lists
2D/2HD FDD 0..3 and shows the currently *staged* image basename directly
beside each drive, with the full path shown inside the drive submenu.
The File menu also shows the HDD image. Until Stage 4-B, choosing/ejecting
a disk changes only the staged configuration; the mounted medium is
unchanged. The HDD selection is still also editable under ROM / Display,
and will apply after reset support is added in Stage 4-C.

The three CPU model choices (MC6809/HD6309 main, 8088/V20 main,
MC6809/HD6309 display) use radio buttons instead of drop-downs.
RAM capacity is an exact eight-choice control: 128, 256, 384, 512,
640, 768, 896, 1024 KiB.

Manual regression check after rebuilding: open both the File-menu and
Settings Browse choosers (confirm separate OS windows and readable initial
size), resize them, cancel and select paths, reopen the Settings editor,
verify selected filenames in the menu, then repeat with
`--renderer gl` and `--renderer software`. The regular CTest suite
continues to cover emulator behavior.

## Stage 4-A addendum: CRT menus and nonblocking file picker

The main SDL CRT window now draws **File / Settings / Help** in a reserved
28-pixel top menu strip (the emulated 640x400 display is never overwritten).
This is rendered using the matching Dear ImGui SDL2/OpenGL3 or
SDL2/SDL_Renderer backend; the guest renderer still selects GL or software
with `--renderer`.

- **File**: 2D and 2HD physical drives 0..3, with **Choose image...**
  and **Eject (edit only)**. These actions edit the pending configuration,
  not the mounted disk. Live media changes are Stage 4-B.
- **Settings**: **Configuration...** opens the separate settings window,
  **Pause/Resume** immediately uses the existing worker pause mechanism.
  **Reset** is visibly disabled pending Stage 4-C.
- **Help**: keyboard shortcuts and About popups.

`Browse` buttons on all eight FDD slots, the HDD image, and all ROM paths
use **ImGuiFileDialog v0.6.8 (MIT)**. The File menu opens its picker
inside the CRT window; settings Browse buttons open a picker inside the
separate settings window. File dialogs run inside ImGui frames
and do not nest a blocking native file-dialog event loop: the guest CPU,
PTM, serial service and SDL painting continue while files are selected.
The chosen path is stored in the pending editor configuration and is
persisted only when **Save user configuration** is clicked. A selection
alone never swaps live FDDs or reinitializes the current machine.

When the settings GUI is enabled, CMake FetchContent fetches ImGuiFileDialog
v0.6.8 along with pinned Dear ImGui. With
`-DFM11_SETTINGS_GUI=OFF` neither library is required, preserving the
existing SDL frontend and its original menu-free guest display.

Manual smoke check on both `--renderer gl` and `--renderer software`:

1. Confirm File/Settings/Help menus appear in the CRT window without
   changing the guest's 640x400 display content or proportions.
2. Choose a 2D FDD image through File > 2D floppy > Drive 0 > Choose image;
   verify the picker can be cancelled or used while the guest continues.
3. In Settings > Configuration, use Browse on a ROM and HDD path; save
   the configuration; inspect `~/.config/fm11emu/fm11emu.conf` (or the XDG
   destination); reopen and verify the edited paths.
4. Pause and resume with Settings > Pause / Resume and F2, check the guest
   keyboard, and repeat on the other renderer.
5. Verify `-DFM11_SETTINGS_GUI=OFF` still builds.

## Stage 4-A: SDL settings editor

The SDL frontend has a separate Dear ImGui settings window, toggled with
**Ctrl+F10**. It keeps the FM-11 CRT window intact, in both OpenGL and
software rendering modes. Edit configuration values under Disks, CPU/RAM,
ROM/Display, Serial, DIP switches, and Timing, then click **Save user
configuration**.

This first stage is **editing and saving only**: edits do not alter the
running emulation. FDD hot-swap is Stage 4-B; reset-and-apply (including
CPU board/RAM rebuilding) is Stage 4-C. In Stage 4-A the disk image
entries are paths, not file picker widgets.

The save destination is always the user's XDG location:

- `$XDG_CONFIG_HOME/fm11emu/fm11emu.conf` if XDG_CONFIG_HOME is absolute
- otherwise `$HOME/.config/fm11emu/fm11emu.conf`

The program continues to load settings in the order `--config`, user
config, `/etc/fm11emu.conf`, built-in defaults. The GUI never writes
`/etc` or an explicitly loaded `--config` path. If you keep passing
`--config /etc/fm11emu.conf` on subsequent launches, it will still
override the GUI-saved user configuration. The GUI displays its save path
and warns about this. Saving writes a complete INI file to a 0600
temporary file and atomically renames it over the user file after
syncing, rejecting invalid entries before replacement.

Stage 4-A uses **Dear ImGui v1.91.9b** fetched by CMake when building the
SDL frontend. This is a build-time dependency, not a new runtime package.
If building offline and the ImGui sources are not cached, configure with
`-DFM11_SETTINGS_GUI=OFF` to retain the existing SDL frontend without
the settings editor. After first configuring with network access, the
fetched sources remain in the build directory.

```sh
cmake -S . -B build -DFM11_SETTINGS_GUI=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R 'machine_config|sdl_help' --output-on-failure
ctest --test-dir build --output-on-failure
./build/fm11-sdl
```

## SDL2 display frontend (experimental)

Install `libsdl2-dev` on Ubuntu, then reconfigure CMake so it can
discover SDL2. This frontend runs the display subsystem through the shared `Machine`
scheduler and renders VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

To run the main 6809 CPU alongside the display CPU, supply its boot ROM:\n\n```sh\n./build/fm11-sdl --main-rom external/roms/boot6809.bin\n```\n\nThe default display ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
SDL always scales the image by an **integer** factor and centers it on
dark-gray margins with a thin light-gray outline just outside the emulated
screen. Neither the margins nor the outline alter any emulated pixels. At the default 960-pixel window width, the 640-pixel image
is shown at native 1:1 size rather than distorted to 1.5x. Enlarge the
window to at least 1280x800 for a sharp 2x presentation in either
200- or 400-line mode. Smaller windows clip instead of using
fractional resampling.
`--high` forces 400-line mode. `[display] height = 200|400` selects the initial viewport (default 400). The SDL frontend defaults to
**`--speed 10`** (ten times real-time emulated CPU/peripheral time).
Use `--speed 1` for real-time FM-11 clocks, `--speed 5` for five times
real-time, or `--speed 0` for unlimited execution (as fast as the host
can manage). Values 0 through 100 are supported as whole-number
multipliers. The hardware clock configuration remains 2 MHz for the
main 6809/display CPU and 4 MHz for the optional Z80 by default;
`--speed` changes the host-time pacing, not those clock settings.
Guest CPU/peripheral clocks remain synchronized relative to each other.
`SYNC/CWAI` advances peripherals at the requested virtual rate while
the main CPU is waiting. The display refresh cadence remains tied to
host time, near 31 frames/s (approximately one frame per 32 ms),
and the SDL renderer does not request VSYNC so rendering does not
block guest CPU execution waiting for a display refresh. `--cycles N` remains the *maximum* number
of primary CPU instructions per SDL loop (default 10000), not a clock
rate. Achieving the requested speed depends on host performance and
graphics/ROM workload. In unlimited mode idle waiting is also unpaced;
expect higher host CPU usage.

The window renderer defaults to `--renderer auto`: it tries the
OpenGL 3.3 core-profile shader renderer first and falls back to the existing
SDL software-composition path if the GPU/context is unavailable. Use
`--renderer gl` to **require** OpenGL (startup reports an error if unsupported),
or `--renderer software` to retain the previous `DisplayBitmap` and
SDL texture path. The chosen backend is printed on startup. The GL
fragment shader decodes all three physical graphics VRAM planes,
the palette, 4 KiB interleaved character/attribute RAM, the ANK ROM,
and the CRTC's latched text start directly from raw integer textures.
It preserves 200/400-line selection, text attributes, reverse video, and
integer pixel scaling without CPU bitmap composition. It requires desktop
OpenGL 3.3; neither the headless emulator nor the portable core/tests
requires OpenGL development headers. Both renderer modes remain
non-VSYNC at the host's ~31 fps repaint cadence. To compare performance,
run the same workload with `--cpu-speed --renderer gl` and with
`--cpu-speed --renderer software` (keeping `--speed` unchanged).

To measure effective main-CPU throughput, pass `--cpu-speed` to `fm11-sdl`.
It reports each second's **executed guest T-states**, wall-clock MHz,
instruction counts, and total guest CPU time per wall second on stderr.
6809 and Z80 are measured separately across FD05 handoffs. For example,
an uninterrupted 4 MHz Z80 at `--speed 1` should approach
**4,000,000 T/s** and **4.000 MHz**; at `--speed 10` the ideal
is **40,000,000 T/s** and **40.000 MHz**. A smaller rate
can indicate host-side slowdown (or guest CPU waiting/switching).
`guest/wall` sums emulated 6809 and Z80 instruction times and divides by
wall time. Interrupt waits (SYNC/CWAI) and other idle periods reduce this
ratio even when the emulator itself is not overloaded. The display sub-CPU
is intentionally excluded. The `target=` field shows the requested
speed multiplier and `guest/wall=` shows the achieved aggregate guest
CPU time per host second (not counting time spent in idle waits).
This diagnostic has no effect unless enabled; it does not adjust
guest clock speeds.

For a black screen while running **original** main and display ROMs, first
inspect the main CPU boot path and shared-bus handshake without changing
emulated memory or acknowledging IRQ/event registers:

```sh
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --trace-boot
```

`--trace-boot` prints the first 64 main 6809 instruction addresses
before execution. The periodic diagnostic line also reports the current
main PC (**decimal**), FD05 sub-CPU control latch, FD04 event latch,
display BUSY byte, number of main-to-sub shared RAM changes, first
shared byte, and display HALT state. All of these readings are
non-destructive; unlike a guest read from FD04, the diagnostic does
not acknowledge the event. Compare changes over multiple reports.
A constant display PC alone does not establish why firmware is waiting.

If the window stays black, use `./build/fm11-sdl --test-pattern` to
verify SDL rendering independently of firmware. This displays eight-color
stripes and does not require a ROM. Normal execution now prints the display
CPU PC, executed instructions, I/O writes, and count of nonzero physical
VRAM bytes every two seconds to stderr. An unchanged PC or zero VRAM count
is useful evidence that the firmware is waiting for main-CPU commands
rather than drawing a frame; the counters do not themselves establish why
it is waiting.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is an early integrated CPU/display viewer, not yet a complete
FM-11 keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

## Main/sub-CPU binary integration test

Before diagnosing original firmware, use the synthetic 6809 programs to
exercise only the main-to-display path. They are generated from opcode
bytes and **contain no original firmware**:

```sh
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_integration build/test-roms/main_test.bin build/test-roms/display_test.bin
ctest --test-dir build -R display_binary_integration --output-on-failure
```

A second, separate regression tests a complete command/acknowledgement
exchange through the same transport. The synthetic main-CPU ROM writes
command `$81` to `$FC80`, the display ROM reads it from `$9F80`,
draws a white pixel, then writes acknowledgement `$55` to `$9F81`.
The main CPU polls `$FC81` and records success as `$66` at
`$FC82`. Run it independently:

```sh
ctest --test-dir build -R display_binary_handshake --output-on-failure
./build/display_binary_integration build/test-roms/main_handshake.bin build/test-roms/display_handshake.bin --handshake
```

This validates bidirectional shared memory and CPU scheduling, **not**
the original FM-11 interrupt/attention protocol; a hardware notification
register is not inferred from this test.

### AFE0 display sub-CPU BUSY/READY (reference SubCpu.cpp)

The original `CSubCpu::SetIoByte($AFE0)` drives the sub-CPU
BUSY/READY handshake: writing bit 0 = 1 signals READY, and writing
bit 0 = 0 signals BUSY. Reading `$AFE0` returns the written control
byte with bit 0 forced to the current READY state. The former Linux
implementation incorrectly treated `$AFF0` as BUSY; the reference
instead sends `$AFF0` writes to a DMA channel register.

```sh
ctest --test-dir build -R 'display_afe0_handshake|display_binary_halt|display_cpu6809' --output-on-failure
```

The display subsystem now models the AFE0 handshake independently of
the AFF0 latch. Main-side **FD05 bit 7 now also reports BUSY** as controlled by
sub-CPU AFE0. An asserted main-side HALT is kept visible until release;
subsequent AFE0 READY clears the bit when HALT is released. Other FD05
bits and the original sub-CPU interrupt wiring still need verification.
The SDL diagnostic `BUSY` reports the AFE0-driven state.

### AFE6 video status (reference SubCpu.cpp)

The display ROM polls bit 7 at `$AFE6`. The original emulator's
`CSubCpu::GetIoByte` **changes status on every read**: after a
`VSyncCount`-driven clear, bits 7 and 2 toggle on subsequent reads
when `VSyncMode` is zero, and bit 5 is set. `VSyncCount` is
decremented as the sub CPU executes instructions and is set to 100
when CRTC register 13 is written. A previously guessed periodic
60Hz/1ms source has been removed.

```sh
ctest --test-dir build -R 'display_binary_raster|display_cpu6809' --output-on-failure
```

The synthetic display ROM exercises the original `TST $AFE6`
branch loops. This fixes the status semantics tested here but does
not yet port the original character-display composition or every
sub-CPU peripheral behavior.

### Display CPU interrupt dispatch and sub-I/O RAM port

The original `SubCpu.cpp::SimONE` checks pending sub IRQ, the `AFE4`
FIRQ latch and a periodic NMI after the first `AFE0` write. The
display 6809 adapter now invokes `SimIRQ`, `SimFIRQ` and `SimNMI`
before executing its next instruction, with the original initial
NMI threshold of 60000 and a CPU-cycle-based clock. Note: the
reference increments its clock for each memory access and adjusts
the NMI threshold dynamically against wall time; the Linux timing
model is thus **not yet cycle-equivalent**.

The original `GetMemByte/SetMemByte` also accesses sub RAM in
`A000-AFFF` except explicitly decoded registers. Linux previously
returned `FF` for much of that aperture. Ordinary sub I/O working
RAM is now readable and writable there; reading `AFE5` acknowledges
the sub IRQ request as in original `GetIoByte`. Tests exercise RAM
readback, reset, IRQ acknowledgment and NMI reset-period defaults.

```sh
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

This does **not** assert that real display ROM boots: sub IRQ request
generation from the main-side `FD05` bit6 and original `105`
ready/interrupt gating are still to be wired, and keyboard host input
and timing require additional integration.

### Sub-CPU keyboard event queue (source SubCpu.cpp)

The original Windows emulator's first `AFE0` write queues key event
`0x0050` and clears its NMI mask. Subsequently, `GetKeyCode`
copies pending keyboard events into `AFEC/AFED`, asserting bit 0 of
`AFE4` for ordinary key codes. Reading `AFED` invokes
`GetKeyCode` to advance the queue.

The Linux subsystem now includes `enqueueKeyCode`, `processKeyCode`,
the one-shot initial `AFE0` key event and the `AFED` queue advancement.
The independent FIFO regression checks these state transitions:

```sh
ctest --test-dir build -R 'display_keyboard_queue|display_io_read_clear' --output-on-failure
```

The original implementation also invokes `GetKeyCode` from a timed
keyboard/vsync event and supplies interrupts through `SimFIRQ` and
`SimNMI`. These *CPU-side interrupt delivery and timing paths* remain
unimplemented and are **not claimed** by the new keyboard FIFO test.
The original ROM may therefore still be black.

### Original display I/O read-clear latches

The reference Windows `Fm11/SubCpu.cpp::GetIoByte` clears three
register latches **on read**: `AFE4` (sub FIRQ), `AFEC`
(keyboard status) and `AFED` (keyboard data). The Linux display
subsystem now reproduces those read-clear semantics, including zero
values after reset and ignoring software writes to these registers.
A regression supplies external latch values using
`signalSubFirq` / `setKeyboardLatches` and verifies one-shot reads.

```sh
ctest --test-dir build -R 'display_io_read_clear|display_cpu6809' --output-on-failure
```

This is **not yet** the whole original keyboard/NMI implementation:
original `GetKeyCode()` processing after `AFED`, NMI timing, and
interrupt delivery remain separate work. Do not interpret passing
this test as proof that original display firmware now boots.

### Stage 10: original display ROM versus synthetic main sender

The stage-8 synthetic main ROM is kept as a **known, deterministic sender**.
Unlike stage 8, however, the sub CPU runs the user's **unmodified
original 20 KiB ROM**. This isolates genuine firmware startup/READY
handling from the main boot ROM and from the synthetic sub-CPU wrapper.

```sh
git pull
cmake --build build -j$(nproc)
python3 tools/make_display_test_roms.py build/test-roms
./build/display_real_rom_probe \\
    build/test-roms/main_text_real_protocol.bin \\
    external/roms/display6809.bin
```

The probe records up to 32 original-sub-CPU AFE0 writes with the
instruction PC, values, main FD05 status, READY/BUSY totals and
the busiest sub-CPU PCs. It also prints FC81/FC82/FC83/FC84 and
FDA0 after a bounded instruction budget. A zero exit code means
**the diagnostic completed**, not that the original firmware has
accepted the stage-8 test command.

The stage-8 main ROM's request format is only a controlled stimulus:
there is **no assertion** that it is the real firmware character
command. Do not use this probe's output to claim the original text
command works. This program intentionally is not a CTest because it
requires the operator's original display ROM. Once the firmware's
own command parsing is established, write a genuine command sender
and its regression test rather than inventing any new protocol.

### Stage 9: audit actual ROM display-command dispatch (preparation)

Do **not** treat the stage-8 `CAA0` byte-receive routine as proof of the
real firmware's text *command* semantics. The reference `SubRom.h`
begins with `B000: JMP B018`, and the ROM contains eight handler
addresses at `B07C-B08B`. Those are actual firmware entry/dispatch
structures, unlike the synthetic byte `A5` used in stage 7.

The first step is a reproducible, read-only audit of the operator's
original display ROM:

```sh
python3 tools/inspect_display_rom.py external/roms/display6809.bin
```

The tool validates the ROM size and startup signature, prints the eight
handler targets and the genuine `CAA0` bytes, and **does not infer**
unknown display command codes. Next, trace the original main-ROM sender
and the `B018/B08C` command decoding and identify the actual character
output handler before producing the stage-9 synthetic-ROM test.
No new communication behavior or firmware-specific display test is
claimed by this inspection-only commit.

### Original SubRom receive handshake: stage 8

This stage does **not invent a new receive protocol**. The generated
display ROM includes, byte-for-byte, the receive subroutine at
`$CAA0-$CACA` from the reference `Fm11/SubRom.h`, relocated
unaltered to `$B100` (all its branches are relative).

The reference `Fm11/SubCpu.cpp` maps the display CPU's `$9F80-$9FFF`
to main shared RAM `$FC80-$FCFF`. Its `$AFE0` bit 0 sets or
clears main-side `$FD05` bit 7.

- The original subroutine signals READY through `AFE0`, polls
  `9F82`, signals BUSY, clears bit 7 in `9F81`, then returns
  the payload from `9F82`.
- The synthetic main ROM waits for READY at `FD05`, writes a
  request with bit 7 set to `FC81` and ASCII `M` to `FC82`,
  and waits until the genuine subroutine clears the request bit.
- A **test-only wrapper**, not original firmware, writes the
  returned character to text RAM and signals display completion
  through `9F83`, after which main enables `FDA0=03`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_real_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_real_protocol.bin \\
    --rom build/test-roms/display_text_real_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected: a white `M` at the screen origin. This validates the
**original receive-handshake routine**, but not yet the actual
original firmware's display-command dispatcher or its character
command encoding. The real boot ROM is still not used in this test.

### Synthetic main-to-sub 6809 text request: stage 7

This is the first test where the **main 6809 requests text via shared
RAM**, rather than the display CPU unconditionally painting a predefined
message.

1. Sub 6809 initializes CRTC then writes `AFE0=1` (READY).
2. Main 6809 waits for `FD05 bit7=0`, writes command `A5` to
   `FC80` and ASCII `M` to `FC81`.
3. Sub 6809 reads the matching shared addresses `9F80/9F81`, copies
   the requested character into its text RAM and acknowledges by writing
   `9F82=66`.
4. Main 6809 detects the acknowledgement, enables text using
   `FDA0=03`, and sets a completion marker `FC83=77`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_protocol.bin \\
    --rom build/test-roms/display_text_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected SDL display: a white `M` at the upper-left of the screen.
The regression checks the READY transition, both CPUs' shared
request/acknowledgement, text/CRTC state, and 200/400-line pixels.
This is a deliberately minimal *test protocol*, not yet the original
firmware's command encoding. Original ROM boot is not part of this stage.

### Synthetic 6809 CRTC display-start scrolling: stage 6

Two sub-CPU ROMs write **identical** `FIRST LINE` and `SECOND LINE`
text into row 0 and row 1 of the 80-column text RAM. The first
ROM sets CRTC register 13 to 0; the second sets it to 80 text
cells (one full row). The main CPU ROM enables text as in stage 1.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_scroll --output-on-failure

# First row shows FIRST LINE, second row SECOND LINE:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_top.bin \\
    --ank-rom external/roms/ank.bin --high

# After CRTC start-address change, SECOND LINE moves to the first row:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_scroll.bin \\
    --ank-rom external/roms/ank.bin --high
```

This stage checks both 200- and 400-line framebuffer results with
generated distinguishable glyphs and verifies that the **same**
physical text RAM contents are present in both variants. It tests
the current CRTC text-start mapping, not original firmware boot.

### Synthetic 6809 display-layer selection: stage 5

This test makes the graphics region **large enough to see**.
The common display ROM writes yellow text `LAYER TEST` plus a 640x200
blue/black alternating-pixel stripe field, and the main ROM selects
which layers are enabled via `FDA0`:

| Main ROM | FDA0 | Expected appearance |
| --- | --- | --- |
| `main_layers_text.bin` | `03` | Yellow text on black |
| `main_layers_graphics.bin` | `0C` | Blue/black stripes across the screen, no text |
| `main_layers_both.bin` | `0F` | Yellow text over blue/black stripes |

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_layers --output-on-failure

# Change 'both' to 'text' or 'graphics' for the other two modes:
./build/fm11-sdl --main-rom build/test-roms/main_layers_both.bin \\
    --rom build/test-roms/display_layers.bin \\
    --ank-rom external/roms/ank.bin
```

The three new tests check actual main/sub-6809 writes and pixels for
both 200- and 400-line scanout. The 400-line view includes the
graphics plane's upper area, which is still blank because the test
draws only the first 200 scanlines.

### Synthetic 6809 graphics/text overlay: stage 4

This stage uses a **new pair of synthetic 6809 ROMs**. The main CPU
enables both text and graphics (`FDA0=$0F`). The display CPU sets
80-column/25-row CRTC text mode, writes a yellow `T` and a reverse
red `R` into text RAM, and paints two blue graphics pixels underneath
their cells using the graphics plane registers.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_overlay --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_overlay.bin \\
    --rom build/test-roms/display_text_overlay.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest runs the same ROMs with a generated minimal ANK font, and
checks both 200- and 400-line frames. The `T` glyph overlays the first
blue pixel in yellow, while the reverse-video `R` leaves the second
blue pixel visible through the glyph hole and paints the other pixels
of that character cell red. This tests actual output blending rather
than simply checking whether the two memory regions were written.

### Synthetic 6809 text attributes: stage 3

After validating basic text and 40/80-column geometry, the next
self-contained 6809 ROM writes six text cells with distinct attributes:

- `R`, `G`, `B`: red, green and blue character colors.
- `R` (fourth cell): reversed red, with the cell background filled.
- `H`: bright-white attribute.
- `W`: normal-white attribute.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_attributes --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_attributes.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest uses a generated minimal ANK glyph ROM, checking the text
attribute bytes written by the sub 6809 and the composited pixel colors
in both 200- and 400-line views. The SDL command uses the actual
extracted ANK ROM to show the characters.

F1 only changes the *200/400-line view*, not the CRTC 40/80-column
setting. In the current rendering model the 200-line view downsamples
400 logical character scanlines, so text appears vertically compressed.
Fidelity to original hardware scanline behavior remains to be verified.

### Synthetic 6809 text geometry: 40/80 columns and 200/400 lines

Stage 2 reuses the working stage-1 main ROM and two new display
6809 ROMs. The display CPU sets CRTC R1 to 20 (40 columns) or
40 (80 columns), R6 to 25 rows and writes `40/80 COL TEST`
into text RAM. The tests check both 640x200 and 640x400 framebuffer
geometry, using a deterministic test-only ANK font.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R 'display_binary_text_(40|80)col' --output-on-failure

# 40 columns (16 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_40col.bin \\
    --ank-rom external/roms/ank.bin --high

# 80 columns (8 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_80col.bin \\
    --ank-rom external/roms/ank.bin --high
```

Press F1 in SDL to compare the 200- and 400-line representations.
The expected 40-column letters are twice as wide as the 80-column
letters. This stage confirms the current geometry model; fidelity
to all original CRTC/scanline behavior must still be compared with
the reference emulator.

### Synthetic 6809 text display: stage 1

Start with a small **end-to-end** text test before original firmware. Both
processors run real 6809 binary instructions: the main CPU writes `$FDA0=3`
(text enabled); the display CPU initializes the CRTC to 80x25, writes
`HELLO FM-11` as alternating character/attribute bytes starting at
`$8000`, and places `$66` in shared RAM on completion.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_stage1 --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_stage1.bin \\
    --ank-rom external/roms/ank.bin --high
```

The headless regression uses a separate generated font
`ank_text_stage1.bin` with simple known glyph pixels, and checks
CPU-written text RAM, CRTC values, and output pixels. The SDL run above
uses the **actual** extracted ANK ROM, so `HELLO FM-11` should appear
at the top left. If the original ANK ROM is not available, the same SDL
command can use `build/test-roms/ank_text_stage1.bin` to show simple
glyph marks rather than letters.

This is **stage 1 only**: plain text on black. After confirming its
SDL result, later stages will cover colors, reverse attributes and
graphics/text overlays separately.

### Reference ANK text composition (first implementation)

The original `Fm11/fm11.hlsl` composites two-byte character/attribute
cells from the sub-CPU text RAM, with the CRTC start address and 8x16
ANK glyphs. That operation is now available in the SDL frontend, including
text reverse video, color attributes, and the 640x200/400 output sizes.
The software scanout matches the shader's LSB-first glyph bit ordering.

The reference archive includes `Fm11/CharRom.h`; convert its embedded
4096-byte ANK array without committing those font bytes:

```sh
python3 tools/extract_ank_rom.py /path/to/Fm11/CharRom.h external/roms/ank.bin
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --ank-rom external/roms/ank.bin
ctest --test-dir build -R display_text_composition --output-on-failure
```

Providing `--ank-rom` loads font data; it does **not** force the
text layer on when a main CPU is present. The original display control
register `$FDA0` enables text with bits 0/1 and graphics with bits
2/3. The renderer also applies the eight programmable palette entries
from `$FD98-$FD9F` and the original 16-entry color table.
The current software scanout supports attribute bit 5 (bright/bold)
and reverse video; blink, cursor and clip state remain future work.
Without a main CPU, specifying `--ank-rom` still explicitly enables
text for standalone diagnostics.
Other original display behavior, including blink, cursor and clip, remains to be ported.
Without `--ank-rom`, the established graphics-only regression frames
remain unchanged.

### Full-frame binary display test and SDL preview

To check the complete raster path without original firmware, generate
the synthetic ROM pair and run the test:

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_pattern --output-on-failure
./build/display_binary_pattern build/test-roms/main_pattern.bin build/test-roms/display_pattern.bin
```

The main 6809 sends a command through shared RAM; the display 6809
writes all 16,000 bytes of the 640x200 plane with an alternating pixel
pattern in all three colors, then acknowledges completion. The test
checks **every rendered pixel**, not just the first byte.

To inspect the same pattern visually in the SDL frontend:

```sh
./build/fm11-sdl --main-rom build/test-roms/main_pattern.bin \\
    --rom build/test-roms/display_pattern.bin
```

This should display narrow alternating white and black vertical lines
across the entire 640x200 framebuffer. It is a deliberately artificial
bring-up image, not the FM-11 firmware's normal boot screen.

### Display-generated ATTENTION (original I/O semantics)

Original `Fm11/SubCpu.cpp`, `CSubCpu::GetIoByte`, case
`$AFE7`, raises main-CPU ATTENTION when the display CPU **reads**
that address. This is distinct from the earlier host-injected event
tests. The implementation now connects that read to the existing main
`$FD04` event latch.

A pair of tiny 6809 binaries verifies that the display CPU's `LDA
$AFE7` triggers an event that the main CPU receives with `LDA
$FD04` and then acknowledges by reading `$FD04` again:

```sh
ctest --test-dir build -R display_binary_afe7 --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_afe7 build/test-roms/main_afe7.bin build/test-roms/display_afe7.bin
```

### HALT and BUSY binary regression

```sh
ctest --test-dir build -R display_binary_halt --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_halt build/test-roms/main_halt.bin build/test-roms/display_halt.bin
```

This runs real, standalone MC6809 binary instructions on both CPU
cores: main writes `$FD05` to halt the display CPU, writes a marker to
shared `$FC80`, and releases HALT. The test checks that the display
PC remains frozen during HALT; after release, the display ROM sets
`$AFF0` (BUSY) and writes a shared-RAM marker at `$9F82`.
There is no dependency on an original ROM dump.

ATTENTION's sub-CPU hardware trigger is still not mapped. The existing
`MainBoard::signalAttention()` path does **not** establish how an
actual display ROM should raise the event; this test deliberately does
not fabricate one.

The main CPU program at `$5000` writes `$80` to `$FC80`.
The display CPU program at `$B000` waits for the same value at
`$9F80`, then writes `$80` to VRAM through the three color planes.
The test asserts shared RAM, physical VRAM and the first rendered white
pixel. CTest creates temporary binaries automatically; the manual commands
above keep them under `build/` for inspection.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current scope

- Portable fixed-width compatibility types for legacy `BYTE`, `WORD`, `DWORD`, and `LONG` usage.
- Existing `Sim09` 6809/6309 instruction core compiled as portable C++.
- Existing `SimZ80` instruction core compiled as portable C++.
- Legacy MFC register-window callbacks removed from the portable core.
- Z80's FM-11-specific memory/MMR wiring is deliberately not embedded in the CPU core. The initial portable hook uses the supplied flat `Memory` buffer; board-specific address translation will be added in the CPU-board layer.

No SDL frontend or complete FM-11 machine is implemented yet.

## Headless serial CPU runner (experimental)

On Linux, `fm11-headless` runs an MC6809/HD6309 raw binary with
standard input/output connected to the main RS-232C byte queues.
It is intended for early serial driver bring-up, **not** NitrOS-9 booting:
there is no IPL, disk controller, OS image loader or execution pacing yet.

```sh
./build/fm11-headless --binary program.bin --load 0x1000 --pc 0x1000 --cpu 6809 --clock 2000000
```

Use `--instructions 100000` for a bounded smoke run. Ctrl-C stops an
unbounded run. Input is processed without canonical line buffering when
attached to a TTY; the original terminal settings are restored on exit.

## FM-11 NitrOS-9 boottrack bring-up

The floppy IPL sources in `sueyasu/fm11-nitros9` load the first
stage from track 0, side 0, sector 5 onwards. A raw, **contiguous**
boottrack payload (not a whole floppy image) may be supplied directly:

```sh
./build/fm11-headless --boottrack boottrack.bin --level 1 --cpu 6809
./build/fm11-headless --boottrack boottrack-l2.bin --level 2 --cpu 6809
```

Level 1 expects exactly 17 × 256 bytes at `$EB00`, starting at
`$EB02`. Level 2 expects exactly 21 × 256 bytes at `$E100`,
starting at `$E102`. This bypasses ROM IPL/FDC sector loading only;
it does **not** emulate any subsequent boot-time disk reads. Normal
NitrOS-9 startup therefore requires further disk-controller support.
Use `--instructions N` for bounded diagnostic runs.

## D88 floppy images

The headless runner accepts either a 327680-byte raw 2D image or a D88
2D image via `--floppy`. D88 sectors are selected by their C/H/R IDs,
not by assuming they appear in sector-number order. The initial
read-only MiniFDC implementation requires a complete 40-track, two-sided,
16-sector/side disk with 256-byte sectors and rejects malformed,
missing, duplicated or non-2D sector records.

## Host monitor and serial PTY

The headless emulator starts **halted**. Its original terminal is a
line-oriented monitor, not the guest console. Startup prints a serial
PTY path, e.g. `/dev/pts/5`; connect from another terminal with
`screen /dev/pts/5` (or `picocom`). The PTY slave is configured for
raw 8-bit communication. Guest Ctrl-C passes through the serial PTY
without terminating the emulator.

At the `fm11>` prompt, use `c` to run, `stop` to pause,
`s` to step one instruction, `r` for registers, `m HEXADDR [COUNT]`
for CPU-logical memory, `mp HEXADDR [COUNT]` for physical memory,
`reset` to reset and reload the supplied bootstrap, and `q` to quit.
The monitor is available while the CPU runs. `--instructions` pauses the
CPU at the limit instead of terminating the monitor.


### CPU idle waiting (6809/6309)

The headless runner executes instructions **without a real-time CPU throttle**
while running. Starting with the parallel-PTM branch, the `c` command also
starts the **same `Machine::startRealtimePtm()` worker used by SDL**.
The shared MC6840 timer advances from host `steady_clock` at 9700 Hz,
independently of guest CPU instructions. CPU instruction cycles and the
`SYNC`/`CWAI` wait loop do **not** advance this timer a second time.
When a guest executes `SYNC` or `CWAI`, the host waits using `poll()`
for PTY/monitor activity or the next timer edge. The CPU adapter samples
the PTM's published IRQ level at instruction/wait boundaries.

`CWAI` keeps its pre-stacked interrupt frame; `SYNC` does not stack.
A masked IRQ wakes `SYNC` without entering its vector, while `CWAI`
requires an accepted interrupt. The monitor `stop`/`q` commands stop
and join the timer worker, and `c` restarts it with a fresh host-time
baseline. `reset` while halted resets the shared timer along with the CPU
card. Single-step while halted uses the legacy deterministic cycle clock.
This is **not** a cycle-exact real-time CPU emulation mode.

### Testing without USART Tx interrupts

To diagnose persistent CPU load while NitrOS-9 is waiting for input, launch
`fm11-headless` with `--no-tx-irq`. This temporarily suppresses only the
8251-compatible **transmit** interrupt request; serial transmission, reception
and receive interrupts remain operational. The default behavior is unchanged.
Compare host CPU usage at the Shell prompt with and without the switch.
This is a diagnostic option, not a verified FM-11 hardware correction.

### CPU idle diagnostics

Add `--idle-log idle.csv` to write a CSV sample approximately once per host
second. The samples contain executed instructions, SYNC/CWAI entries, measured
host wait duration for each state (milliseconds), IRQ entries accepted by the
CPU, PTM IRQ assertions, and current PC/CC/wait state. Counts are interval
deltas, not cumulative. Compare samples at the NitrOS-9 shell prompt.

## Boot from a 2D floppy image

With an FM-11 NitrOS-9 bootable raw 2D or D88 floppy image, no separate
boottrack file or OS level is needed:

```sh
./build/fm11-headless --floppy disk.d88 --cpu 6809
```

The FM-11 ROM's initial floppy load is simulated: track 0, side 0,
sectors 1 and 2 are copied to `$0400` and execution starts at `$0400`.
The IPL contained in those sectors then loads the boottrack using the
emulated MiniFDC. This is a disk IPL handoff, not execution of the
original FM-11 system ROM. `--boottrack FILE --level 1|2` remains available.
The monitor's `reset` command reloads the disk IPL from the mounted image.

## 8088/V20 boot-ROM trace (experimental)

The separate diagnostic runner executes an external raw **4096-byte** FM-11
8088 boot-ROM image, beginning at the x86 reset vector FFFF:0000. No ROM
binary is included in this repository. This is a CPU-card diagnostic, not
full FM-11 boot: the remaining hardware and CPU instructions are incomplete.

```sh
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu 8088 --instructions 10000 --trace
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu v20 --instructions 10000
```

The runner prints the first unimplemented opcode with its physical PC and
instruction number; `--trace` logs per-instruction register values. For now
8088 and V20 share the same implemented instruction subset. This tool does
not modify the 6809/6309 headless boot path.


## Experimental parallel CPU/PTM branch

The `dev/parallel-cpu-ptm` branch is an incremental migration; **stage 1
does not start any emulation worker threads**. CPU, display CPU, PTM and
frame rendering still run through the existing single-thread scheduler.

Stage 1 adds `WorkerGroup`, a cooperative lifecycle controller for future
CPU and PTM workers. `start(step)` registers a finite work step;
`pause()` waits until **all** workers have parked at a step boundary;
`resume()` releases them, and `stop()` joins the threads. Worker
exceptions stop the group and can be rethrown on the owner thread via
`rethrowWorkerFailure()`. Do not call `stop()` or destroy the group from
one of its own worker threads, and do not use unbounded blocking steps.

The SDL-to-display-CPU keyboard ring buffer now has a narrow mutex. **This
does not make `DisplaySubsystem` or `MainBoard` generally thread-safe.**
Shared RAM at $9F80, FD05/AFE0 BUSY and HALT latches, main/sub IRQ and
FIRQ state, PTM registers, and VRAM/CRTC access have side effects. These
must be assigned clear owners and access ordering before starting concurrent
CPU/PTM work (stages 2-4). In particular, OpenGL currently reads display
memory directly and requires an immutable render snapshot once the display
worker exists. Do not wrap every RAM access in a global mutex; do not enable
unprotected concurrent guest execution.

Stage 2: SDL and the interactive `fm11-headless` runner use a
**dedicated real-time PTM worker**. SDL starts it after initialization;
headless starts it on monitor `c` and stops it on `stop`/`q`. The MC6840 advances from host `steady_clock` at its 9700 Hz
input rate, regardless of `--speed` or main CPU instruction cycles. PTM
register accesses catch up to the exact host time under a narrow mutex;
a 1 ms worker polling interval supplies asynchronous IRQs even when the
CPU is in SYNC/CWAI. The IRQ level is published atomically; only the CPU
thread changes the CPU adapter's IRQ aggregator. SDL pause parks the PTM
worker and rebases its clock on resume, and reset stops/restarts the worker.

Stage 3: the SDL frontend starts a **separate display sub-CPU worker**.
It advances the display 6809 directly from host `steady_clock` at 2 MHz,
independent of the main CPU and the PTM. The former
`advanceDisplayCpuCycles()` / `advanceDisplayTime()` paths are disabled
while the display worker runs, but remain for deterministic legacy tests.
The SDL SYNC/CWAI path no longer advances guest virtual time. A narrow
display-bus mutex serializes each sub-CPU instruction with main-CPU
mailbox/FD05 operations; rendering holds the same lock while reading the
current VRAM/CRTC state. Display ATTENTION is published through an atomic
event latch. SDL pause/resume controls both workers and rebases the display
clock on resume. Headless and other noninteractive harnesses retain their
existing deterministic display scheduler unless explicitly started.
The PTM worker must be stopped before manipulating or resetting shared
device state directly. This is not yet a fully parallel CPU/display machine;
all other shared state still belongs to the single emulation thread.

Run `ctest --test-dir build --output-on-failure`, including `worker_group`,
`realtime_ptm` and `realtime_display_cpu`. Verify NitrOS-9 boot, shared
mailbox handshakes and wall-time clock on SDL and `fm11-headless`.
Both PTM and display clocks are **independent of** `--speed`; only the
main CPU still uses SDL pacing.

### Stage 4: main CPU worker

SDL no longer executes main 6809/Z80 instructions on its event/render thread.
A dedicated `WorkerGroup` CPU worker owns the existing
`Machine::executeOne()` path, including FD05 CPU-card selection, IRQ
sampling, and the `SdlCpuPacer` clock. `SYNC/CWAI` only wake when their
interrupt conditions are satisfied; the PTM and display workers continue
independently. The main CPU worker services the serial PTY before and after each bounded
instruction batch, so serial input never waits for SDL's rendering loop or
contends for a CPU mutex. A separate narrow main-CPU mutex protects frontend
CPU diagnostics. Display shared RAM/FD05 transactions preserve
the CPU-to-display lock order. F2 pauses all three workers; resume rebases
CPU pacing rather than replaying paused time. Rendering copies display
memory under its own lock, then presents outside the lock.

The deterministic, nonthreaded `Machine::executeOne()` API remains
available to existing headless runners and core regression tests. The
`parallel_cpu_display` test exercises three workers and display snapshots.

### Stage 5: remove the old scheduler from the parallel execution path

The legacy main-CPU-cycle-coupled display scheduler has been extracted from
`Machine.cpp` into `src/core/DeterministicDisplayScheduler.h`. It is retained
only for single-threaded headless utilities and regression tests; while the
display worker runs, CPU instructions and SDL rendering never execute that
scheduler. This preserves deterministic tests without maintaining two display
clocks in the interactive frontend.

The hot path caches primary/optional CPU-card indices at attachment time,
and FD05 writes now update the active CPU selection through a dedicated
callback, independently of diagnostic observers. The active CPU's core pointer,
CPU kind and post-instruction dispatch are cached when the selected board
changes; ordinary instructions no longer perform per-instruction RTTI or
FD05 selection polling. Manual selection, reset, and board attachment
also refresh the cache. SDL's CPU
worker publishes its instruction count once per batch instead of performing
an atomic increment per instruction, and takes the diagnostic CPU-state mutex
only when a frontend speed or CPU diagnostics consumer is enabled. The display
worker amortizes its display-bus lock across at most 16 instructions, retaining
short mailbox/FD05 access opportunities for the main CPU. PTM frequency and
clock semantics are unchanged by these performance changes.

### PTM channel-specific IRQ acknowledgement

PTM status reads now acknowledge each channel's pending expiry independently.
An unrelated timer channel reaching terminal count must not reactivate an
already acknowledged IRQ from another channel. The legacy status latch remains
visible until that channel's counter register is read, preserving existing
software behavior. The `main6809_ptm` regression exercises this specific
cross-channel reassertion case. This correction does not alter the 9700 Hz
input clock or the 50 Hz OS clock reload value, and does not by itself establish
why SDL clock time was previously accelerated.

### OpenGL snapshot and display worker speed

SDL `--speed 1..100` applies the multiplier to the main CPU and
display sub-CPU, each starting from its configured 2 MHz hardware clock.
The display sub-CPU is provisionally capped at 8 MHz, so `--speed 4`
and above target the same display clock. With `--speed 0`, the main CPU
runs unlimited while the display sub-CPU remains paced at 8 MHz.
These are pacing targets, not guaranteed achieved throughput. The
headless/deterministic display scheduler remains at its configured
base clock. The PTM remains tied to host steady-clock time.

The OpenGL frontend still captures a coherent copy of display VRAM,
text/ANK data, CRTC state, display control and palette while holding
the display-bus mutex. All OpenGL texture uploads, uniforms, rendering
and buffer swaps take place after the mutex has been released.

### Independent display bus and mailbox locking

The main CPU no longer takes the display-CPU/VRAM recursive mutex when
accessing its 128-byte FC80-FCFF mailbox. The shared bytes are guarded by
a dedicated mutex on both the main-side FC80-FCFF path and the display
CPU's 9F80-9FFF memory accesses. Shared transaction tracing is serialized
under that same mutex. The shared-write revision is atomic.

FD05 BUSY status and the sub-CPU busy latch can be read atomically
without acquiring the display mutex. FD05 writes that change sub-CPU
state still use the display mutex. The wake-observer callback has a
separate lifetime mutex so mailbox writes can wake the display worker
without holding the display lock. Display-CPU instruction slices and
video snapshots retain their existing display mutex.

### Avoid redundant main-CPU PTM IRQ synchronization

The SDL main-6809 worker samples the PTM IRQ before each instruction for
SYNC/CWAI wakeup and interrupt acceptance. It now passes that fact into
`Machine::executeOne()`, which skips the redundant post-instruction PTM
aggregation only while the PTM is in independent real-time mode. Headless
and deterministic stepping retain their post-instruction synchronization,
as do optional CPU-card execution paths. The main-6809 IRQ aggregator
also avoids rebuilding FD03/FD2C when the published PTM IRQ source level
has not changed; explicit IRQ-mask and other-source updates continue to
recalculate their status at the point of the write.

### Main 6809 RAM page lookup optimization

The main MC6809/HD6309 CPU caches the host-memory base address of each
logical 4 KiB RAM page, avoiding repeated MMR context translation and
physical-memory bounds checking on every ordinary RAM access. FD80-FD8F
writes update the affected active page, while FD90 context changes and FD93
MMR enable changes refresh the 16-entry mapping. Reset and replacement of
the shared 1 MiB physical RAM backing also rebuild the cache. The boot-ROM
overlay, 7C00-7FFF window, FC00-FFFF fixed/IO aperture, and direct physical
DMA accesses continue through their existing handlers. The `main6809_mmr`
regression covers page remapping, contexts, enable toggles, window override,
and shared-memory replacement.

## Intel 8088 hardware-reference instruction tests

The standalone `fm11-8088-singlestep` runner can compare
`Main8088Board::Core::executeOne()` with the real-CPU
[SingleStepTests/8088 V2](https://github.com/SingleStepTests/8088)
instruction test suite (external data, not bundled). Build normally, then:

```sh
ctest --test-dir build -R main8088_singlestep_smoke --output-on-failure
git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
git -C external/8088-tests sparse-checkout set v2
python3 tests/run_8088_singlestep.py --opcode 00 --limit 100
python3 tests/run_8088_singlestep.py --all --limit 100
```

See [docs/8088-singlestep.md](docs/8088-singlestep.md) for full-case
execution, grouped ModR/M opcodes, failure reports and prefetch limitations.
 --output-on-failure
```


Linux port and ongoing development of the FM-11 emulator.

This first revision establishes a portable CMake build for the existing 6809/6309 and Z80 CPU cores. The Windows MFC/Direct3D frontend is intentionally not part of the Linux build.

## Optional link-time optimization (LTO)

LTO is **off by default**. To compare the same Release build with and without
interprocedural optimization, use separate build directories:

```sh
cmake -S . -B build-normal -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=OFF
cmake --build build-normal -j$(nproc)
ctest --test-dir build-normal --output-on-failure

cmake -S . -B build-lto -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=ON
cmake --build build-lto -j$(nproc)
ctest --test-dir build-lto --output-on-failure
```

`FM11_ENABLE_LTO=ON` checks whether CMake's C++ IPO support is available and
stops configuration with a diagnostic if it is not. IPO is then enabled for
the emulator core, frontends, and tests. No CPU emulation logic or scheduling
behavior changes. Benchmark `fm11-sdl --speed 0 --cpu-speed --renderer gl`
under identical host conditions to assess whether LTO improves throughput.

## Installed CPU cards and main-CPU DIP selection

`fm11-sdl` reads CPU hardware settings from the first available file:
`--config FILE` (explicit), `~/.config/fm11emu/fm11emu.conf`, or
`/etc/fm11emu.conf`. With no file it defaults to the original 6809-only
configuration. A supplied but invalid file produces a line-numbered error.

Copy `config/fm11emu.conf.example` and edit its values. The independent
`[main_6809]` and `[main_8088]` `installed` values describe physically
fitted cards; `[main] active = 6809|8088` models the FM-11 EX DIP switch.
Selecting either card leaves the other fitted and reset, but inactive.
The `[main_6809] cpu` accepts `6809|6309`; `[main_8088] cpu`
accepts `8088|v20`. All `clock_hz` settings are in Hz.

The separate `[optional_cpu] type = none|z80` controls the optional
Z80 card, selected via FD05 relative to the DIP-selected main card.
The display CPU supports 6809 only at present; a `6309` setting is
rejected rather than silently ignored.

CPU ROM and media file paths remain runtime parameters: `--main-rom`
loads the 6809 card firmware, and `--8088-rom` loads the 8088/V20 card
firmware. Specify the selected card's ROM to start its CPU. For instance:

```sh
./build/fm11-sdl --config config/fm11emu.conf.example \
    --main-rom external/roms/boot6809.bin \
    --8088-rom external/roms/boot8088.bin --speed 0
```

`--speed` remains a runtime multiplier of the configured physical CPU
clock. The independent display CPU still observes its 8 MHz runtime cap.
The old `--z80` and `--z80-clock` flags are replaced by config settings.

## External ROM images (not committed)

Original emulator firmware is supplied as C byte arrays. Extract them once
from the source ZIP or an unpacked `Fm11/` directory:

```sh
python3 tools/extract_roms.py /path/to/fm11_x64.zip
# Or: python3 tools/extract_roms.py /path/to/Fm11/
```

This writes the following files under `external/roms/`, which is excluded
by `.gitignore` and **must not be committed**:

| File | Size | Reference array |
|---|---:|---|
| `boot6809.bin` | 4096 | `BootRom.h:BootRom` |
| `boot8088.bin` | 4096 | `BootRom86.h:BootRom86` |
| `display6809.bin` | 20480 | `SubRom.h:SubRom` |
| `ank.bin` | 4096 | `CharRom.h:AnkRom` |
| `kanji.bin` | 131072 | `KanRom.h:KanRom` |

The final emulator will use ROM dumps supplied by the user from their own
machine, in the same binary file format and directory layout. ROM filenames
and sizes are validated, but ROM contents are never embedded in the binary.
The C++ core offers `loadDisplayRomFile` on `Machine` and
`loadBootRomFile` on the 6809 and 8088 adapters. The diagnostic runner
defaults to `external/roms/display6809.bin`:

```sh
./build/fm11-display-romtrace --instructions 100000
# Or override: --rom /somewhere/display6809.bin
```

The headless OS boottrack diagnostic currently uses a synthetic boot ROM,
not the extracted original boot firmware. The font ROMs are staged for the
later display/keyboard frontend, not consumed by current rendering code.

## SDL2 display frontend (experimental)

Install `libsdl2-dev` on Ubuntu, then reconfigure CMake so it can
discover SDL2. This frontend runs the display subsystem through the shared `Machine`
scheduler and renders VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

To run the main 6809 CPU alongside the display CPU, supply its boot ROM:\n\n```sh\n./build/fm11-sdl --main-rom external/roms/boot6809.bin\n```\n\nThe default display ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
SDL always scales the image by an **integer** factor and centers it on
dark-gray margins with a thin light-gray outline just outside the emulated
screen. Neither the margins nor the outline alter any emulated pixels. At the default 960-pixel window width, the 640-pixel image
is shown at native 1:1 size rather than distorted to 1.5x. Enlarge the
window to at least 1280x400 for a sharp 2x 640x200 display, or 1280x800
for a sharp 2x 640x400 display. Smaller windows clip instead of using
fractional resampling.
`--high` starts in 400-line mode. The SDL frontend defaults to
**`--speed 10`** (ten times real-time emulated CPU/peripheral time).
Use `--speed 1` for real-time FM-11 clocks, `--speed 5` for five times
real-time, or `--speed 0` for unlimited execution (as fast as the host
can manage). Values 0 through 100 are supported as whole-number
multipliers. The hardware clock configuration remains 2 MHz for the
main 6809/display CPU and 4 MHz for the optional Z80 by default;
`--speed` changes the host-time pacing, not those clock settings.
Guest CPU/peripheral clocks remain synchronized relative to each other.
`SYNC/CWAI` advances peripherals at the requested virtual rate while
the main CPU is waiting. The display refresh cadence remains tied to
host time, near 31 frames/s (approximately one frame per 32 ms),
and the SDL renderer does not request VSYNC so rendering does not
block guest CPU execution waiting for a display refresh. `--cycles N` remains the *maximum* number
of primary CPU instructions per SDL loop (default 10000), not a clock
rate. Achieving the requested speed depends on host performance and
graphics/ROM workload. In unlimited mode idle waiting is also unpaced;
expect higher host CPU usage.

The window renderer defaults to `--renderer auto`: it tries the
OpenGL 3.3 core-profile shader renderer first and falls back to the existing
SDL software-composition path if the GPU/context is unavailable. Use
`--renderer gl` to **require** OpenGL (startup reports an error if unsupported),
or `--renderer software` to retain the previous `DisplayBitmap` and
SDL texture path. The chosen backend is printed on startup. The GL
fragment shader decodes all three physical graphics VRAM planes,
the palette, 4 KiB interleaved character/attribute RAM, the ANK ROM,
and the CRTC's latched text start directly from raw integer textures.
It preserves 200/400-line selection, text attributes, reverse video, and
integer pixel scaling without CPU bitmap composition. It requires desktop
OpenGL 3.3; neither the headless emulator nor the portable core/tests
requires OpenGL development headers. Both renderer modes remain
non-VSYNC at the host's ~31 fps repaint cadence. To compare performance,
run the same workload with `--cpu-speed --renderer gl` and with
`--cpu-speed --renderer software` (keeping `--speed` unchanged).

To measure effective main-CPU throughput, pass `--cpu-speed` to `fm11-sdl`.
It reports each second's **executed guest T-states**, wall-clock MHz,
instruction counts, and total guest CPU time per wall second on stderr.
6809 and Z80 are measured separately across FD05 handoffs. For example,
an uninterrupted 4 MHz Z80 at `--speed 1` should approach
**4,000,000 T/s** and **4.000 MHz**; at `--speed 10` the ideal
is **40,000,000 T/s** and **40.000 MHz**. A smaller rate
can indicate host-side slowdown (or guest CPU waiting/switching).
`guest/wall` sums emulated 6809 and Z80 instruction times and divides by
wall time. Interrupt waits (SYNC/CWAI) and other idle periods reduce this
ratio even when the emulator itself is not overloaded. The display sub-CPU
is intentionally excluded. The `target=` field shows the requested
speed multiplier and `guest/wall=` shows the achieved aggregate guest
CPU time per host second (not counting time spent in idle waits).
This diagnostic has no effect unless enabled; it does not adjust
guest clock speeds.

For a black screen while running **original** main and display ROMs, first
inspect the main CPU boot path and shared-bus handshake without changing
emulated memory or acknowledging IRQ/event registers:

```sh
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --trace-boot
```

`--trace-boot` prints the first 64 main 6809 instruction addresses
before execution. The periodic diagnostic line also reports the current
main PC (**decimal**), FD05 sub-CPU control latch, FD04 event latch,
display BUSY byte, number of main-to-sub shared RAM changes, first
shared byte, and display HALT state. All of these readings are
non-destructive; unlike a guest read from FD04, the diagnostic does
not acknowledge the event. Compare changes over multiple reports.
A constant display PC alone does not establish why firmware is waiting.

If the window stays black, use `./build/fm11-sdl --test-pattern` to
verify SDL rendering independently of firmware. This displays eight-color
stripes and does not require a ROM. Normal execution now prints the display
CPU PC, executed instructions, I/O writes, and count of nonzero physical
VRAM bytes every two seconds to stderr. An unchanged PC or zero VRAM count
is useful evidence that the firmware is waiting for main-CPU commands
rather than drawing a frame; the counters do not themselves establish why
it is waiting.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is an early integrated CPU/display viewer, not yet a complete
FM-11 keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

## Main/sub-CPU binary integration test

Before diagnosing original firmware, use the synthetic 6809 programs to
exercise only the main-to-display path. They are generated from opcode
bytes and **contain no original firmware**:

```sh
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_integration build/test-roms/main_test.bin build/test-roms/display_test.bin
ctest --test-dir build -R display_binary_integration --output-on-failure
```

A second, separate regression tests a complete command/acknowledgement
exchange through the same transport. The synthetic main-CPU ROM writes
command `$81` to `$FC80`, the display ROM reads it from `$9F80`,
draws a white pixel, then writes acknowledgement `$55` to `$9F81`.
The main CPU polls `$FC81` and records success as `$66` at
`$FC82`. Run it independently:

```sh
ctest --test-dir build -R display_binary_handshake --output-on-failure
./build/display_binary_integration build/test-roms/main_handshake.bin build/test-roms/display_handshake.bin --handshake
```

This validates bidirectional shared memory and CPU scheduling, **not**
the original FM-11 interrupt/attention protocol; a hardware notification
register is not inferred from this test.

### AFE0 display sub-CPU BUSY/READY (reference SubCpu.cpp)

The original `CSubCpu::SetIoByte($AFE0)` drives the sub-CPU
BUSY/READY handshake: writing bit 0 = 1 signals READY, and writing
bit 0 = 0 signals BUSY. Reading `$AFE0` returns the written control
byte with bit 0 forced to the current READY state. The former Linux
implementation incorrectly treated `$AFF0` as BUSY; the reference
instead sends `$AFF0` writes to a DMA channel register.

```sh
ctest --test-dir build -R 'display_afe0_handshake|display_binary_halt|display_cpu6809' --output-on-failure
```

The display subsystem now models the AFE0 handshake independently of
the AFF0 latch. Main-side **FD05 bit 7 now also reports BUSY** as controlled by
sub-CPU AFE0. An asserted main-side HALT is kept visible until release;
subsequent AFE0 READY clears the bit when HALT is released. Other FD05
bits and the original sub-CPU interrupt wiring still need verification.
The SDL diagnostic `BUSY` reports the AFE0-driven state.

### AFE6 video status (reference SubCpu.cpp)

The display ROM polls bit 7 at `$AFE6`. The original emulator's
`CSubCpu::GetIoByte` **changes status on every read**: after a
`VSyncCount`-driven clear, bits 7 and 2 toggle on subsequent reads
when `VSyncMode` is zero, and bit 5 is set. `VSyncCount` is
decremented as the sub CPU executes instructions and is set to 100
when CRTC register 13 is written. A previously guessed periodic
60Hz/1ms source has been removed.

```sh
ctest --test-dir build -R 'display_binary_raster|display_cpu6809' --output-on-failure
```

The synthetic display ROM exercises the original `TST $AFE6`
branch loops. This fixes the status semantics tested here but does
not yet port the original character-display composition or every
sub-CPU peripheral behavior.

### Display CPU interrupt dispatch and sub-I/O RAM port

The original `SubCpu.cpp::SimONE` checks pending sub IRQ, the `AFE4`
FIRQ latch and a periodic NMI after the first `AFE0` write. The
display 6809 adapter now invokes `SimIRQ`, `SimFIRQ` and `SimNMI`
before executing its next instruction, with the original initial
NMI threshold of 60000 and a CPU-cycle-based clock. Note: the
reference increments its clock for each memory access and adjusts
the NMI threshold dynamically against wall time; the Linux timing
model is thus **not yet cycle-equivalent**.

The original `GetMemByte/SetMemByte` also accesses sub RAM in
`A000-AFFF` except explicitly decoded registers. Linux previously
returned `FF` for much of that aperture. Ordinary sub I/O working
RAM is now readable and writable there; reading `AFE5` acknowledges
the sub IRQ request as in original `GetIoByte`. Tests exercise RAM
readback, reset, IRQ acknowledgment and NMI reset-period defaults.

```sh
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

This does **not** assert that real display ROM boots: sub IRQ request
generation from the main-side `FD05` bit6 and original `105`
ready/interrupt gating are still to be wired, and keyboard host input
and timing require additional integration.

### Sub-CPU keyboard event queue (source SubCpu.cpp)

The original Windows emulator's first `AFE0` write queues key event
`0x0050` and clears its NMI mask. Subsequently, `GetKeyCode`
copies pending keyboard events into `AFEC/AFED`, asserting bit 0 of
`AFE4` for ordinary key codes. Reading `AFED` invokes
`GetKeyCode` to advance the queue.

The Linux subsystem now includes `enqueueKeyCode`, `processKeyCode`,
the one-shot initial `AFE0` key event and the `AFED` queue advancement.
The independent FIFO regression checks these state transitions:

```sh
ctest --test-dir build -R 'display_keyboard_queue|display_io_read_clear' --output-on-failure
```

The original implementation also invokes `GetKeyCode` from a timed
keyboard/vsync event and supplies interrupts through `SimFIRQ` and
`SimNMI`. These *CPU-side interrupt delivery and timing paths* remain
unimplemented and are **not claimed** by the new keyboard FIFO test.
The original ROM may therefore still be black.

### Original display I/O read-clear latches

The reference Windows `Fm11/SubCpu.cpp::GetIoByte` clears three
register latches **on read**: `AFE4` (sub FIRQ), `AFEC`
(keyboard status) and `AFED` (keyboard data). The Linux display
subsystem now reproduces those read-clear semantics, including zero
values after reset and ignoring software writes to these registers.
A regression supplies external latch values using
`signalSubFirq` / `setKeyboardLatches` and verifies one-shot reads.

```sh
ctest --test-dir build -R 'display_io_read_clear|display_cpu6809' --output-on-failure
```

This is **not yet** the whole original keyboard/NMI implementation:
original `GetKeyCode()` processing after `AFED`, NMI timing, and
interrupt delivery remain separate work. Do not interpret passing
this test as proof that original display firmware now boots.

### Stage 10: original display ROM versus synthetic main sender

The stage-8 synthetic main ROM is kept as a **known, deterministic sender**.
Unlike stage 8, however, the sub CPU runs the user's **unmodified
original 20 KiB ROM**. This isolates genuine firmware startup/READY
handling from the main boot ROM and from the synthetic sub-CPU wrapper.

```sh
git pull
cmake --build build -j$(nproc)
python3 tools/make_display_test_roms.py build/test-roms
./build/display_real_rom_probe \\
    build/test-roms/main_text_real_protocol.bin \\
    external/roms/display6809.bin
```

The probe records up to 32 original-sub-CPU AFE0 writes with the
instruction PC, values, main FD05 status, READY/BUSY totals and
the busiest sub-CPU PCs. It also prints FC81/FC82/FC83/FC84 and
FDA0 after a bounded instruction budget. A zero exit code means
**the diagnostic completed**, not that the original firmware has
accepted the stage-8 test command.

The stage-8 main ROM's request format is only a controlled stimulus:
there is **no assertion** that it is the real firmware character
command. Do not use this probe's output to claim the original text
command works. This program intentionally is not a CTest because it
requires the operator's original display ROM. Once the firmware's
own command parsing is established, write a genuine command sender
and its regression test rather than inventing any new protocol.

### Stage 9: audit actual ROM display-command dispatch (preparation)

Do **not** treat the stage-8 `CAA0` byte-receive routine as proof of the
real firmware's text *command* semantics. The reference `SubRom.h`
begins with `B000: JMP B018`, and the ROM contains eight handler
addresses at `B07C-B08B`. Those are actual firmware entry/dispatch
structures, unlike the synthetic byte `A5` used in stage 7.

The first step is a reproducible, read-only audit of the operator's
original display ROM:

```sh
python3 tools/inspect_display_rom.py external/roms/display6809.bin
```

The tool validates the ROM size and startup signature, prints the eight
handler targets and the genuine `CAA0` bytes, and **does not infer**
unknown display command codes. Next, trace the original main-ROM sender
and the `B018/B08C` command decoding and identify the actual character
output handler before producing the stage-9 synthetic-ROM test.
No new communication behavior or firmware-specific display test is
claimed by this inspection-only commit.

### Original SubRom receive handshake: stage 8

This stage does **not invent a new receive protocol**. The generated
display ROM includes, byte-for-byte, the receive subroutine at
`$CAA0-$CACA` from the reference `Fm11/SubRom.h`, relocated
unaltered to `$B100` (all its branches are relative).

The reference `Fm11/SubCpu.cpp` maps the display CPU's `$9F80-$9FFF`
to main shared RAM `$FC80-$FCFF`. Its `$AFE0` bit 0 sets or
clears main-side `$FD05` bit 7.

- The original subroutine signals READY through `AFE0`, polls
  `9F82`, signals BUSY, clears bit 7 in `9F81`, then returns
  the payload from `9F82`.
- The synthetic main ROM waits for READY at `FD05`, writes a
  request with bit 7 set to `FC81` and ASCII `M` to `FC82`,
  and waits until the genuine subroutine clears the request bit.
- A **test-only wrapper**, not original firmware, writes the
  returned character to text RAM and signals display completion
  through `9F83`, after which main enables `FDA0=03`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_real_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_real_protocol.bin \\
    --rom build/test-roms/display_text_real_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected: a white `M` at the screen origin. This validates the
**original receive-handshake routine**, but not yet the actual
original firmware's display-command dispatcher or its character
command encoding. The real boot ROM is still not used in this test.

### Synthetic main-to-sub 6809 text request: stage 7

This is the first test where the **main 6809 requests text via shared
RAM**, rather than the display CPU unconditionally painting a predefined
message.

1. Sub 6809 initializes CRTC then writes `AFE0=1` (READY).
2. Main 6809 waits for `FD05 bit7=0`, writes command `A5` to
   `FC80` and ASCII `M` to `FC81`.
3. Sub 6809 reads the matching shared addresses `9F80/9F81`, copies
   the requested character into its text RAM and acknowledges by writing
   `9F82=66`.
4. Main 6809 detects the acknowledgement, enables text using
   `FDA0=03`, and sets a completion marker `FC83=77`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_protocol.bin \\
    --rom build/test-roms/display_text_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected SDL display: a white `M` at the upper-left of the screen.
The regression checks the READY transition, both CPUs' shared
request/acknowledgement, text/CRTC state, and 200/400-line pixels.
This is a deliberately minimal *test protocol*, not yet the original
firmware's command encoding. Original ROM boot is not part of this stage.

### Synthetic 6809 CRTC display-start scrolling: stage 6

Two sub-CPU ROMs write **identical** `FIRST LINE` and `SECOND LINE`
text into row 0 and row 1 of the 80-column text RAM. The first
ROM sets CRTC register 13 to 0; the second sets it to 80 text
cells (one full row). The main CPU ROM enables text as in stage 1.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_scroll --output-on-failure

# First row shows FIRST LINE, second row SECOND LINE:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_top.bin \\
    --ank-rom external/roms/ank.bin --high

# After CRTC start-address change, SECOND LINE moves to the first row:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_scroll.bin \\
    --ank-rom external/roms/ank.bin --high
```

This stage checks both 200- and 400-line framebuffer results with
generated distinguishable glyphs and verifies that the **same**
physical text RAM contents are present in both variants. It tests
the current CRTC text-start mapping, not original firmware boot.

### Synthetic 6809 display-layer selection: stage 5

This test makes the graphics region **large enough to see**.
The common display ROM writes yellow text `LAYER TEST` plus a 640x200
blue/black alternating-pixel stripe field, and the main ROM selects
which layers are enabled via `FDA0`:

| Main ROM | FDA0 | Expected appearance |
| --- | --- | --- |
| `main_layers_text.bin` | `03` | Yellow text on black |
| `main_layers_graphics.bin` | `0C` | Blue/black stripes across the screen, no text |
| `main_layers_both.bin` | `0F` | Yellow text over blue/black stripes |

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_layers --output-on-failure

# Change 'both' to 'text' or 'graphics' for the other two modes:
./build/fm11-sdl --main-rom build/test-roms/main_layers_both.bin \\
    --rom build/test-roms/display_layers.bin \\
    --ank-rom external/roms/ank.bin
```

The three new tests check actual main/sub-6809 writes and pixels for
both 200- and 400-line scanout. The 400-line view includes the
graphics plane's upper area, which is still blank because the test
draws only the first 200 scanlines.

### Synthetic 6809 graphics/text overlay: stage 4

This stage uses a **new pair of synthetic 6809 ROMs**. The main CPU
enables both text and graphics (`FDA0=$0F`). The display CPU sets
80-column/25-row CRTC text mode, writes a yellow `T` and a reverse
red `R` into text RAM, and paints two blue graphics pixels underneath
their cells using the graphics plane registers.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_overlay --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_overlay.bin \\
    --rom build/test-roms/display_text_overlay.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest runs the same ROMs with a generated minimal ANK font, and
checks both 200- and 400-line frames. The `T` glyph overlays the first
blue pixel in yellow, while the reverse-video `R` leaves the second
blue pixel visible through the glyph hole and paints the other pixels
of that character cell red. This tests actual output blending rather
than simply checking whether the two memory regions were written.

### Synthetic 6809 text attributes: stage 3

After validating basic text and 40/80-column geometry, the next
self-contained 6809 ROM writes six text cells with distinct attributes:

- `R`, `G`, `B`: red, green and blue character colors.
- `R` (fourth cell): reversed red, with the cell background filled.
- `H`: bright-white attribute.
- `W`: normal-white attribute.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_attributes --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_attributes.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest uses a generated minimal ANK glyph ROM, checking the text
attribute bytes written by the sub 6809 and the composited pixel colors
in both 200- and 400-line views. The SDL command uses the actual
extracted ANK ROM to show the characters.

F1 only changes the *200/400-line view*, not the CRTC 40/80-column
setting. In the current rendering model the 200-line view downsamples
400 logical character scanlines, so text appears vertically compressed.
Fidelity to original hardware scanline behavior remains to be verified.

### Synthetic 6809 text geometry: 40/80 columns and 200/400 lines

Stage 2 reuses the working stage-1 main ROM and two new display
6809 ROMs. The display CPU sets CRTC R1 to 20 (40 columns) or
40 (80 columns), R6 to 25 rows and writes `40/80 COL TEST`
into text RAM. The tests check both 640x200 and 640x400 framebuffer
geometry, using a deterministic test-only ANK font.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R 'display_binary_text_(40|80)col' --output-on-failure

# 40 columns (16 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_40col.bin \\
    --ank-rom external/roms/ank.bin --high

# 80 columns (8 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_80col.bin \\
    --ank-rom external/roms/ank.bin --high
```

Press F1 in SDL to compare the 200- and 400-line representations.
The expected 40-column letters are twice as wide as the 80-column
letters. This stage confirms the current geometry model; fidelity
to all original CRTC/scanline behavior must still be compared with
the reference emulator.

### Synthetic 6809 text display: stage 1

Start with a small **end-to-end** text test before original firmware. Both
processors run real 6809 binary instructions: the main CPU writes `$FDA0=3`
(text enabled); the display CPU initializes the CRTC to 80x25, writes
`HELLO FM-11` as alternating character/attribute bytes starting at
`$8000`, and places `$66` in shared RAM on completion.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_stage1 --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_stage1.bin \\
    --ank-rom external/roms/ank.bin --high
```

The headless regression uses a separate generated font
`ank_text_stage1.bin` with simple known glyph pixels, and checks
CPU-written text RAM, CRTC values, and output pixels. The SDL run above
uses the **actual** extracted ANK ROM, so `HELLO FM-11` should appear
at the top left. If the original ANK ROM is not available, the same SDL
command can use `build/test-roms/ank_text_stage1.bin` to show simple
glyph marks rather than letters.

This is **stage 1 only**: plain text on black. After confirming its
SDL result, later stages will cover colors, reverse attributes and
graphics/text overlays separately.

### Reference ANK text composition (first implementation)

The original `Fm11/fm11.hlsl` composites two-byte character/attribute
cells from the sub-CPU text RAM, with the CRTC start address and 8x16
ANK glyphs. That operation is now available in the SDL frontend, including
text reverse video, color attributes, and the 640x200/400 output sizes.
The software scanout matches the shader's LSB-first glyph bit ordering.

The reference archive includes `Fm11/CharRom.h`; convert its embedded
4096-byte ANK array without committing those font bytes:

```sh
python3 tools/extract_ank_rom.py /path/to/Fm11/CharRom.h external/roms/ank.bin
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --ank-rom external/roms/ank.bin
ctest --test-dir build -R display_text_composition --output-on-failure
```

Providing `--ank-rom` loads font data; it does **not** force the
text layer on when a main CPU is present. The original display control
register `$FDA0` enables text with bits 0/1 and graphics with bits
2/3. The renderer also applies the eight programmable palette entries
from `$FD98-$FD9F` and the original 16-entry color table.
The current software scanout supports attribute bit 5 (bright/bold)
and reverse video; blink, cursor and clip state remain future work.
Without a main CPU, specifying `--ank-rom` still explicitly enables
text for standalone diagnostics.
Other original display behavior, including blink, cursor and clip, remains to be ported.
Without `--ank-rom`, the established graphics-only regression frames
remain unchanged.

### Full-frame binary display test and SDL preview

To check the complete raster path without original firmware, generate
the synthetic ROM pair and run the test:

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_pattern --output-on-failure
./build/display_binary_pattern build/test-roms/main_pattern.bin build/test-roms/display_pattern.bin
```

The main 6809 sends a command through shared RAM; the display 6809
writes all 16,000 bytes of the 640x200 plane with an alternating pixel
pattern in all three colors, then acknowledges completion. The test
checks **every rendered pixel**, not just the first byte.

To inspect the same pattern visually in the SDL frontend:

```sh
./build/fm11-sdl --main-rom build/test-roms/main_pattern.bin \\
    --rom build/test-roms/display_pattern.bin
```

This should display narrow alternating white and black vertical lines
across the entire 640x200 framebuffer. It is a deliberately artificial
bring-up image, not the FM-11 firmware's normal boot screen.

### Display-generated ATTENTION (original I/O semantics)

Original `Fm11/SubCpu.cpp`, `CSubCpu::GetIoByte`, case
`$AFE7`, raises main-CPU ATTENTION when the display CPU **reads**
that address. This is distinct from the earlier host-injected event
tests. The implementation now connects that read to the existing main
`$FD04` event latch.

A pair of tiny 6809 binaries verifies that the display CPU's `LDA
$AFE7` triggers an event that the main CPU receives with `LDA
$FD04` and then acknowledges by reading `$FD04` again:

```sh
ctest --test-dir build -R display_binary_afe7 --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_afe7 build/test-roms/main_afe7.bin build/test-roms/display_afe7.bin
```

### HALT and BUSY binary regression

```sh
ctest --test-dir build -R display_binary_halt --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_halt build/test-roms/main_halt.bin build/test-roms/display_halt.bin
```

This runs real, standalone MC6809 binary instructions on both CPU
cores: main writes `$FD05` to halt the display CPU, writes a marker to
shared `$FC80`, and releases HALT. The test checks that the display
PC remains frozen during HALT; after release, the display ROM sets
`$AFF0` (BUSY) and writes a shared-RAM marker at `$9F82`.
There is no dependency on an original ROM dump.

ATTENTION's sub-CPU hardware trigger is still not mapped. The existing
`MainBoard::signalAttention()` path does **not** establish how an
actual display ROM should raise the event; this test deliberately does
not fabricate one.

The main CPU program at `$5000` writes `$80` to `$FC80`.
The display CPU program at `$B000` waits for the same value at
`$9F80`, then writes `$80` to VRAM through the three color planes.
The test asserts shared RAM, physical VRAM and the first rendered white
pixel. CTest creates temporary binaries automatically; the manual commands
above keep them under `build/` for inspection.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current scope

- Portable fixed-width compatibility types for legacy `BYTE`, `WORD`, `DWORD`, and `LONG` usage.
- Existing `Sim09` 6809/6309 instruction core compiled as portable C++.
- Existing `SimZ80` instruction core compiled as portable C++.
- Legacy MFC register-window callbacks removed from the portable core.
- Z80's FM-11-specific memory/MMR wiring is deliberately not embedded in the CPU core. The initial portable hook uses the supplied flat `Memory` buffer; board-specific address translation will be added in the CPU-board layer.

No SDL frontend or complete FM-11 machine is implemented yet.

## Headless serial CPU runner (experimental)

On Linux, `fm11-headless` runs an MC6809/HD6309 raw binary with
standard input/output connected to the main RS-232C byte queues.
It is intended for early serial driver bring-up, **not** NitrOS-9 booting:
there is no IPL, disk controller, OS image loader or execution pacing yet.

```sh
./build/fm11-headless --binary program.bin --load 0x1000 --pc 0x1000 --cpu 6809 --clock 2000000
```

Use `--instructions 100000` for a bounded smoke run. Ctrl-C stops an
unbounded run. Input is processed without canonical line buffering when
attached to a TTY; the original terminal settings are restored on exit.

## FM-11 NitrOS-9 boottrack bring-up

The floppy IPL sources in `sueyasu/fm11-nitros9` load the first
stage from track 0, side 0, sector 5 onwards. A raw, **contiguous**
boottrack payload (not a whole floppy image) may be supplied directly:

```sh
./build/fm11-headless --boottrack boottrack.bin --level 1 --cpu 6809
./build/fm11-headless --boottrack boottrack-l2.bin --level 2 --cpu 6809
```

Level 1 expects exactly 17 × 256 bytes at `$EB00`, starting at
`$EB02`. Level 2 expects exactly 21 × 256 bytes at `$E100`,
starting at `$E102`. This bypasses ROM IPL/FDC sector loading only;
it does **not** emulate any subsequent boot-time disk reads. Normal
NitrOS-9 startup therefore requires further disk-controller support.
Use `--instructions N` for bounded diagnostic runs.

## D88 floppy images

The headless runner accepts either a 327680-byte raw 2D image or a D88
2D image via `--floppy`. D88 sectors are selected by their C/H/R IDs,
not by assuming they appear in sector-number order. The initial
read-only MiniFDC implementation requires a complete 40-track, two-sided,
16-sector/side disk with 256-byte sectors and rejects malformed,
missing, duplicated or non-2D sector records.

## Host monitor and serial PTY

The headless emulator starts **halted**. Its original terminal is a
line-oriented monitor, not the guest console. Startup prints a serial
PTY path, e.g. `/dev/pts/5`; connect from another terminal with
`screen /dev/pts/5` (or `picocom`). The PTY slave is configured for
raw 8-bit communication. Guest Ctrl-C passes through the serial PTY
without terminating the emulator.

At the `fm11>` prompt, use `c` to run, `stop` to pause,
`s` to step one instruction, `r` for registers, `m HEXADDR [COUNT]`
for CPU-logical memory, `mp HEXADDR [COUNT]` for physical memory,
`reset` to reset and reload the supplied bootstrap, and `q` to quit.
The monitor is available while the CPU runs. `--instructions` pauses the
CPU at the limit instead of terminating the monitor.


### CPU idle waiting (6809/6309)

The headless runner executes instructions **without a real-time CPU throttle**
while running. Starting with the parallel-PTM branch, the `c` command also
starts the **same `Machine::startRealtimePtm()` worker used by SDL**.
The shared MC6840 timer advances from host `steady_clock` at 9700 Hz,
independently of guest CPU instructions. CPU instruction cycles and the
`SYNC`/`CWAI` wait loop do **not** advance this timer a second time.
When a guest executes `SYNC` or `CWAI`, the host waits using `poll()`
for PTY/monitor activity or the next timer edge. The CPU adapter samples
the PTM's published IRQ level at instruction/wait boundaries.

`CWAI` keeps its pre-stacked interrupt frame; `SYNC` does not stack.
A masked IRQ wakes `SYNC` without entering its vector, while `CWAI`
requires an accepted interrupt. The monitor `stop`/`q` commands stop
and join the timer worker, and `c` restarts it with a fresh host-time
baseline. `reset` while halted resets the shared timer along with the CPU
card. Single-step while halted uses the legacy deterministic cycle clock.
This is **not** a cycle-exact real-time CPU emulation mode.

### Testing without USART Tx interrupts

To diagnose persistent CPU load while NitrOS-9 is waiting for input, launch
`fm11-headless` with `--no-tx-irq`. This temporarily suppresses only the
8251-compatible **transmit** interrupt request; serial transmission, reception
and receive interrupts remain operational. The default behavior is unchanged.
Compare host CPU usage at the Shell prompt with and without the switch.
This is a diagnostic option, not a verified FM-11 hardware correction.

### CPU idle diagnostics

Add `--idle-log idle.csv` to write a CSV sample approximately once per host
second. The samples contain executed instructions, SYNC/CWAI entries, measured
host wait duration for each state (milliseconds), IRQ entries accepted by the
CPU, PTM IRQ assertions, and current PC/CC/wait state. Counts are interval
deltas, not cumulative. Compare samples at the NitrOS-9 shell prompt.

## Boot from a 2D floppy image

With an FM-11 NitrOS-9 bootable raw 2D or D88 floppy image, no separate
boottrack file or OS level is needed:

```sh
./build/fm11-headless --floppy disk.d88 --cpu 6809
```

The FM-11 ROM's initial floppy load is simulated: track 0, side 0,
sectors 1 and 2 are copied to `$0400` and execution starts at `$0400`.
The IPL contained in those sectors then loads the boottrack using the
emulated MiniFDC. This is a disk IPL handoff, not execution of the
original FM-11 system ROM. `--boottrack FILE --level 1|2` remains available.
The monitor's `reset` command reloads the disk IPL from the mounted image.

## 8088/V20 boot-ROM trace (experimental)

The separate diagnostic runner executes an external raw **4096-byte** FM-11
8088 boot-ROM image, beginning at the x86 reset vector FFFF:0000. No ROM
binary is included in this repository. This is a CPU-card diagnostic, not
full FM-11 boot: the remaining hardware and CPU instructions are incomplete.

```sh
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu 8088 --instructions 10000 --trace
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu v20 --instructions 10000
```

The runner prints the first unimplemented opcode with its physical PC and
instruction number; `--trace` logs per-instruction register values. For now
8088 and V20 share the same implemented instruction subset. This tool does
not modify the 6809/6309 headless boot path.


## Experimental parallel CPU/PTM branch

The `dev/parallel-cpu-ptm` branch is an incremental migration; **stage 1
does not start any emulation worker threads**. CPU, display CPU, PTM and
frame rendering still run through the existing single-thread scheduler.

Stage 1 adds `WorkerGroup`, a cooperative lifecycle controller for future
CPU and PTM workers. `start(step)` registers a finite work step;
`pause()` waits until **all** workers have parked at a step boundary;
`resume()` releases them, and `stop()` joins the threads. Worker
exceptions stop the group and can be rethrown on the owner thread via
`rethrowWorkerFailure()`. Do not call `stop()` or destroy the group from
one of its own worker threads, and do not use unbounded blocking steps.

The SDL-to-display-CPU keyboard ring buffer now has a narrow mutex. **This
does not make `DisplaySubsystem` or `MainBoard` generally thread-safe.**
Shared RAM at $9F80, FD05/AFE0 BUSY and HALT latches, main/sub IRQ and
FIRQ state, PTM registers, and VRAM/CRTC access have side effects. These
must be assigned clear owners and access ordering before starting concurrent
CPU/PTM work (stages 2-4). In particular, OpenGL currently reads display
memory directly and requires an immutable render snapshot once the display
worker exists. Do not wrap every RAM access in a global mutex; do not enable
unprotected concurrent guest execution.

Stage 2: SDL and the interactive `fm11-headless` runner use a
**dedicated real-time PTM worker**. SDL starts it after initialization;
headless starts it on monitor `c` and stops it on `stop`/`q`. The MC6840 advances from host `steady_clock` at its 9700 Hz
input rate, regardless of `--speed` or main CPU instruction cycles. PTM
register accesses catch up to the exact host time under a narrow mutex;
a 1 ms worker polling interval supplies asynchronous IRQs even when the
CPU is in SYNC/CWAI. The IRQ level is published atomically; only the CPU
thread changes the CPU adapter's IRQ aggregator. SDL pause parks the PTM
worker and rebases its clock on resume, and reset stops/restarts the worker.

Stage 3: the SDL frontend starts a **separate display sub-CPU worker**.
It advances the display 6809 directly from host `steady_clock` at 2 MHz,
independent of the main CPU and the PTM. The former
`advanceDisplayCpuCycles()` / `advanceDisplayTime()` paths are disabled
while the display worker runs, but remain for deterministic legacy tests.
The SDL SYNC/CWAI path no longer advances guest virtual time. A narrow
display-bus mutex serializes each sub-CPU instruction with main-CPU
mailbox/FD05 operations; rendering holds the same lock while reading the
current VRAM/CRTC state. Display ATTENTION is published through an atomic
event latch. SDL pause/resume controls both workers and rebases the display
clock on resume. Headless and other noninteractive harnesses retain their
existing deterministic display scheduler unless explicitly started.
The PTM worker must be stopped before manipulating or resetting shared
device state directly. This is not yet a fully parallel CPU/display machine;
all other shared state still belongs to the single emulation thread.

Run `ctest --test-dir build --output-on-failure`, including `worker_group`,
`realtime_ptm` and `realtime_display_cpu`. Verify NitrOS-9 boot, shared
mailbox handshakes and wall-time clock on SDL and `fm11-headless`.
Both PTM and display clocks are **independent of** `--speed`; only the
main CPU still uses SDL pacing.

### Stage 4: main CPU worker

SDL no longer executes main 6809/Z80 instructions on its event/render thread.
A dedicated `WorkerGroup` CPU worker owns the existing
`Machine::executeOne()` path, including FD05 CPU-card selection, IRQ
sampling, and the `SdlCpuPacer` clock. `SYNC/CWAI` only wake when their
interrupt conditions are satisfied; the PTM and display workers continue
independently. The main CPU worker services the serial PTY before and after each bounded
instruction batch, so serial input never waits for SDL's rendering loop or
contends for a CPU mutex. A separate narrow main-CPU mutex protects frontend
CPU diagnostics. Display shared RAM/FD05 transactions preserve
the CPU-to-display lock order. F2 pauses all three workers; resume rebases
CPU pacing rather than replaying paused time. Rendering copies display
memory under its own lock, then presents outside the lock.

The deterministic, nonthreaded `Machine::executeOne()` API remains
available to existing headless runners and core regression tests. The
`parallel_cpu_display` test exercises three workers and display snapshots.

### Stage 5: remove the old scheduler from the parallel execution path

The legacy main-CPU-cycle-coupled display scheduler has been extracted from
`Machine.cpp` into `src/core/DeterministicDisplayScheduler.h`. It is retained
only for single-threaded headless utilities and regression tests; while the
display worker runs, CPU instructions and SDL rendering never execute that
scheduler. This preserves deterministic tests without maintaining two display
clocks in the interactive frontend.

The hot path caches primary/optional CPU-card indices at attachment time,
and FD05 writes now update the active CPU selection through a dedicated
callback, independently of diagnostic observers. The active CPU's core pointer,
CPU kind and post-instruction dispatch are cached when the selected board
changes; ordinary instructions no longer perform per-instruction RTTI or
FD05 selection polling. Manual selection, reset, and board attachment
also refresh the cache. SDL's CPU
worker publishes its instruction count once per batch instead of performing
an atomic increment per instruction, and takes the diagnostic CPU-state mutex
only when a frontend speed or CPU diagnostics consumer is enabled. The display
worker amortizes its display-bus lock across at most 16 instructions, retaining
short mailbox/FD05 access opportunities for the main CPU. PTM frequency and
clock semantics are unchanged by these performance changes.

### PTM channel-specific IRQ acknowledgement

PTM status reads now acknowledge each channel's pending expiry independently.
An unrelated timer channel reaching terminal count must not reactivate an
already acknowledged IRQ from another channel. The legacy status latch remains
visible until that channel's counter register is read, preserving existing
software behavior. The `main6809_ptm` regression exercises this specific
cross-channel reassertion case. This correction does not alter the 9700 Hz
input clock or the 50 Hz OS clock reload value, and does not by itself establish
why SDL clock time was previously accelerated.

### OpenGL snapshot and display worker speed

SDL `--speed 1..100` applies the multiplier to the main CPU and
display sub-CPU, each starting from its configured 2 MHz hardware clock.
The display sub-CPU is provisionally capped at 8 MHz, so `--speed 4`
and above target the same display clock. With `--speed 0`, the main CPU
runs unlimited while the display sub-CPU remains paced at 8 MHz.
These are pacing targets, not guaranteed achieved throughput. The
headless/deterministic display scheduler remains at its configured
base clock. The PTM remains tied to host steady-clock time.

The OpenGL frontend still captures a coherent copy of display VRAM,
text/ANK data, CRTC state, display control and palette while holding
the display-bus mutex. All OpenGL texture uploads, uniforms, rendering
and buffer swaps take place after the mutex has been released.

### Independent display bus and mailbox locking

The main CPU no longer takes the display-CPU/VRAM recursive mutex when
accessing its 128-byte FC80-FCFF mailbox. The shared bytes are guarded by
a dedicated mutex on both the main-side FC80-FCFF path and the display
CPU's 9F80-9FFF memory accesses. Shared transaction tracing is serialized
under that same mutex. The shared-write revision is atomic.

FD05 BUSY status and the sub-CPU busy latch can be read atomically
without acquiring the display mutex. FD05 writes that change sub-CPU
state still use the display mutex. The wake-observer callback has a
separate lifetime mutex so mailbox writes can wake the display worker
without holding the display lock. Display-CPU instruction slices and
video snapshots retain their existing display mutex.

### Avoid redundant main-CPU PTM IRQ synchronization

The SDL main-6809 worker samples the PTM IRQ before each instruction for
SYNC/CWAI wakeup and interrupt acceptance. It now passes that fact into
`Machine::executeOne()`, which skips the redundant post-instruction PTM
aggregation only while the PTM is in independent real-time mode. Headless
and deterministic stepping retain their post-instruction synchronization,
as do optional CPU-card execution paths. The main-6809 IRQ aggregator
also avoids rebuilding FD03/FD2C when the published PTM IRQ source level
has not changed; explicit IRQ-mask and other-source updates continue to
recalculate their status at the point of the write.

### Main 6809 RAM page lookup optimization

The main MC6809/HD6309 CPU caches the host-memory base address of each
logical 4 KiB RAM page, avoiding repeated MMR context translation and
physical-memory bounds checking on every ordinary RAM access. FD80-FD8F
writes update the affected active page, while FD90 context changes and FD93
MMR enable changes refresh the 16-entry mapping. Reset and replacement of
the shared 1 MiB physical RAM backing also rebuild the cache. The boot-ROM
overlay, 7C00-7FFF window, FC00-FFFF fixed/IO aperture, and direct physical
DMA accesses continue through their existing handlers. The `main6809_mmr`
regression covers page remapping, contexts, enable toggles, window override,
and shared-memory replacement.

## Intel 8088 hardware-reference instruction tests

The standalone `fm11-8088-singlestep` runner can compare
`Main8088Board::Core::executeOne()` with the real-CPU
[SingleStepTests/8088 V2](https://github.com/SingleStepTests/8088)
instruction test suite (external data, not bundled). Build normally, then:

```sh
ctest --test-dir build -R main8088_singlestep_smoke --output-on-failure
git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
git -C external/8088-tests sparse-checkout set v2
python3 tests/run_8088_singlestep.py --opcode 00 --limit 100
python3 tests/run_8088_singlestep.py --all --limit 100
```

See [docs/8088-singlestep.md](docs/8088-singlestep.md) for full-case
execution, grouped ModR/M opcodes, failure reports and prefetch limitations.
 --output-on-failure
ctest --test-dir build --output-on-failure
```

GUI-specific integration remains a manual smoke test because automated
desktop window-manager behavior is not reproducible in headless CTest:
on both OpenGL and software SDL paths, check independent file picker
geometry and notifications, FDD insert/eject and guest persistence, and
Apply → close → Reset with 128/512/1024 KiB RAM and CPU selections.
Also verify the original running machine survives a rejected
configuration and an invalid image selection.

## Stage 4-C UI: Apply stages settings and closes the editor

The Settings editor's **Apply** button validates and stages the edited
hardware configuration for the next **Settings > Reset machine**. On
successful Apply the Settings editor closes immediately; the guest does not
reset or otherwise change until Reset is requested. An invalid configuration
keeps the editor open and reports the failure.

Clicking **Close**, the window manager's **X**, or toggling the editor off
with **Configuration... / Ctrl+F10** discards any changes not yet applied.
Reopening the editor starts from the last applied configuration, not from
the abandoned form. **Discard edits** also restores that applied baseline
without closing the window. A previously staged Apply survives cancelling a
later editing session and remains pending for Reset.

**Save user configuration** is independent: it writes the edited values
to the XDG file but does not Apply or Reset them. Live FDD exchanges remain
immediate and update the matching displayed/staged drive paths.

## Stage 4-C: GUI reset and apply configuration

The SDL **Settings** menu now has two separate commands:

- **Reset machine**: reset the current emulator hardware, keeping its
  current CPU cards, RAM, ROM selections and live-mounted disk images.
  Pending edits are **not applied**.
- **Apply settings & Reset**: use all currently edited fields from the
  Settings dialog to construct a new Machine (CPU cards, 6809/6309/V20,
  memory, display CPU and clocks, boot ROMs, DIP switches, serial/PTM
  timing, HDD and FDD image paths). The existing SDL CRT, renderer,
  menus and independent settings/file-picker windows are retained.

The Settings editor provides matching **Reset only** and
**Apply & Reset** buttons. The running CPU worker, display CPU and
real-time PTM are parked before a normal reset, and fully stopped before
machine reconstruction. Previously paused emulation remains paused;
previously running emulation resumes. Serial and audio backends are
rebound to the newly configured machine.

Apply is deliberately **separate from Save user configuration**: the
user's XDG INI file is not modified until Save is explicitly selected.
The currently running image paths are also preserved on a plain Reset,
including FDDs hot-swapped via Stage 4-B. A new Apply & Reset mounts
the image paths from the edited GUI state.

Before reconstructing, the frontend synchronizes outstanding host disk
writes. If validation or new-machine setup fails, it restores the
previous machine, configuration, mounted images and worker execution
state rather than discarding the running guest. An error dialog
explains the failure. CPU debugger hooks are reattached to a new
Machine; CPU debug breakpoints are not automatically migrated to the
new machine after an Apply & Reset.

The `worker_group` regression additionally checks explicit stop,
rearm and restart, including parking the new worker. Run all CTest
tests and smoke-test both `--renderer gl` and
`--renderer software` with NitrOS-9, CP/M-80 and CP/M-86 boot disks.
Suggested manual check: first use Reset machine with a mounted FDD,
then change RAM and main CPU type (with an appropriate ROM and installed
card), Apply & Reset, check the rebooted guest and serial endpoint,
and confirm the saved user configuration is unchanged until Save.

## Stage 4-B: live FDD media exchange

The SDL File menu's **2D floppy** and **2HD floppy** drive submenus now
mount/replace/eject their selected images immediately, across all four
physical drives on each controller. File selection itself is asynchronous;
only the short media swap uses the existing CPU, sub-CPU and PTM pause
mechanism. The emulator resumes its original running/paused state.

The FDC refuses an exchange during a selected-drive seek, timed read/write,
WRITE TRACK, or unfinished shared PIO/DMA transfer. A rejected exchange
shows a GUI error and leaves the current image and its displayed path
unchanged. Retry after the drive finishes I/O. A malformed image, a
read-only/unwritable host image or a duplicate mounted filename is likewise
rejected before the live media is touched. Aliases to the same canonical
file are detected.

All floppy sector and WRITE TRACK host writes now follow the *current*
per-drive image, instead of the startup image captured in the callback.
The old image is explicitly fdatasync'ed before detach; a failed sync
blocks the exchange. The 100-ms background host sync continues. Eject
disconnects the selected medium without changing other drives or the HDD.

**Important:** Host synchronization does not flush the *guest OS's* own
disk cache. Before exchanging a disk that the guest has written, close
open files or invoke the guest's normal flush/unmount procedure. The GUI
does not automatically save these changes to a configuration file;
**Save user configuration** remains a separate explicit action. The
HDD image path still changes only after Stage 4-C reset/apply support.

Regression test:
```sh
cmake --build build -j$(nproc)
ctest --test-dir build -R 'live_floppy_mounts|host_image_sync|floppy' --output-on-failure
ctest --test-dir build --output-on-failure
```

Manual check: boot from a 2D disk, use File > 2D floppy > Drive 1 to
mount another image, verify the guest can read it, then eject it and
verify not-ready behavior. Repeat for 2HD. Verify writes after
replacement reach the new host image and not the old one, including
after closing and restarting the emulator. Busy-drive exchanges should
be rejected and may be retried after disk activity ends.

## Stage 4-A usability update: larger controls and standalone chooser

The CRT File/Settings/Help menu and Settings editor now use larger fonts
(19-pixel ImGui default fonts) and more generous widget spacing. The
menu strip height is 40 pixels, outside the unmodified FM-11 640x400
image. It works with both OpenGL and software SDL renderer backends.

ImGuiFileDialog now runs inside a **third, separately managed SDL window**
rather than as a popup embedded in either the CRT or Settings window.
The chooser initially measures 1024x740 with a minimum of 850x620,
and its ImGui dialog is sized to fill the window. File-selection events
are polled in the existing SDL event loop, so displaying it never
requires a blocking GTK/NFD dialog or pauses emulated CPU workers.

The Settings editor's former Disks tab is removed. The File menu lists
2D/2HD FDD 0..3 and shows the currently *staged* image basename directly
beside each drive, with the full path shown inside the drive submenu.
The File menu also shows the HDD image. Until Stage 4-B, choosing/ejecting
a disk changes only the staged configuration; the mounted medium is
unchanged. The HDD selection is still also editable under ROM / Display,
and will apply after reset support is added in Stage 4-C.

The three CPU model choices (MC6809/HD6309 main, 8088/V20 main,
MC6809/HD6309 display) use radio buttons instead of drop-downs.
RAM capacity is an exact eight-choice control: 128, 256, 384, 512,
640, 768, 896, 1024 KiB.

Manual regression check after rebuilding: open both the File-menu and
Settings Browse choosers (confirm separate OS windows and readable initial
size), resize them, cancel and select paths, reopen the Settings editor,
verify selected filenames in the menu, then repeat with
`--renderer gl` and `--renderer software`. The regular CTest suite
continues to cover emulator behavior.

## Stage 4-A addendum: CRT menus and nonblocking file picker

The main SDL CRT window now draws **File / Settings / Help** in a reserved
28-pixel top menu strip (the emulated 640x400 display is never overwritten).
This is rendered using the matching Dear ImGui SDL2/OpenGL3 or
SDL2/SDL_Renderer backend; the guest renderer still selects GL or software
with `--renderer`.

- **File**: 2D and 2HD physical drives 0..3, with **Choose image...**
  and **Eject (edit only)**. These actions edit the pending configuration,
  not the mounted disk. Live media changes are Stage 4-B.
- **Settings**: **Configuration...** opens the separate settings window,
  **Pause/Resume** immediately uses the existing worker pause mechanism.
  **Reset** is visibly disabled pending Stage 4-C.
- **Help**: keyboard shortcuts and About popups.

`Browse` buttons on all eight FDD slots, the HDD image, and all ROM paths
use **ImGuiFileDialog v0.6.8 (MIT)**. The File menu opens its picker
inside the CRT window; settings Browse buttons open a picker inside the
separate settings window. File dialogs run inside ImGui frames
and do not nest a blocking native file-dialog event loop: the guest CPU,
PTM, serial service and SDL painting continue while files are selected.
The chosen path is stored in the pending editor configuration and is
persisted only when **Save user configuration** is clicked. A selection
alone never swaps live FDDs or reinitializes the current machine.

When the settings GUI is enabled, CMake FetchContent fetches ImGuiFileDialog
v0.6.8 along with pinned Dear ImGui. With
`-DFM11_SETTINGS_GUI=OFF` neither library is required, preserving the
existing SDL frontend and its original menu-free guest display.

Manual smoke check on both `--renderer gl` and `--renderer software`:

1. Confirm File/Settings/Help menus appear in the CRT window without
   changing the guest's 640x400 display content or proportions.
2. Choose a 2D FDD image through File > 2D floppy > Drive 0 > Choose image;
   verify the picker can be cancelled or used while the guest continues.
3. In Settings > Configuration, use Browse on a ROM and HDD path; save
   the configuration; inspect `~/.config/fm11emu/fm11emu.conf` (or the XDG
   destination); reopen and verify the edited paths.
4. Pause and resume with Settings > Pause / Resume and F2, check the guest
   keyboard, and repeat on the other renderer.
5. Verify `-DFM11_SETTINGS_GUI=OFF` still builds.

## Stage 4-A: SDL settings editor

The SDL frontend has a separate Dear ImGui settings window, toggled with
**Ctrl+F10**. It keeps the FM-11 CRT window intact, in both OpenGL and
software rendering modes. Edit configuration values under Disks, CPU/RAM,
ROM/Display, Serial, DIP switches, and Timing, then click **Save user
configuration**.

This first stage is **editing and saving only**: edits do not alter the
running emulation. FDD hot-swap is Stage 4-B; reset-and-apply (including
CPU board/RAM rebuilding) is Stage 4-C. In Stage 4-A the disk image
entries are paths, not file picker widgets.

The save destination is always the user's XDG location:

- `$XDG_CONFIG_HOME/fm11emu/fm11emu.conf` if XDG_CONFIG_HOME is absolute
- otherwise `$HOME/.config/fm11emu/fm11emu.conf`

The program continues to load settings in the order `--config`, user
config, `/etc/fm11emu.conf`, built-in defaults. The GUI never writes
`/etc` or an explicitly loaded `--config` path. If you keep passing
`--config /etc/fm11emu.conf` on subsequent launches, it will still
override the GUI-saved user configuration. The GUI displays its save path
and warns about this. Saving writes a complete INI file to a 0600
temporary file and atomically renames it over the user file after
syncing, rejecting invalid entries before replacement.

Stage 4-A uses **Dear ImGui v1.91.9b** fetched by CMake when building the
SDL frontend. This is a build-time dependency, not a new runtime package.
If building offline and the ImGui sources are not cached, configure with
`-DFM11_SETTINGS_GUI=OFF` to retain the existing SDL frontend without
the settings editor. After first configuring with network access, the
fetched sources remain in the build directory.

```sh
cmake -S . -B build -DFM11_SETTINGS_GUI=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R 'machine_config|sdl_help' --output-on-failure
ctest --test-dir build --output-on-failure
./build/fm11-sdl
```

## SDL2 display frontend (experimental)

Install `libsdl2-dev` on Ubuntu, then reconfigure CMake so it can
discover SDL2. This frontend runs the display subsystem through the shared `Machine`
scheduler and renders VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

To run the main 6809 CPU alongside the display CPU, supply its boot ROM:\n\n```sh\n./build/fm11-sdl --main-rom external/roms/boot6809.bin\n```\n\nThe default display ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
SDL always scales the image by an **integer** factor and centers it on
dark-gray margins with a thin light-gray outline just outside the emulated
screen. Neither the margins nor the outline alter any emulated pixels. At the default 960-pixel window width, the 640-pixel image
is shown at native 1:1 size rather than distorted to 1.5x. Enlarge the
window to at least 1280x800 for a sharp 2x presentation in either
200- or 400-line mode. Smaller windows clip instead of using
fractional resampling.
`--high` forces 400-line mode. `[display] height = 200|400` selects the initial viewport (default 400). The SDL frontend defaults to
**`--speed 10`** (ten times real-time emulated CPU/peripheral time).
Use `--speed 1` for real-time FM-11 clocks, `--speed 5` for five times
real-time, or `--speed 0` for unlimited execution (as fast as the host
can manage). Values 0 through 100 are supported as whole-number
multipliers. The hardware clock configuration remains 2 MHz for the
main 6809/display CPU and 4 MHz for the optional Z80 by default;
`--speed` changes the host-time pacing, not those clock settings.
Guest CPU/peripheral clocks remain synchronized relative to each other.
`SYNC/CWAI` advances peripherals at the requested virtual rate while
the main CPU is waiting. The display refresh cadence remains tied to
host time, near 31 frames/s (approximately one frame per 32 ms),
and the SDL renderer does not request VSYNC so rendering does not
block guest CPU execution waiting for a display refresh. `--cycles N` remains the *maximum* number
of primary CPU instructions per SDL loop (default 10000), not a clock
rate. Achieving the requested speed depends on host performance and
graphics/ROM workload. In unlimited mode idle waiting is also unpaced;
expect higher host CPU usage.

The window renderer defaults to `--renderer auto`: it tries the
OpenGL 3.3 core-profile shader renderer first and falls back to the existing
SDL software-composition path if the GPU/context is unavailable. Use
`--renderer gl` to **require** OpenGL (startup reports an error if unsupported),
or `--renderer software` to retain the previous `DisplayBitmap` and
SDL texture path. The chosen backend is printed on startup. The GL
fragment shader decodes all three physical graphics VRAM planes,
the palette, 4 KiB interleaved character/attribute RAM, the ANK ROM,
and the CRTC's latched text start directly from raw integer textures.
It preserves 200/400-line selection, text attributes, reverse video, and
integer pixel scaling without CPU bitmap composition. It requires desktop
OpenGL 3.3; neither the headless emulator nor the portable core/tests
requires OpenGL development headers. Both renderer modes remain
non-VSYNC at the host's ~31 fps repaint cadence. To compare performance,
run the same workload with `--cpu-speed --renderer gl` and with
`--cpu-speed --renderer software` (keeping `--speed` unchanged).

To measure effective main-CPU throughput, pass `--cpu-speed` to `fm11-sdl`.
It reports each second's **executed guest T-states**, wall-clock MHz,
instruction counts, and total guest CPU time per wall second on stderr.
6809 and Z80 are measured separately across FD05 handoffs. For example,
an uninterrupted 4 MHz Z80 at `--speed 1` should approach
**4,000,000 T/s** and **4.000 MHz**; at `--speed 10` the ideal
is **40,000,000 T/s** and **40.000 MHz**. A smaller rate
can indicate host-side slowdown (or guest CPU waiting/switching).
`guest/wall` sums emulated 6809 and Z80 instruction times and divides by
wall time. Interrupt waits (SYNC/CWAI) and other idle periods reduce this
ratio even when the emulator itself is not overloaded. The display sub-CPU
is intentionally excluded. The `target=` field shows the requested
speed multiplier and `guest/wall=` shows the achieved aggregate guest
CPU time per host second (not counting time spent in idle waits).
This diagnostic has no effect unless enabled; it does not adjust
guest clock speeds.

For a black screen while running **original** main and display ROMs, first
inspect the main CPU boot path and shared-bus handshake without changing
emulated memory or acknowledging IRQ/event registers:

```sh
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --trace-boot
```

`--trace-boot` prints the first 64 main 6809 instruction addresses
before execution. The periodic diagnostic line also reports the current
main PC (**decimal**), FD05 sub-CPU control latch, FD04 event latch,
display BUSY byte, number of main-to-sub shared RAM changes, first
shared byte, and display HALT state. All of these readings are
non-destructive; unlike a guest read from FD04, the diagnostic does
not acknowledge the event. Compare changes over multiple reports.
A constant display PC alone does not establish why firmware is waiting.

If the window stays black, use `./build/fm11-sdl --test-pattern` to
verify SDL rendering independently of firmware. This displays eight-color
stripes and does not require a ROM. Normal execution now prints the display
CPU PC, executed instructions, I/O writes, and count of nonzero physical
VRAM bytes every two seconds to stderr. An unchanged PC or zero VRAM count
is useful evidence that the firmware is waiting for main-CPU commands
rather than drawing a frame; the counters do not themselves establish why
it is waiting.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is an early integrated CPU/display viewer, not yet a complete
FM-11 keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

## Main/sub-CPU binary integration test

Before diagnosing original firmware, use the synthetic 6809 programs to
exercise only the main-to-display path. They are generated from opcode
bytes and **contain no original firmware**:

```sh
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_integration build/test-roms/main_test.bin build/test-roms/display_test.bin
ctest --test-dir build -R display_binary_integration --output-on-failure
```

A second, separate regression tests a complete command/acknowledgement
exchange through the same transport. The synthetic main-CPU ROM writes
command `$81` to `$FC80`, the display ROM reads it from `$9F80`,
draws a white pixel, then writes acknowledgement `$55` to `$9F81`.
The main CPU polls `$FC81` and records success as `$66` at
`$FC82`. Run it independently:

```sh
ctest --test-dir build -R display_binary_handshake --output-on-failure
./build/display_binary_integration build/test-roms/main_handshake.bin build/test-roms/display_handshake.bin --handshake
```

This validates bidirectional shared memory and CPU scheduling, **not**
the original FM-11 interrupt/attention protocol; a hardware notification
register is not inferred from this test.

### AFE0 display sub-CPU BUSY/READY (reference SubCpu.cpp)

The original `CSubCpu::SetIoByte($AFE0)` drives the sub-CPU
BUSY/READY handshake: writing bit 0 = 1 signals READY, and writing
bit 0 = 0 signals BUSY. Reading `$AFE0` returns the written control
byte with bit 0 forced to the current READY state. The former Linux
implementation incorrectly treated `$AFF0` as BUSY; the reference
instead sends `$AFF0` writes to a DMA channel register.

```sh
ctest --test-dir build -R 'display_afe0_handshake|display_binary_halt|display_cpu6809' --output-on-failure
```

The display subsystem now models the AFE0 handshake independently of
the AFF0 latch. Main-side **FD05 bit 7 now also reports BUSY** as controlled by
sub-CPU AFE0. An asserted main-side HALT is kept visible until release;
subsequent AFE0 READY clears the bit when HALT is released. Other FD05
bits and the original sub-CPU interrupt wiring still need verification.
The SDL diagnostic `BUSY` reports the AFE0-driven state.

### AFE6 video status (reference SubCpu.cpp)

The display ROM polls bit 7 at `$AFE6`. The original emulator's
`CSubCpu::GetIoByte` **changes status on every read**: after a
`VSyncCount`-driven clear, bits 7 and 2 toggle on subsequent reads
when `VSyncMode` is zero, and bit 5 is set. `VSyncCount` is
decremented as the sub CPU executes instructions and is set to 100
when CRTC register 13 is written. A previously guessed periodic
60Hz/1ms source has been removed.

```sh
ctest --test-dir build -R 'display_binary_raster|display_cpu6809' --output-on-failure
```

The synthetic display ROM exercises the original `TST $AFE6`
branch loops. This fixes the status semantics tested here but does
not yet port the original character-display composition or every
sub-CPU peripheral behavior.

### Display CPU interrupt dispatch and sub-I/O RAM port

The original `SubCpu.cpp::SimONE` checks pending sub IRQ, the `AFE4`
FIRQ latch and a periodic NMI after the first `AFE0` write. The
display 6809 adapter now invokes `SimIRQ`, `SimFIRQ` and `SimNMI`
before executing its next instruction, with the original initial
NMI threshold of 60000 and a CPU-cycle-based clock. Note: the
reference increments its clock for each memory access and adjusts
the NMI threshold dynamically against wall time; the Linux timing
model is thus **not yet cycle-equivalent**.

The original `GetMemByte/SetMemByte` also accesses sub RAM in
`A000-AFFF` except explicitly decoded registers. Linux previously
returned `FF` for much of that aperture. Ordinary sub I/O working
RAM is now readable and writable there; reading `AFE5` acknowledges
the sub IRQ request as in original `GetIoByte`. Tests exercise RAM
readback, reset, IRQ acknowledgment and NMI reset-period defaults.

```sh
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

This does **not** assert that real display ROM boots: sub IRQ request
generation from the main-side `FD05` bit6 and original `105`
ready/interrupt gating are still to be wired, and keyboard host input
and timing require additional integration.

### Sub-CPU keyboard event queue (source SubCpu.cpp)

The original Windows emulator's first `AFE0` write queues key event
`0x0050` and clears its NMI mask. Subsequently, `GetKeyCode`
copies pending keyboard events into `AFEC/AFED`, asserting bit 0 of
`AFE4` for ordinary key codes. Reading `AFED` invokes
`GetKeyCode` to advance the queue.

The Linux subsystem now includes `enqueueKeyCode`, `processKeyCode`,
the one-shot initial `AFE0` key event and the `AFED` queue advancement.
The independent FIFO regression checks these state transitions:

```sh
ctest --test-dir build -R 'display_keyboard_queue|display_io_read_clear' --output-on-failure
```

The original implementation also invokes `GetKeyCode` from a timed
keyboard/vsync event and supplies interrupts through `SimFIRQ` and
`SimNMI`. These *CPU-side interrupt delivery and timing paths* remain
unimplemented and are **not claimed** by the new keyboard FIFO test.
The original ROM may therefore still be black.

### Original display I/O read-clear latches

The reference Windows `Fm11/SubCpu.cpp::GetIoByte` clears three
register latches **on read**: `AFE4` (sub FIRQ), `AFEC`
(keyboard status) and `AFED` (keyboard data). The Linux display
subsystem now reproduces those read-clear semantics, including zero
values after reset and ignoring software writes to these registers.
A regression supplies external latch values using
`signalSubFirq` / `setKeyboardLatches` and verifies one-shot reads.

```sh
ctest --test-dir build -R 'display_io_read_clear|display_cpu6809' --output-on-failure
```

This is **not yet** the whole original keyboard/NMI implementation:
original `GetKeyCode()` processing after `AFED`, NMI timing, and
interrupt delivery remain separate work. Do not interpret passing
this test as proof that original display firmware now boots.

### Stage 10: original display ROM versus synthetic main sender

The stage-8 synthetic main ROM is kept as a **known, deterministic sender**.
Unlike stage 8, however, the sub CPU runs the user's **unmodified
original 20 KiB ROM**. This isolates genuine firmware startup/READY
handling from the main boot ROM and from the synthetic sub-CPU wrapper.

```sh
git pull
cmake --build build -j$(nproc)
python3 tools/make_display_test_roms.py build/test-roms
./build/display_real_rom_probe \\
    build/test-roms/main_text_real_protocol.bin \\
    external/roms/display6809.bin
```

The probe records up to 32 original-sub-CPU AFE0 writes with the
instruction PC, values, main FD05 status, READY/BUSY totals and
the busiest sub-CPU PCs. It also prints FC81/FC82/FC83/FC84 and
FDA0 after a bounded instruction budget. A zero exit code means
**the diagnostic completed**, not that the original firmware has
accepted the stage-8 test command.

The stage-8 main ROM's request format is only a controlled stimulus:
there is **no assertion** that it is the real firmware character
command. Do not use this probe's output to claim the original text
command works. This program intentionally is not a CTest because it
requires the operator's original display ROM. Once the firmware's
own command parsing is established, write a genuine command sender
and its regression test rather than inventing any new protocol.

### Stage 9: audit actual ROM display-command dispatch (preparation)

Do **not** treat the stage-8 `CAA0` byte-receive routine as proof of the
real firmware's text *command* semantics. The reference `SubRom.h`
begins with `B000: JMP B018`, and the ROM contains eight handler
addresses at `B07C-B08B`. Those are actual firmware entry/dispatch
structures, unlike the synthetic byte `A5` used in stage 7.

The first step is a reproducible, read-only audit of the operator's
original display ROM:

```sh
python3 tools/inspect_display_rom.py external/roms/display6809.bin
```

The tool validates the ROM size and startup signature, prints the eight
handler targets and the genuine `CAA0` bytes, and **does not infer**
unknown display command codes. Next, trace the original main-ROM sender
and the `B018/B08C` command decoding and identify the actual character
output handler before producing the stage-9 synthetic-ROM test.
No new communication behavior or firmware-specific display test is
claimed by this inspection-only commit.

### Original SubRom receive handshake: stage 8

This stage does **not invent a new receive protocol**. The generated
display ROM includes, byte-for-byte, the receive subroutine at
`$CAA0-$CACA` from the reference `Fm11/SubRom.h`, relocated
unaltered to `$B100` (all its branches are relative).

The reference `Fm11/SubCpu.cpp` maps the display CPU's `$9F80-$9FFF`
to main shared RAM `$FC80-$FCFF`. Its `$AFE0` bit 0 sets or
clears main-side `$FD05` bit 7.

- The original subroutine signals READY through `AFE0`, polls
  `9F82`, signals BUSY, clears bit 7 in `9F81`, then returns
  the payload from `9F82`.
- The synthetic main ROM waits for READY at `FD05`, writes a
  request with bit 7 set to `FC81` and ASCII `M` to `FC82`,
  and waits until the genuine subroutine clears the request bit.
- A **test-only wrapper**, not original firmware, writes the
  returned character to text RAM and signals display completion
  through `9F83`, after which main enables `FDA0=03`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_real_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_real_protocol.bin \\
    --rom build/test-roms/display_text_real_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected: a white `M` at the screen origin. This validates the
**original receive-handshake routine**, but not yet the actual
original firmware's display-command dispatcher or its character
command encoding. The real boot ROM is still not used in this test.

### Synthetic main-to-sub 6809 text request: stage 7

This is the first test where the **main 6809 requests text via shared
RAM**, rather than the display CPU unconditionally painting a predefined
message.

1. Sub 6809 initializes CRTC then writes `AFE0=1` (READY).
2. Main 6809 waits for `FD05 bit7=0`, writes command `A5` to
   `FC80` and ASCII `M` to `FC81`.
3. Sub 6809 reads the matching shared addresses `9F80/9F81`, copies
   the requested character into its text RAM and acknowledges by writing
   `9F82=66`.
4. Main 6809 detects the acknowledgement, enables text using
   `FDA0=03`, and sets a completion marker `FC83=77`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_protocol.bin \\
    --rom build/test-roms/display_text_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected SDL display: a white `M` at the upper-left of the screen.
The regression checks the READY transition, both CPUs' shared
request/acknowledgement, text/CRTC state, and 200/400-line pixels.
This is a deliberately minimal *test protocol*, not yet the original
firmware's command encoding. Original ROM boot is not part of this stage.

### Synthetic 6809 CRTC display-start scrolling: stage 6

Two sub-CPU ROMs write **identical** `FIRST LINE` and `SECOND LINE`
text into row 0 and row 1 of the 80-column text RAM. The first
ROM sets CRTC register 13 to 0; the second sets it to 80 text
cells (one full row). The main CPU ROM enables text as in stage 1.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_scroll --output-on-failure

# First row shows FIRST LINE, second row SECOND LINE:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_top.bin \\
    --ank-rom external/roms/ank.bin --high

# After CRTC start-address change, SECOND LINE moves to the first row:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_scroll.bin \\
    --ank-rom external/roms/ank.bin --high
```

This stage checks both 200- and 400-line framebuffer results with
generated distinguishable glyphs and verifies that the **same**
physical text RAM contents are present in both variants. It tests
the current CRTC text-start mapping, not original firmware boot.

### Synthetic 6809 display-layer selection: stage 5

This test makes the graphics region **large enough to see**.
The common display ROM writes yellow text `LAYER TEST` plus a 640x200
blue/black alternating-pixel stripe field, and the main ROM selects
which layers are enabled via `FDA0`:

| Main ROM | FDA0 | Expected appearance |
| --- | --- | --- |
| `main_layers_text.bin` | `03` | Yellow text on black |
| `main_layers_graphics.bin` | `0C` | Blue/black stripes across the screen, no text |
| `main_layers_both.bin` | `0F` | Yellow text over blue/black stripes |

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_layers --output-on-failure

# Change 'both' to 'text' or 'graphics' for the other two modes:
./build/fm11-sdl --main-rom build/test-roms/main_layers_both.bin \\
    --rom build/test-roms/display_layers.bin \\
    --ank-rom external/roms/ank.bin
```

The three new tests check actual main/sub-6809 writes and pixels for
both 200- and 400-line scanout. The 400-line view includes the
graphics plane's upper area, which is still blank because the test
draws only the first 200 scanlines.

### Synthetic 6809 graphics/text overlay: stage 4

This stage uses a **new pair of synthetic 6809 ROMs**. The main CPU
enables both text and graphics (`FDA0=$0F`). The display CPU sets
80-column/25-row CRTC text mode, writes a yellow `T` and a reverse
red `R` into text RAM, and paints two blue graphics pixels underneath
their cells using the graphics plane registers.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_overlay --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_overlay.bin \\
    --rom build/test-roms/display_text_overlay.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest runs the same ROMs with a generated minimal ANK font, and
checks both 200- and 400-line frames. The `T` glyph overlays the first
blue pixel in yellow, while the reverse-video `R` leaves the second
blue pixel visible through the glyph hole and paints the other pixels
of that character cell red. This tests actual output blending rather
than simply checking whether the two memory regions were written.

### Synthetic 6809 text attributes: stage 3

After validating basic text and 40/80-column geometry, the next
self-contained 6809 ROM writes six text cells with distinct attributes:

- `R`, `G`, `B`: red, green and blue character colors.
- `R` (fourth cell): reversed red, with the cell background filled.
- `H`: bright-white attribute.
- `W`: normal-white attribute.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_attributes --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_attributes.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest uses a generated minimal ANK glyph ROM, checking the text
attribute bytes written by the sub 6809 and the composited pixel colors
in both 200- and 400-line views. The SDL command uses the actual
extracted ANK ROM to show the characters.

F1 only changes the *200/400-line view*, not the CRTC 40/80-column
setting. In the current rendering model the 200-line view downsamples
400 logical character scanlines, so text appears vertically compressed.
Fidelity to original hardware scanline behavior remains to be verified.

### Synthetic 6809 text geometry: 40/80 columns and 200/400 lines

Stage 2 reuses the working stage-1 main ROM and two new display
6809 ROMs. The display CPU sets CRTC R1 to 20 (40 columns) or
40 (80 columns), R6 to 25 rows and writes `40/80 COL TEST`
into text RAM. The tests check both 640x200 and 640x400 framebuffer
geometry, using a deterministic test-only ANK font.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R 'display_binary_text_(40|80)col' --output-on-failure

# 40 columns (16 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_40col.bin \\
    --ank-rom external/roms/ank.bin --high

# 80 columns (8 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_80col.bin \\
    --ank-rom external/roms/ank.bin --high
```

Press F1 in SDL to compare the 200- and 400-line representations.
The expected 40-column letters are twice as wide as the 80-column
letters. This stage confirms the current geometry model; fidelity
to all original CRTC/scanline behavior must still be compared with
the reference emulator.

### Synthetic 6809 text display: stage 1

Start with a small **end-to-end** text test before original firmware. Both
processors run real 6809 binary instructions: the main CPU writes `$FDA0=3`
(text enabled); the display CPU initializes the CRTC to 80x25, writes
`HELLO FM-11` as alternating character/attribute bytes starting at
`$8000`, and places `$66` in shared RAM on completion.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_stage1 --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_stage1.bin \\
    --ank-rom external/roms/ank.bin --high
```

The headless regression uses a separate generated font
`ank_text_stage1.bin` with simple known glyph pixels, and checks
CPU-written text RAM, CRTC values, and output pixels. The SDL run above
uses the **actual** extracted ANK ROM, so `HELLO FM-11` should appear
at the top left. If the original ANK ROM is not available, the same SDL
command can use `build/test-roms/ank_text_stage1.bin` to show simple
glyph marks rather than letters.

This is **stage 1 only**: plain text on black. After confirming its
SDL result, later stages will cover colors, reverse attributes and
graphics/text overlays separately.

### Reference ANK text composition (first implementation)

The original `Fm11/fm11.hlsl` composites two-byte character/attribute
cells from the sub-CPU text RAM, with the CRTC start address and 8x16
ANK glyphs. That operation is now available in the SDL frontend, including
text reverse video, color attributes, and the 640x200/400 output sizes.
The software scanout matches the shader's LSB-first glyph bit ordering.

The reference archive includes `Fm11/CharRom.h`; convert its embedded
4096-byte ANK array without committing those font bytes:

```sh
python3 tools/extract_ank_rom.py /path/to/Fm11/CharRom.h external/roms/ank.bin
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --ank-rom external/roms/ank.bin
ctest --test-dir build -R display_text_composition --output-on-failure
```

Providing `--ank-rom` loads font data; it does **not** force the
text layer on when a main CPU is present. The original display control
register `$FDA0` enables text with bits 0/1 and graphics with bits
2/3. The renderer also applies the eight programmable palette entries
from `$FD98-$FD9F` and the original 16-entry color table.
The current software scanout supports attribute bit 5 (bright/bold)
and reverse video; blink, cursor and clip state remain future work.
Without a main CPU, specifying `--ank-rom` still explicitly enables
text for standalone diagnostics.
Other original display behavior, including blink, cursor and clip, remains to be ported.
Without `--ank-rom`, the established graphics-only regression frames
remain unchanged.

### Full-frame binary display test and SDL preview

To check the complete raster path without original firmware, generate
the synthetic ROM pair and run the test:

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_pattern --output-on-failure
./build/display_binary_pattern build/test-roms/main_pattern.bin build/test-roms/display_pattern.bin
```

The main 6809 sends a command through shared RAM; the display 6809
writes all 16,000 bytes of the 640x200 plane with an alternating pixel
pattern in all three colors, then acknowledges completion. The test
checks **every rendered pixel**, not just the first byte.

To inspect the same pattern visually in the SDL frontend:

```sh
./build/fm11-sdl --main-rom build/test-roms/main_pattern.bin \\
    --rom build/test-roms/display_pattern.bin
```

This should display narrow alternating white and black vertical lines
across the entire 640x200 framebuffer. It is a deliberately artificial
bring-up image, not the FM-11 firmware's normal boot screen.

### Display-generated ATTENTION (original I/O semantics)

Original `Fm11/SubCpu.cpp`, `CSubCpu::GetIoByte`, case
`$AFE7`, raises main-CPU ATTENTION when the display CPU **reads**
that address. This is distinct from the earlier host-injected event
tests. The implementation now connects that read to the existing main
`$FD04` event latch.

A pair of tiny 6809 binaries verifies that the display CPU's `LDA
$AFE7` triggers an event that the main CPU receives with `LDA
$FD04` and then acknowledges by reading `$FD04` again:

```sh
ctest --test-dir build -R display_binary_afe7 --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_afe7 build/test-roms/main_afe7.bin build/test-roms/display_afe7.bin
```

### HALT and BUSY binary regression

```sh
ctest --test-dir build -R display_binary_halt --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_halt build/test-roms/main_halt.bin build/test-roms/display_halt.bin
```

This runs real, standalone MC6809 binary instructions on both CPU
cores: main writes `$FD05` to halt the display CPU, writes a marker to
shared `$FC80`, and releases HALT. The test checks that the display
PC remains frozen during HALT; after release, the display ROM sets
`$AFF0` (BUSY) and writes a shared-RAM marker at `$9F82`.
There is no dependency on an original ROM dump.

ATTENTION's sub-CPU hardware trigger is still not mapped. The existing
`MainBoard::signalAttention()` path does **not** establish how an
actual display ROM should raise the event; this test deliberately does
not fabricate one.

The main CPU program at `$5000` writes `$80` to `$FC80`.
The display CPU program at `$B000` waits for the same value at
`$9F80`, then writes `$80` to VRAM through the three color planes.
The test asserts shared RAM, physical VRAM and the first rendered white
pixel. CTest creates temporary binaries automatically; the manual commands
above keep them under `build/` for inspection.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current scope

- Portable fixed-width compatibility types for legacy `BYTE`, `WORD`, `DWORD`, and `LONG` usage.
- Existing `Sim09` 6809/6309 instruction core compiled as portable C++.
- Existing `SimZ80` instruction core compiled as portable C++.
- Legacy MFC register-window callbacks removed from the portable core.
- Z80's FM-11-specific memory/MMR wiring is deliberately not embedded in the CPU core. The initial portable hook uses the supplied flat `Memory` buffer; board-specific address translation will be added in the CPU-board layer.

No SDL frontend or complete FM-11 machine is implemented yet.

## Headless serial CPU runner (experimental)

On Linux, `fm11-headless` runs an MC6809/HD6309 raw binary with
standard input/output connected to the main RS-232C byte queues.
It is intended for early serial driver bring-up, **not** NitrOS-9 booting:
there is no IPL, disk controller, OS image loader or execution pacing yet.

```sh
./build/fm11-headless --binary program.bin --load 0x1000 --pc 0x1000 --cpu 6809 --clock 2000000
```

Use `--instructions 100000` for a bounded smoke run. Ctrl-C stops an
unbounded run. Input is processed without canonical line buffering when
attached to a TTY; the original terminal settings are restored on exit.

## FM-11 NitrOS-9 boottrack bring-up

The floppy IPL sources in `sueyasu/fm11-nitros9` load the first
stage from track 0, side 0, sector 5 onwards. A raw, **contiguous**
boottrack payload (not a whole floppy image) may be supplied directly:

```sh
./build/fm11-headless --boottrack boottrack.bin --level 1 --cpu 6809
./build/fm11-headless --boottrack boottrack-l2.bin --level 2 --cpu 6809
```

Level 1 expects exactly 17 × 256 bytes at `$EB00`, starting at
`$EB02`. Level 2 expects exactly 21 × 256 bytes at `$E100`,
starting at `$E102`. This bypasses ROM IPL/FDC sector loading only;
it does **not** emulate any subsequent boot-time disk reads. Normal
NitrOS-9 startup therefore requires further disk-controller support.
Use `--instructions N` for bounded diagnostic runs.

## D88 floppy images

The headless runner accepts either a 327680-byte raw 2D image or a D88
2D image via `--floppy`. D88 sectors are selected by their C/H/R IDs,
not by assuming they appear in sector-number order. The initial
read-only MiniFDC implementation requires a complete 40-track, two-sided,
16-sector/side disk with 256-byte sectors and rejects malformed,
missing, duplicated or non-2D sector records.

## Host monitor and serial PTY

The headless emulator starts **halted**. Its original terminal is a
line-oriented monitor, not the guest console. Startup prints a serial
PTY path, e.g. `/dev/pts/5`; connect from another terminal with
`screen /dev/pts/5` (or `picocom`). The PTY slave is configured for
raw 8-bit communication. Guest Ctrl-C passes through the serial PTY
without terminating the emulator.

At the `fm11>` prompt, use `c` to run, `stop` to pause,
`s` to step one instruction, `r` for registers, `m HEXADDR [COUNT]`
for CPU-logical memory, `mp HEXADDR [COUNT]` for physical memory,
`reset` to reset and reload the supplied bootstrap, and `q` to quit.
The monitor is available while the CPU runs. `--instructions` pauses the
CPU at the limit instead of terminating the monitor.


### CPU idle waiting (6809/6309)

The headless runner executes instructions **without a real-time CPU throttle**
while running. Starting with the parallel-PTM branch, the `c` command also
starts the **same `Machine::startRealtimePtm()` worker used by SDL**.
The shared MC6840 timer advances from host `steady_clock` at 9700 Hz,
independently of guest CPU instructions. CPU instruction cycles and the
`SYNC`/`CWAI` wait loop do **not** advance this timer a second time.
When a guest executes `SYNC` or `CWAI`, the host waits using `poll()`
for PTY/monitor activity or the next timer edge. The CPU adapter samples
the PTM's published IRQ level at instruction/wait boundaries.

`CWAI` keeps its pre-stacked interrupt frame; `SYNC` does not stack.
A masked IRQ wakes `SYNC` without entering its vector, while `CWAI`
requires an accepted interrupt. The monitor `stop`/`q` commands stop
and join the timer worker, and `c` restarts it with a fresh host-time
baseline. `reset` while halted resets the shared timer along with the CPU
card. Single-step while halted uses the legacy deterministic cycle clock.
This is **not** a cycle-exact real-time CPU emulation mode.

### Testing without USART Tx interrupts

To diagnose persistent CPU load while NitrOS-9 is waiting for input, launch
`fm11-headless` with `--no-tx-irq`. This temporarily suppresses only the
8251-compatible **transmit** interrupt request; serial transmission, reception
and receive interrupts remain operational. The default behavior is unchanged.
Compare host CPU usage at the Shell prompt with and without the switch.
This is a diagnostic option, not a verified FM-11 hardware correction.

### CPU idle diagnostics

Add `--idle-log idle.csv` to write a CSV sample approximately once per host
second. The samples contain executed instructions, SYNC/CWAI entries, measured
host wait duration for each state (milliseconds), IRQ entries accepted by the
CPU, PTM IRQ assertions, and current PC/CC/wait state. Counts are interval
deltas, not cumulative. Compare samples at the NitrOS-9 shell prompt.

## Boot from a 2D floppy image

With an FM-11 NitrOS-9 bootable raw 2D or D88 floppy image, no separate
boottrack file or OS level is needed:

```sh
./build/fm11-headless --floppy disk.d88 --cpu 6809
```

The FM-11 ROM's initial floppy load is simulated: track 0, side 0,
sectors 1 and 2 are copied to `$0400` and execution starts at `$0400`.
The IPL contained in those sectors then loads the boottrack using the
emulated MiniFDC. This is a disk IPL handoff, not execution of the
original FM-11 system ROM. `--boottrack FILE --level 1|2` remains available.
The monitor's `reset` command reloads the disk IPL from the mounted image.

## 8088/V20 boot-ROM trace (experimental)

The separate diagnostic runner executes an external raw **4096-byte** FM-11
8088 boot-ROM image, beginning at the x86 reset vector FFFF:0000. No ROM
binary is included in this repository. This is a CPU-card diagnostic, not
full FM-11 boot: the remaining hardware and CPU instructions are incomplete.

```sh
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu 8088 --instructions 10000 --trace
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu v20 --instructions 10000
```

The runner prints the first unimplemented opcode with its physical PC and
instruction number; `--trace` logs per-instruction register values. For now
8088 and V20 share the same implemented instruction subset. This tool does
not modify the 6809/6309 headless boot path.


## Experimental parallel CPU/PTM branch

The `dev/parallel-cpu-ptm` branch is an incremental migration; **stage 1
does not start any emulation worker threads**. CPU, display CPU, PTM and
frame rendering still run through the existing single-thread scheduler.

Stage 1 adds `WorkerGroup`, a cooperative lifecycle controller for future
CPU and PTM workers. `start(step)` registers a finite work step;
`pause()` waits until **all** workers have parked at a step boundary;
`resume()` releases them, and `stop()` joins the threads. Worker
exceptions stop the group and can be rethrown on the owner thread via
`rethrowWorkerFailure()`. Do not call `stop()` or destroy the group from
one of its own worker threads, and do not use unbounded blocking steps.

The SDL-to-display-CPU keyboard ring buffer now has a narrow mutex. **This
does not make `DisplaySubsystem` or `MainBoard` generally thread-safe.**
Shared RAM at $9F80, FD05/AFE0 BUSY and HALT latches, main/sub IRQ and
FIRQ state, PTM registers, and VRAM/CRTC access have side effects. These
must be assigned clear owners and access ordering before starting concurrent
CPU/PTM work (stages 2-4). In particular, OpenGL currently reads display
memory directly and requires an immutable render snapshot once the display
worker exists. Do not wrap every RAM access in a global mutex; do not enable
unprotected concurrent guest execution.

Stage 2: SDL and the interactive `fm11-headless` runner use a
**dedicated real-time PTM worker**. SDL starts it after initialization;
headless starts it on monitor `c` and stops it on `stop`/`q`. The MC6840 advances from host `steady_clock` at its 9700 Hz
input rate, regardless of `--speed` or main CPU instruction cycles. PTM
register accesses catch up to the exact host time under a narrow mutex;
a 1 ms worker polling interval supplies asynchronous IRQs even when the
CPU is in SYNC/CWAI. The IRQ level is published atomically; only the CPU
thread changes the CPU adapter's IRQ aggregator. SDL pause parks the PTM
worker and rebases its clock on resume, and reset stops/restarts the worker.

Stage 3: the SDL frontend starts a **separate display sub-CPU worker**.
It advances the display 6809 directly from host `steady_clock` at 2 MHz,
independent of the main CPU and the PTM. The former
`advanceDisplayCpuCycles()` / `advanceDisplayTime()` paths are disabled
while the display worker runs, but remain for deterministic legacy tests.
The SDL SYNC/CWAI path no longer advances guest virtual time. A narrow
display-bus mutex serializes each sub-CPU instruction with main-CPU
mailbox/FD05 operations; rendering holds the same lock while reading the
current VRAM/CRTC state. Display ATTENTION is published through an atomic
event latch. SDL pause/resume controls both workers and rebases the display
clock on resume. Headless and other noninteractive harnesses retain their
existing deterministic display scheduler unless explicitly started.
The PTM worker must be stopped before manipulating or resetting shared
device state directly. This is not yet a fully parallel CPU/display machine;
all other shared state still belongs to the single emulation thread.

Run `ctest --test-dir build --output-on-failure`, including `worker_group`,
`realtime_ptm` and `realtime_display_cpu`. Verify NitrOS-9 boot, shared
mailbox handshakes and wall-time clock on SDL and `fm11-headless`.
Both PTM and display clocks are **independent of** `--speed`; only the
main CPU still uses SDL pacing.

### Stage 4: main CPU worker

SDL no longer executes main 6809/Z80 instructions on its event/render thread.
A dedicated `WorkerGroup` CPU worker owns the existing
`Machine::executeOne()` path, including FD05 CPU-card selection, IRQ
sampling, and the `SdlCpuPacer` clock. `SYNC/CWAI` only wake when their
interrupt conditions are satisfied; the PTM and display workers continue
independently. The main CPU worker services the serial PTY before and after each bounded
instruction batch, so serial input never waits for SDL's rendering loop or
contends for a CPU mutex. A separate narrow main-CPU mutex protects frontend
CPU diagnostics. Display shared RAM/FD05 transactions preserve
the CPU-to-display lock order. F2 pauses all three workers; resume rebases
CPU pacing rather than replaying paused time. Rendering copies display
memory under its own lock, then presents outside the lock.

The deterministic, nonthreaded `Machine::executeOne()` API remains
available to existing headless runners and core regression tests. The
`parallel_cpu_display` test exercises three workers and display snapshots.

### Stage 5: remove the old scheduler from the parallel execution path

The legacy main-CPU-cycle-coupled display scheduler has been extracted from
`Machine.cpp` into `src/core/DeterministicDisplayScheduler.h`. It is retained
only for single-threaded headless utilities and regression tests; while the
display worker runs, CPU instructions and SDL rendering never execute that
scheduler. This preserves deterministic tests without maintaining two display
clocks in the interactive frontend.

The hot path caches primary/optional CPU-card indices at attachment time,
and FD05 writes now update the active CPU selection through a dedicated
callback, independently of diagnostic observers. The active CPU's core pointer,
CPU kind and post-instruction dispatch are cached when the selected board
changes; ordinary instructions no longer perform per-instruction RTTI or
FD05 selection polling. Manual selection, reset, and board attachment
also refresh the cache. SDL's CPU
worker publishes its instruction count once per batch instead of performing
an atomic increment per instruction, and takes the diagnostic CPU-state mutex
only when a frontend speed or CPU diagnostics consumer is enabled. The display
worker amortizes its display-bus lock across at most 16 instructions, retaining
short mailbox/FD05 access opportunities for the main CPU. PTM frequency and
clock semantics are unchanged by these performance changes.

### PTM channel-specific IRQ acknowledgement

PTM status reads now acknowledge each channel's pending expiry independently.
An unrelated timer channel reaching terminal count must not reactivate an
already acknowledged IRQ from another channel. The legacy status latch remains
visible until that channel's counter register is read, preserving existing
software behavior. The `main6809_ptm` regression exercises this specific
cross-channel reassertion case. This correction does not alter the 9700 Hz
input clock or the 50 Hz OS clock reload value, and does not by itself establish
why SDL clock time was previously accelerated.

### OpenGL snapshot and display worker speed

SDL `--speed 1..100` applies the multiplier to the main CPU and
display sub-CPU, each starting from its configured 2 MHz hardware clock.
The display sub-CPU is provisionally capped at 8 MHz, so `--speed 4`
and above target the same display clock. With `--speed 0`, the main CPU
runs unlimited while the display sub-CPU remains paced at 8 MHz.
These are pacing targets, not guaranteed achieved throughput. The
headless/deterministic display scheduler remains at its configured
base clock. The PTM remains tied to host steady-clock time.

The OpenGL frontend still captures a coherent copy of display VRAM,
text/ANK data, CRTC state, display control and palette while holding
the display-bus mutex. All OpenGL texture uploads, uniforms, rendering
and buffer swaps take place after the mutex has been released.

### Independent display bus and mailbox locking

The main CPU no longer takes the display-CPU/VRAM recursive mutex when
accessing its 128-byte FC80-FCFF mailbox. The shared bytes are guarded by
a dedicated mutex on both the main-side FC80-FCFF path and the display
CPU's 9F80-9FFF memory accesses. Shared transaction tracing is serialized
under that same mutex. The shared-write revision is atomic.

FD05 BUSY status and the sub-CPU busy latch can be read atomically
without acquiring the display mutex. FD05 writes that change sub-CPU
state still use the display mutex. The wake-observer callback has a
separate lifetime mutex so mailbox writes can wake the display worker
without holding the display lock. Display-CPU instruction slices and
video snapshots retain their existing display mutex.

### Avoid redundant main-CPU PTM IRQ synchronization

The SDL main-6809 worker samples the PTM IRQ before each instruction for
SYNC/CWAI wakeup and interrupt acceptance. It now passes that fact into
`Machine::executeOne()`, which skips the redundant post-instruction PTM
aggregation only while the PTM is in independent real-time mode. Headless
and deterministic stepping retain their post-instruction synchronization,
as do optional CPU-card execution paths. The main-6809 IRQ aggregator
also avoids rebuilding FD03/FD2C when the published PTM IRQ source level
has not changed; explicit IRQ-mask and other-source updates continue to
recalculate their status at the point of the write.

### Main 6809 RAM page lookup optimization

The main MC6809/HD6309 CPU caches the host-memory base address of each
logical 4 KiB RAM page, avoiding repeated MMR context translation and
physical-memory bounds checking on every ordinary RAM access. FD80-FD8F
writes update the affected active page, while FD90 context changes and FD93
MMR enable changes refresh the 16-entry mapping. Reset and replacement of
the shared 1 MiB physical RAM backing also rebuild the cache. The boot-ROM
overlay, 7C00-7FFF window, FC00-FFFF fixed/IO aperture, and direct physical
DMA accesses continue through their existing handlers. The `main6809_mmr`
regression covers page remapping, contexts, enable toggles, window override,
and shared-memory replacement.

## Intel 8088 hardware-reference instruction tests

The standalone `fm11-8088-singlestep` runner can compare
`Main8088Board::Core::executeOne()` with the real-CPU
[SingleStepTests/8088 V2](https://github.com/SingleStepTests/8088)
instruction test suite (external data, not bundled). Build normally, then:

```sh
ctest --test-dir build -R main8088_singlestep_smoke --output-on-failure
git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
git -C external/8088-tests sparse-checkout set v2
python3 tests/run_8088_singlestep.py --opcode 00 --limit 100
python3 tests/run_8088_singlestep.py --all --limit 100
```

See [docs/8088-singlestep.md](docs/8088-singlestep.md) for full-case
execution, grouped ModR/M opcodes, failure reports and prefetch limitations.
 --output-on-failure
```


Linux port and ongoing development of the FM-11 emulator.

This first revision establishes a portable CMake build for the existing 6809/6309 and Z80 CPU cores. The Windows MFC/Direct3D frontend is intentionally not part of the Linux build.

## Optional link-time optimization (LTO)

LTO is **off by default**. To compare the same Release build with and without
interprocedural optimization, use separate build directories:

```sh
cmake -S . -B build-normal -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=OFF
cmake --build build-normal -j$(nproc)
ctest --test-dir build-normal --output-on-failure

cmake -S . -B build-lto -DCMAKE_BUILD_TYPE=Release -DFM11_ENABLE_LTO=ON
cmake --build build-lto -j$(nproc)
ctest --test-dir build-lto --output-on-failure
```

`FM11_ENABLE_LTO=ON` checks whether CMake's C++ IPO support is available and
stops configuration with a diagnostic if it is not. IPO is then enabled for
the emulator core, frontends, and tests. No CPU emulation logic or scheduling
behavior changes. Benchmark `fm11-sdl --speed 0 --cpu-speed --renderer gl`
under identical host conditions to assess whether LTO improves throughput.

## Installed CPU cards and main-CPU DIP selection

`fm11-sdl` reads CPU hardware settings from the first available file:
`--config FILE` (explicit), `~/.config/fm11emu/fm11emu.conf`, or
`/etc/fm11emu.conf`. With no file it defaults to the original 6809-only
configuration. A supplied but invalid file produces a line-numbered error.

Copy `config/fm11emu.conf.example` and edit its values. The independent
`[main_6809]` and `[main_8088]` `installed` values describe physically
fitted cards; `[main] active = 6809|8088` models the FM-11 EX DIP switch.
Selecting either card leaves the other fitted and reset, but inactive.
The `[main_6809] cpu` accepts `6809|6309`; `[main_8088] cpu`
accepts `8088|v20`. All `clock_hz` settings are in Hz.

The separate `[optional_cpu] type = none|z80` controls the optional
Z80 card, selected via FD05 relative to the DIP-selected main card.
The display CPU supports 6809 only at present; a `6309` setting is
rejected rather than silently ignored.

CPU ROM and media file paths remain runtime parameters: `--main-rom`
loads the 6809 card firmware, and `--8088-rom` loads the 8088/V20 card
firmware. Specify the selected card's ROM to start its CPU. For instance:

```sh
./build/fm11-sdl --config config/fm11emu.conf.example \
    --main-rom external/roms/boot6809.bin \
    --8088-rom external/roms/boot8088.bin --speed 0
```

`--speed` remains a runtime multiplier of the configured physical CPU
clock. The independent display CPU still observes its 8 MHz runtime cap.
The old `--z80` and `--z80-clock` flags are replaced by config settings.

## External ROM images (not committed)

Original emulator firmware is supplied as C byte arrays. Extract them once
from the source ZIP or an unpacked `Fm11/` directory:

```sh
python3 tools/extract_roms.py /path/to/fm11_x64.zip
# Or: python3 tools/extract_roms.py /path/to/Fm11/
```

This writes the following files under `external/roms/`, which is excluded
by `.gitignore` and **must not be committed**:

| File | Size | Reference array |
|---|---:|---|
| `boot6809.bin` | 4096 | `BootRom.h:BootRom` |
| `boot8088.bin` | 4096 | `BootRom86.h:BootRom86` |
| `display6809.bin` | 20480 | `SubRom.h:SubRom` |
| `ank.bin` | 4096 | `CharRom.h:AnkRom` |
| `kanji.bin` | 131072 | `KanRom.h:KanRom` |

The final emulator will use ROM dumps supplied by the user from their own
machine, in the same binary file format and directory layout. ROM filenames
and sizes are validated, but ROM contents are never embedded in the binary.
The C++ core offers `loadDisplayRomFile` on `Machine` and
`loadBootRomFile` on the 6809 and 8088 adapters. The diagnostic runner
defaults to `external/roms/display6809.bin`:

```sh
./build/fm11-display-romtrace --instructions 100000
# Or override: --rom /somewhere/display6809.bin
```

The headless OS boottrack diagnostic currently uses a synthetic boot ROM,
not the extracted original boot firmware. The font ROMs are staged for the
later display/keyboard frontend, not consumed by current rendering code.

## SDL2 display frontend (experimental)

Install `libsdl2-dev` on Ubuntu, then reconfigure CMake so it can
discover SDL2. This frontend runs the display subsystem through the shared `Machine`
scheduler and renders VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

To run the main 6809 CPU alongside the display CPU, supply its boot ROM:\n\n```sh\n./build/fm11-sdl --main-rom external/roms/boot6809.bin\n```\n\nThe default display ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
SDL always scales the image by an **integer** factor and centers it on
dark-gray margins with a thin light-gray outline just outside the emulated
screen. Neither the margins nor the outline alter any emulated pixels. At the default 960-pixel window width, the 640-pixel image
is shown at native 1:1 size rather than distorted to 1.5x. Enlarge the
window to at least 1280x400 for a sharp 2x 640x200 display, or 1280x800
for a sharp 2x 640x400 display. Smaller windows clip instead of using
fractional resampling.
`--high` starts in 400-line mode. The SDL frontend defaults to
**`--speed 10`** (ten times real-time emulated CPU/peripheral time).
Use `--speed 1` for real-time FM-11 clocks, `--speed 5` for five times
real-time, or `--speed 0` for unlimited execution (as fast as the host
can manage). Values 0 through 100 are supported as whole-number
multipliers. The hardware clock configuration remains 2 MHz for the
main 6809/display CPU and 4 MHz for the optional Z80 by default;
`--speed` changes the host-time pacing, not those clock settings.
Guest CPU/peripheral clocks remain synchronized relative to each other.
`SYNC/CWAI` advances peripherals at the requested virtual rate while
the main CPU is waiting. The display refresh cadence remains tied to
host time, near 31 frames/s (approximately one frame per 32 ms),
and the SDL renderer does not request VSYNC so rendering does not
block guest CPU execution waiting for a display refresh. `--cycles N` remains the *maximum* number
of primary CPU instructions per SDL loop (default 10000), not a clock
rate. Achieving the requested speed depends on host performance and
graphics/ROM workload. In unlimited mode idle waiting is also unpaced;
expect higher host CPU usage.

The window renderer defaults to `--renderer auto`: it tries the
OpenGL 3.3 core-profile shader renderer first and falls back to the existing
SDL software-composition path if the GPU/context is unavailable. Use
`--renderer gl` to **require** OpenGL (startup reports an error if unsupported),
or `--renderer software` to retain the previous `DisplayBitmap` and
SDL texture path. The chosen backend is printed on startup. The GL
fragment shader decodes all three physical graphics VRAM planes,
the palette, 4 KiB interleaved character/attribute RAM, the ANK ROM,
and the CRTC's latched text start directly from raw integer textures.
It preserves 200/400-line selection, text attributes, reverse video, and
integer pixel scaling without CPU bitmap composition. It requires desktop
OpenGL 3.3; neither the headless emulator nor the portable core/tests
requires OpenGL development headers. Both renderer modes remain
non-VSYNC at the host's ~31 fps repaint cadence. To compare performance,
run the same workload with `--cpu-speed --renderer gl` and with
`--cpu-speed --renderer software` (keeping `--speed` unchanged).

To measure effective main-CPU throughput, pass `--cpu-speed` to `fm11-sdl`.
It reports each second's **executed guest T-states**, wall-clock MHz,
instruction counts, and total guest CPU time per wall second on stderr.
6809 and Z80 are measured separately across FD05 handoffs. For example,
an uninterrupted 4 MHz Z80 at `--speed 1` should approach
**4,000,000 T/s** and **4.000 MHz**; at `--speed 10` the ideal
is **40,000,000 T/s** and **40.000 MHz**. A smaller rate
can indicate host-side slowdown (or guest CPU waiting/switching).
`guest/wall` sums emulated 6809 and Z80 instruction times and divides by
wall time. Interrupt waits (SYNC/CWAI) and other idle periods reduce this
ratio even when the emulator itself is not overloaded. The display sub-CPU
is intentionally excluded. The `target=` field shows the requested
speed multiplier and `guest/wall=` shows the achieved aggregate guest
CPU time per host second (not counting time spent in idle waits).
This diagnostic has no effect unless enabled; it does not adjust
guest clock speeds.

For a black screen while running **original** main and display ROMs, first
inspect the main CPU boot path and shared-bus handshake without changing
emulated memory or acknowledging IRQ/event registers:

```sh
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --trace-boot
```

`--trace-boot` prints the first 64 main 6809 instruction addresses
before execution. The periodic diagnostic line also reports the current
main PC (**decimal**), FD05 sub-CPU control latch, FD04 event latch,
display BUSY byte, number of main-to-sub shared RAM changes, first
shared byte, and display HALT state. All of these readings are
non-destructive; unlike a guest read from FD04, the diagnostic does
not acknowledge the event. Compare changes over multiple reports.
A constant display PC alone does not establish why firmware is waiting.

If the window stays black, use `./build/fm11-sdl --test-pattern` to
verify SDL rendering independently of firmware. This displays eight-color
stripes and does not require a ROM. Normal execution now prints the display
CPU PC, executed instructions, I/O writes, and count of nonzero physical
VRAM bytes every two seconds to stderr. An unchanged PC or zero VRAM count
is useful evidence that the firmware is waiting for main-CPU commands
rather than drawing a frame; the counters do not themselves establish why
it is waiting.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is an early integrated CPU/display viewer, not yet a complete
FM-11 keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

## Main/sub-CPU binary integration test

Before diagnosing original firmware, use the synthetic 6809 programs to
exercise only the main-to-display path. They are generated from opcode
bytes and **contain no original firmware**:

```sh
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_integration build/test-roms/main_test.bin build/test-roms/display_test.bin
ctest --test-dir build -R display_binary_integration --output-on-failure
```

A second, separate regression tests a complete command/acknowledgement
exchange through the same transport. The synthetic main-CPU ROM writes
command `$81` to `$FC80`, the display ROM reads it from `$9F80`,
draws a white pixel, then writes acknowledgement `$55` to `$9F81`.
The main CPU polls `$FC81` and records success as `$66` at
`$FC82`. Run it independently:

```sh
ctest --test-dir build -R display_binary_handshake --output-on-failure
./build/display_binary_integration build/test-roms/main_handshake.bin build/test-roms/display_handshake.bin --handshake
```

This validates bidirectional shared memory and CPU scheduling, **not**
the original FM-11 interrupt/attention protocol; a hardware notification
register is not inferred from this test.

### AFE0 display sub-CPU BUSY/READY (reference SubCpu.cpp)

The original `CSubCpu::SetIoByte($AFE0)` drives the sub-CPU
BUSY/READY handshake: writing bit 0 = 1 signals READY, and writing
bit 0 = 0 signals BUSY. Reading `$AFE0` returns the written control
byte with bit 0 forced to the current READY state. The former Linux
implementation incorrectly treated `$AFF0` as BUSY; the reference
instead sends `$AFF0` writes to a DMA channel register.

```sh
ctest --test-dir build -R 'display_afe0_handshake|display_binary_halt|display_cpu6809' --output-on-failure
```

The display subsystem now models the AFE0 handshake independently of
the AFF0 latch. Main-side **FD05 bit 7 now also reports BUSY** as controlled by
sub-CPU AFE0. An asserted main-side HALT is kept visible until release;
subsequent AFE0 READY clears the bit when HALT is released. Other FD05
bits and the original sub-CPU interrupt wiring still need verification.
The SDL diagnostic `BUSY` reports the AFE0-driven state.

### AFE6 video status (reference SubCpu.cpp)

The display ROM polls bit 7 at `$AFE6`. The original emulator's
`CSubCpu::GetIoByte` **changes status on every read**: after a
`VSyncCount`-driven clear, bits 7 and 2 toggle on subsequent reads
when `VSyncMode` is zero, and bit 5 is set. `VSyncCount` is
decremented as the sub CPU executes instructions and is set to 100
when CRTC register 13 is written. A previously guessed periodic
60Hz/1ms source has been removed.

```sh
ctest --test-dir build -R 'display_binary_raster|display_cpu6809' --output-on-failure
```

The synthetic display ROM exercises the original `TST $AFE6`
branch loops. This fixes the status semantics tested here but does
not yet port the original character-display composition or every
sub-CPU peripheral behavior.

### Display CPU interrupt dispatch and sub-I/O RAM port

The original `SubCpu.cpp::SimONE` checks pending sub IRQ, the `AFE4`
FIRQ latch and a periodic NMI after the first `AFE0` write. The
display 6809 adapter now invokes `SimIRQ`, `SimFIRQ` and `SimNMI`
before executing its next instruction, with the original initial
NMI threshold of 60000 and a CPU-cycle-based clock. Note: the
reference increments its clock for each memory access and adjusts
the NMI threshold dynamically against wall time; the Linux timing
model is thus **not yet cycle-equivalent**.

The original `GetMemByte/SetMemByte` also accesses sub RAM in
`A000-AFFF` except explicitly decoded registers. Linux previously
returned `FF` for much of that aperture. Ordinary sub I/O working
RAM is now readable and writable there; reading `AFE5` acknowledges
the sub IRQ request as in original `GetIoByte`. Tests exercise RAM
readback, reset, IRQ acknowledgment and NMI reset-period defaults.

```sh
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

This does **not** assert that real display ROM boots: sub IRQ request
generation from the main-side `FD05` bit6 and original `105`
ready/interrupt gating are still to be wired, and keyboard host input
and timing require additional integration.

### Sub-CPU keyboard event queue (source SubCpu.cpp)

The original Windows emulator's first `AFE0` write queues key event
`0x0050` and clears its NMI mask. Subsequently, `GetKeyCode`
copies pending keyboard events into `AFEC/AFED`, asserting bit 0 of
`AFE4` for ordinary key codes. Reading `AFED` invokes
`GetKeyCode` to advance the queue.

The Linux subsystem now includes `enqueueKeyCode`, `processKeyCode`,
the one-shot initial `AFE0` key event and the `AFED` queue advancement.
The independent FIFO regression checks these state transitions:

```sh
ctest --test-dir build -R 'display_keyboard_queue|display_io_read_clear' --output-on-failure
```

The original implementation also invokes `GetKeyCode` from a timed
keyboard/vsync event and supplies interrupts through `SimFIRQ` and
`SimNMI`. These *CPU-side interrupt delivery and timing paths* remain
unimplemented and are **not claimed** by the new keyboard FIFO test.
The original ROM may therefore still be black.

### Original display I/O read-clear latches

The reference Windows `Fm11/SubCpu.cpp::GetIoByte` clears three
register latches **on read**: `AFE4` (sub FIRQ), `AFEC`
(keyboard status) and `AFED` (keyboard data). The Linux display
subsystem now reproduces those read-clear semantics, including zero
values after reset and ignoring software writes to these registers.
A regression supplies external latch values using
`signalSubFirq` / `setKeyboardLatches` and verifies one-shot reads.

```sh
ctest --test-dir build -R 'display_io_read_clear|display_cpu6809' --output-on-failure
```

This is **not yet** the whole original keyboard/NMI implementation:
original `GetKeyCode()` processing after `AFED`, NMI timing, and
interrupt delivery remain separate work. Do not interpret passing
this test as proof that original display firmware now boots.

### Stage 10: original display ROM versus synthetic main sender

The stage-8 synthetic main ROM is kept as a **known, deterministic sender**.
Unlike stage 8, however, the sub CPU runs the user's **unmodified
original 20 KiB ROM**. This isolates genuine firmware startup/READY
handling from the main boot ROM and from the synthetic sub-CPU wrapper.

```sh
git pull
cmake --build build -j$(nproc)
python3 tools/make_display_test_roms.py build/test-roms
./build/display_real_rom_probe \\
    build/test-roms/main_text_real_protocol.bin \\
    external/roms/display6809.bin
```

The probe records up to 32 original-sub-CPU AFE0 writes with the
instruction PC, values, main FD05 status, READY/BUSY totals and
the busiest sub-CPU PCs. It also prints FC81/FC82/FC83/FC84 and
FDA0 after a bounded instruction budget. A zero exit code means
**the diagnostic completed**, not that the original firmware has
accepted the stage-8 test command.

The stage-8 main ROM's request format is only a controlled stimulus:
there is **no assertion** that it is the real firmware character
command. Do not use this probe's output to claim the original text
command works. This program intentionally is not a CTest because it
requires the operator's original display ROM. Once the firmware's
own command parsing is established, write a genuine command sender
and its regression test rather than inventing any new protocol.

### Stage 9: audit actual ROM display-command dispatch (preparation)

Do **not** treat the stage-8 `CAA0` byte-receive routine as proof of the
real firmware's text *command* semantics. The reference `SubRom.h`
begins with `B000: JMP B018`, and the ROM contains eight handler
addresses at `B07C-B08B`. Those are actual firmware entry/dispatch
structures, unlike the synthetic byte `A5` used in stage 7.

The first step is a reproducible, read-only audit of the operator's
original display ROM:

```sh
python3 tools/inspect_display_rom.py external/roms/display6809.bin
```

The tool validates the ROM size and startup signature, prints the eight
handler targets and the genuine `CAA0` bytes, and **does not infer**
unknown display command codes. Next, trace the original main-ROM sender
and the `B018/B08C` command decoding and identify the actual character
output handler before producing the stage-9 synthetic-ROM test.
No new communication behavior or firmware-specific display test is
claimed by this inspection-only commit.

### Original SubRom receive handshake: stage 8

This stage does **not invent a new receive protocol**. The generated
display ROM includes, byte-for-byte, the receive subroutine at
`$CAA0-$CACA` from the reference `Fm11/SubRom.h`, relocated
unaltered to `$B100` (all its branches are relative).

The reference `Fm11/SubCpu.cpp` maps the display CPU's `$9F80-$9FFF`
to main shared RAM `$FC80-$FCFF`. Its `$AFE0` bit 0 sets or
clears main-side `$FD05` bit 7.

- The original subroutine signals READY through `AFE0`, polls
  `9F82`, signals BUSY, clears bit 7 in `9F81`, then returns
  the payload from `9F82`.
- The synthetic main ROM waits for READY at `FD05`, writes a
  request with bit 7 set to `FC81` and ASCII `M` to `FC82`,
  and waits until the genuine subroutine clears the request bit.
- A **test-only wrapper**, not original firmware, writes the
  returned character to text RAM and signals display completion
  through `9F83`, after which main enables `FDA0=03`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_real_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_real_protocol.bin \\
    --rom build/test-roms/display_text_real_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected: a white `M` at the screen origin. This validates the
**original receive-handshake routine**, but not yet the actual
original firmware's display-command dispatcher or its character
command encoding. The real boot ROM is still not used in this test.

### Synthetic main-to-sub 6809 text request: stage 7

This is the first test where the **main 6809 requests text via shared
RAM**, rather than the display CPU unconditionally painting a predefined
message.

1. Sub 6809 initializes CRTC then writes `AFE0=1` (READY).
2. Main 6809 waits for `FD05 bit7=0`, writes command `A5` to
   `FC80` and ASCII `M` to `FC81`.
3. Sub 6809 reads the matching shared addresses `9F80/9F81`, copies
   the requested character into its text RAM and acknowledges by writing
   `9F82=66`.
4. Main 6809 detects the acknowledgement, enables text using
   `FDA0=03`, and sets a completion marker `FC83=77`.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_protocol --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_protocol.bin \\
    --rom build/test-roms/display_text_protocol.bin \\
    --ank-rom external/roms/ank.bin --high
```

Expected SDL display: a white `M` at the upper-left of the screen.
The regression checks the READY transition, both CPUs' shared
request/acknowledgement, text/CRTC state, and 200/400-line pixels.
This is a deliberately minimal *test protocol*, not yet the original
firmware's command encoding. Original ROM boot is not part of this stage.

### Synthetic 6809 CRTC display-start scrolling: stage 6

Two sub-CPU ROMs write **identical** `FIRST LINE` and `SECOND LINE`
text into row 0 and row 1 of the 80-column text RAM. The first
ROM sets CRTC register 13 to 0; the second sets it to 80 text
cells (one full row). The main CPU ROM enables text as in stage 1.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_scroll --output-on-failure

# First row shows FIRST LINE, second row SECOND LINE:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_top.bin \\
    --ank-rom external/roms/ank.bin --high

# After CRTC start-address change, SECOND LINE moves to the first row:
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_scroll_scroll.bin \\
    --ank-rom external/roms/ank.bin --high
```

This stage checks both 200- and 400-line framebuffer results with
generated distinguishable glyphs and verifies that the **same**
physical text RAM contents are present in both variants. It tests
the current CRTC text-start mapping, not original firmware boot.

### Synthetic 6809 display-layer selection: stage 5

This test makes the graphics region **large enough to see**.
The common display ROM writes yellow text `LAYER TEST` plus a 640x200
blue/black alternating-pixel stripe field, and the main ROM selects
which layers are enabled via `FDA0`:

| Main ROM | FDA0 | Expected appearance |
| --- | --- | --- |
| `main_layers_text.bin` | `03` | Yellow text on black |
| `main_layers_graphics.bin` | `0C` | Blue/black stripes across the screen, no text |
| `main_layers_both.bin` | `0F` | Yellow text over blue/black stripes |

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_layers --output-on-failure

# Change 'both' to 'text' or 'graphics' for the other two modes:
./build/fm11-sdl --main-rom build/test-roms/main_layers_both.bin \\
    --rom build/test-roms/display_layers.bin \\
    --ank-rom external/roms/ank.bin
```

The three new tests check actual main/sub-6809 writes and pixels for
both 200- and 400-line scanout. The 400-line view includes the
graphics plane's upper area, which is still blank because the test
draws only the first 200 scanlines.

### Synthetic 6809 graphics/text overlay: stage 4

This stage uses a **new pair of synthetic 6809 ROMs**. The main CPU
enables both text and graphics (`FDA0=$0F`). The display CPU sets
80-column/25-row CRTC text mode, writes a yellow `T` and a reverse
red `R` into text RAM, and paints two blue graphics pixels underneath
their cells using the graphics plane registers.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_overlay --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_overlay.bin \\
    --rom build/test-roms/display_text_overlay.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest runs the same ROMs with a generated minimal ANK font, and
checks both 200- and 400-line frames. The `T` glyph overlays the first
blue pixel in yellow, while the reverse-video `R` leaves the second
blue pixel visible through the glyph hole and paints the other pixels
of that character cell red. This tests actual output blending rather
than simply checking whether the two memory regions were written.

### Synthetic 6809 text attributes: stage 3

After validating basic text and 40/80-column geometry, the next
self-contained 6809 ROM writes six text cells with distinct attributes:

- `R`, `G`, `B`: red, green and blue character colors.
- `R` (fourth cell): reversed red, with the cell background filled.
- `H`: bright-white attribute.
- `W`: normal-white attribute.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_attributes --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_attributes.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest uses a generated minimal ANK glyph ROM, checking the text
attribute bytes written by the sub 6809 and the composited pixel colors
in both 200- and 400-line views. The SDL command uses the actual
extracted ANK ROM to show the characters.

F1 only changes the *200/400-line view*, not the CRTC 40/80-column
setting. In the current rendering model the 200-line view downsamples
400 logical character scanlines, so text appears vertically compressed.
Fidelity to original hardware scanline behavior remains to be verified.

### Synthetic 6809 text geometry: 40/80 columns and 200/400 lines

Stage 2 reuses the working stage-1 main ROM and two new display
6809 ROMs. The display CPU sets CRTC R1 to 20 (40 columns) or
40 (80 columns), R6 to 25 rows and writes `40/80 COL TEST`
into text RAM. The tests check both 640x200 and 640x400 framebuffer
geometry, using a deterministic test-only ANK font.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R 'display_binary_text_(40|80)col' --output-on-failure

# 40 columns (16 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_40col.bin \\
    --ank-rom external/roms/ank.bin --high

# 80 columns (8 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_80col.bin \\
    --ank-rom external/roms/ank.bin --high
```

Press F1 in SDL to compare the 200- and 400-line representations.
The expected 40-column letters are twice as wide as the 80-column
letters. This stage confirms the current geometry model; fidelity
to all original CRTC/scanline behavior must still be compared with
the reference emulator.

### Synthetic 6809 text display: stage 1

Start with a small **end-to-end** text test before original firmware. Both
processors run real 6809 binary instructions: the main CPU writes `$FDA0=3`
(text enabled); the display CPU initializes the CRTC to 80x25, writes
`HELLO FM-11` as alternating character/attribute bytes starting at
`$8000`, and places `$66` in shared RAM on completion.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_stage1 --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_stage1.bin \\
    --ank-rom external/roms/ank.bin --high
```

The headless regression uses a separate generated font
`ank_text_stage1.bin` with simple known glyph pixels, and checks
CPU-written text RAM, CRTC values, and output pixels. The SDL run above
uses the **actual** extracted ANK ROM, so `HELLO FM-11` should appear
at the top left. If the original ANK ROM is not available, the same SDL
command can use `build/test-roms/ank_text_stage1.bin` to show simple
glyph marks rather than letters.

This is **stage 1 only**: plain text on black. After confirming its
SDL result, later stages will cover colors, reverse attributes and
graphics/text overlays separately.

### Reference ANK text composition (first implementation)

The original `Fm11/fm11.hlsl` composites two-byte character/attribute
cells from the sub-CPU text RAM, with the CRTC start address and 8x16
ANK glyphs. That operation is now available in the SDL frontend, including
text reverse video, color attributes, and the 640x200/400 output sizes.
The software scanout matches the shader's LSB-first glyph bit ordering.

The reference archive includes `Fm11/CharRom.h`; convert its embedded
4096-byte ANK array without committing those font bytes:

```sh
python3 tools/extract_ank_rom.py /path/to/Fm11/CharRom.h external/roms/ank.bin
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --ank-rom external/roms/ank.bin
ctest --test-dir build -R display_text_composition --output-on-failure
```

Providing `--ank-rom` loads font data; it does **not** force the
text layer on when a main CPU is present. The original display control
register `$FDA0` enables text with bits 0/1 and graphics with bits
2/3. The renderer also applies the eight programmable palette entries
from `$FD98-$FD9F` and the original 16-entry color table.
The current software scanout supports attribute bit 5 (bright/bold)
and reverse video; blink, cursor and clip state remain future work.
Without a main CPU, specifying `--ank-rom` still explicitly enables
text for standalone diagnostics.
Other original display behavior, including blink, cursor and clip, remains to be ported.
Without `--ank-rom`, the established graphics-only regression frames
remain unchanged.

### Full-frame binary display test and SDL preview

To check the complete raster path without original firmware, generate
the synthetic ROM pair and run the test:

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_pattern --output-on-failure
./build/display_binary_pattern build/test-roms/main_pattern.bin build/test-roms/display_pattern.bin
```

The main 6809 sends a command through shared RAM; the display 6809
writes all 16,000 bytes of the 640x200 plane with an alternating pixel
pattern in all three colors, then acknowledges completion. The test
checks **every rendered pixel**, not just the first byte.

To inspect the same pattern visually in the SDL frontend:

```sh
./build/fm11-sdl --main-rom build/test-roms/main_pattern.bin \\
    --rom build/test-roms/display_pattern.bin
```

This should display narrow alternating white and black vertical lines
across the entire 640x200 framebuffer. It is a deliberately artificial
bring-up image, not the FM-11 firmware's normal boot screen.

### Display-generated ATTENTION (original I/O semantics)

Original `Fm11/SubCpu.cpp`, `CSubCpu::GetIoByte`, case
`$AFE7`, raises main-CPU ATTENTION when the display CPU **reads**
that address. This is distinct from the earlier host-injected event
tests. The implementation now connects that read to the existing main
`$FD04` event latch.

A pair of tiny 6809 binaries verifies that the display CPU's `LDA
$AFE7` triggers an event that the main CPU receives with `LDA
$FD04` and then acknowledges by reading `$FD04` again:

```sh
ctest --test-dir build -R display_binary_afe7 --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_afe7 build/test-roms/main_afe7.bin build/test-roms/display_afe7.bin
```

### HALT and BUSY binary regression

```sh
ctest --test-dir build -R display_binary_halt --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_halt build/test-roms/main_halt.bin build/test-roms/display_halt.bin
```

This runs real, standalone MC6809 binary instructions on both CPU
cores: main writes `$FD05` to halt the display CPU, writes a marker to
shared `$FC80`, and releases HALT. The test checks that the display
PC remains frozen during HALT; after release, the display ROM sets
`$AFF0` (BUSY) and writes a shared-RAM marker at `$9F82`.
There is no dependency on an original ROM dump.

ATTENTION's sub-CPU hardware trigger is still not mapped. The existing
`MainBoard::signalAttention()` path does **not** establish how an
actual display ROM should raise the event; this test deliberately does
not fabricate one.

The main CPU program at `$5000` writes `$80` to `$FC80`.
The display CPU program at `$B000` waits for the same value at
`$9F80`, then writes `$80` to VRAM through the three color planes.
The test asserts shared RAM, physical VRAM and the first rendered white
pixel. CTest creates temporary binaries automatically; the manual commands
above keep them under `build/` for inspection.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current scope

- Portable fixed-width compatibility types for legacy `BYTE`, `WORD`, `DWORD`, and `LONG` usage.
- Existing `Sim09` 6809/6309 instruction core compiled as portable C++.
- Existing `SimZ80` instruction core compiled as portable C++.
- Legacy MFC register-window callbacks removed from the portable core.
- Z80's FM-11-specific memory/MMR wiring is deliberately not embedded in the CPU core. The initial portable hook uses the supplied flat `Memory` buffer; board-specific address translation will be added in the CPU-board layer.

No SDL frontend or complete FM-11 machine is implemented yet.

## Headless serial CPU runner (experimental)

On Linux, `fm11-headless` runs an MC6809/HD6309 raw binary with
standard input/output connected to the main RS-232C byte queues.
It is intended for early serial driver bring-up, **not** NitrOS-9 booting:
there is no IPL, disk controller, OS image loader or execution pacing yet.

```sh
./build/fm11-headless --binary program.bin --load 0x1000 --pc 0x1000 --cpu 6809 --clock 2000000
```

Use `--instructions 100000` for a bounded smoke run. Ctrl-C stops an
unbounded run. Input is processed without canonical line buffering when
attached to a TTY; the original terminal settings are restored on exit.

## FM-11 NitrOS-9 boottrack bring-up

The floppy IPL sources in `sueyasu/fm11-nitros9` load the first
stage from track 0, side 0, sector 5 onwards. A raw, **contiguous**
boottrack payload (not a whole floppy image) may be supplied directly:

```sh
./build/fm11-headless --boottrack boottrack.bin --level 1 --cpu 6809
./build/fm11-headless --boottrack boottrack-l2.bin --level 2 --cpu 6809
```

Level 1 expects exactly 17 × 256 bytes at `$EB00`, starting at
`$EB02`. Level 2 expects exactly 21 × 256 bytes at `$E100`,
starting at `$E102`. This bypasses ROM IPL/FDC sector loading only;
it does **not** emulate any subsequent boot-time disk reads. Normal
NitrOS-9 startup therefore requires further disk-controller support.
Use `--instructions N` for bounded diagnostic runs.

## D88 floppy images

The headless runner accepts either a 327680-byte raw 2D image or a D88
2D image via `--floppy`. D88 sectors are selected by their C/H/R IDs,
not by assuming they appear in sector-number order. The initial
read-only MiniFDC implementation requires a complete 40-track, two-sided,
16-sector/side disk with 256-byte sectors and rejects malformed,
missing, duplicated or non-2D sector records.

## Host monitor and serial PTY

The headless emulator starts **halted**. Its original terminal is a
line-oriented monitor, not the guest console. Startup prints a serial
PTY path, e.g. `/dev/pts/5`; connect from another terminal with
`screen /dev/pts/5` (or `picocom`). The PTY slave is configured for
raw 8-bit communication. Guest Ctrl-C passes through the serial PTY
without terminating the emulator.

At the `fm11>` prompt, use `c` to run, `stop` to pause,
`s` to step one instruction, `r` for registers, `m HEXADDR [COUNT]`
for CPU-logical memory, `mp HEXADDR [COUNT]` for physical memory,
`reset` to reset and reload the supplied bootstrap, and `q` to quit.
The monitor is available while the CPU runs. `--instructions` pauses the
CPU at the limit instead of terminating the monitor.


### CPU idle waiting (6809/6309)

The headless runner executes instructions **without a real-time CPU throttle**
while running. Starting with the parallel-PTM branch, the `c` command also
starts the **same `Machine::startRealtimePtm()` worker used by SDL**.
The shared MC6840 timer advances from host `steady_clock` at 9700 Hz,
independently of guest CPU instructions. CPU instruction cycles and the
`SYNC`/`CWAI` wait loop do **not** advance this timer a second time.
When a guest executes `SYNC` or `CWAI`, the host waits using `poll()`
for PTY/monitor activity or the next timer edge. The CPU adapter samples
the PTM's published IRQ level at instruction/wait boundaries.

`CWAI` keeps its pre-stacked interrupt frame; `SYNC` does not stack.
A masked IRQ wakes `SYNC` without entering its vector, while `CWAI`
requires an accepted interrupt. The monitor `stop`/`q` commands stop
and join the timer worker, and `c` restarts it with a fresh host-time
baseline. `reset` while halted resets the shared timer along with the CPU
card. Single-step while halted uses the legacy deterministic cycle clock.
This is **not** a cycle-exact real-time CPU emulation mode.

### Testing without USART Tx interrupts

To diagnose persistent CPU load while NitrOS-9 is waiting for input, launch
`fm11-headless` with `--no-tx-irq`. This temporarily suppresses only the
8251-compatible **transmit** interrupt request; serial transmission, reception
and receive interrupts remain operational. The default behavior is unchanged.
Compare host CPU usage at the Shell prompt with and without the switch.
This is a diagnostic option, not a verified FM-11 hardware correction.

### CPU idle diagnostics

Add `--idle-log idle.csv` to write a CSV sample approximately once per host
second. The samples contain executed instructions, SYNC/CWAI entries, measured
host wait duration for each state (milliseconds), IRQ entries accepted by the
CPU, PTM IRQ assertions, and current PC/CC/wait state. Counts are interval
deltas, not cumulative. Compare samples at the NitrOS-9 shell prompt.

## Boot from a 2D floppy image

With an FM-11 NitrOS-9 bootable raw 2D or D88 floppy image, no separate
boottrack file or OS level is needed:

```sh
./build/fm11-headless --floppy disk.d88 --cpu 6809
```

The FM-11 ROM's initial floppy load is simulated: track 0, side 0,
sectors 1 and 2 are copied to `$0400` and execution starts at `$0400`.
The IPL contained in those sectors then loads the boottrack using the
emulated MiniFDC. This is a disk IPL handoff, not execution of the
original FM-11 system ROM. `--boottrack FILE --level 1|2` remains available.
The monitor's `reset` command reloads the disk IPL from the mounted image.

## 8088/V20 boot-ROM trace (experimental)

The separate diagnostic runner executes an external raw **4096-byte** FM-11
8088 boot-ROM image, beginning at the x86 reset vector FFFF:0000. No ROM
binary is included in this repository. This is a CPU-card diagnostic, not
full FM-11 boot: the remaining hardware and CPU instructions are incomplete.

```sh
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu 8088 --instructions 10000 --trace
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu v20 --instructions 10000
```

The runner prints the first unimplemented opcode with its physical PC and
instruction number; `--trace` logs per-instruction register values. For now
8088 and V20 share the same implemented instruction subset. This tool does
not modify the 6809/6309 headless boot path.


## Experimental parallel CPU/PTM branch

The `dev/parallel-cpu-ptm` branch is an incremental migration; **stage 1
does not start any emulation worker threads**. CPU, display CPU, PTM and
frame rendering still run through the existing single-thread scheduler.

Stage 1 adds `WorkerGroup`, a cooperative lifecycle controller for future
CPU and PTM workers. `start(step)` registers a finite work step;
`pause()` waits until **all** workers have parked at a step boundary;
`resume()` releases them, and `stop()` joins the threads. Worker
exceptions stop the group and can be rethrown on the owner thread via
`rethrowWorkerFailure()`. Do not call `stop()` or destroy the group from
one of its own worker threads, and do not use unbounded blocking steps.

The SDL-to-display-CPU keyboard ring buffer now has a narrow mutex. **This
does not make `DisplaySubsystem` or `MainBoard` generally thread-safe.**
Shared RAM at $9F80, FD05/AFE0 BUSY and HALT latches, main/sub IRQ and
FIRQ state, PTM registers, and VRAM/CRTC access have side effects. These
must be assigned clear owners and access ordering before starting concurrent
CPU/PTM work (stages 2-4). In particular, OpenGL currently reads display
memory directly and requires an immutable render snapshot once the display
worker exists. Do not wrap every RAM access in a global mutex; do not enable
unprotected concurrent guest execution.

Stage 2: SDL and the interactive `fm11-headless` runner use a
**dedicated real-time PTM worker**. SDL starts it after initialization;
headless starts it on monitor `c` and stops it on `stop`/`q`. The MC6840 advances from host `steady_clock` at its 9700 Hz
input rate, regardless of `--speed` or main CPU instruction cycles. PTM
register accesses catch up to the exact host time under a narrow mutex;
a 1 ms worker polling interval supplies asynchronous IRQs even when the
CPU is in SYNC/CWAI. The IRQ level is published atomically; only the CPU
thread changes the CPU adapter's IRQ aggregator. SDL pause parks the PTM
worker and rebases its clock on resume, and reset stops/restarts the worker.

Stage 3: the SDL frontend starts a **separate display sub-CPU worker**.
It advances the display 6809 directly from host `steady_clock` at 2 MHz,
independent of the main CPU and the PTM. The former
`advanceDisplayCpuCycles()` / `advanceDisplayTime()` paths are disabled
while the display worker runs, but remain for deterministic legacy tests.
The SDL SYNC/CWAI path no longer advances guest virtual time. A narrow
display-bus mutex serializes each sub-CPU instruction with main-CPU
mailbox/FD05 operations; rendering holds the same lock while reading the
current VRAM/CRTC state. Display ATTENTION is published through an atomic
event latch. SDL pause/resume controls both workers and rebases the display
clock on resume. Headless and other noninteractive harnesses retain their
existing deterministic display scheduler unless explicitly started.
The PTM worker must be stopped before manipulating or resetting shared
device state directly. This is not yet a fully parallel CPU/display machine;
all other shared state still belongs to the single emulation thread.

Run `ctest --test-dir build --output-on-failure`, including `worker_group`,
`realtime_ptm` and `realtime_display_cpu`. Verify NitrOS-9 boot, shared
mailbox handshakes and wall-time clock on SDL and `fm11-headless`.
Both PTM and display clocks are **independent of** `--speed`; only the
main CPU still uses SDL pacing.

### Stage 4: main CPU worker

SDL no longer executes main 6809/Z80 instructions on its event/render thread.
A dedicated `WorkerGroup` CPU worker owns the existing
`Machine::executeOne()` path, including FD05 CPU-card selection, IRQ
sampling, and the `SdlCpuPacer` clock. `SYNC/CWAI` only wake when their
interrupt conditions are satisfied; the PTM and display workers continue
independently. The main CPU worker services the serial PTY before and after each bounded
instruction batch, so serial input never waits for SDL's rendering loop or
contends for a CPU mutex. A separate narrow main-CPU mutex protects frontend
CPU diagnostics. Display shared RAM/FD05 transactions preserve
the CPU-to-display lock order. F2 pauses all three workers; resume rebases
CPU pacing rather than replaying paused time. Rendering copies display
memory under its own lock, then presents outside the lock.

The deterministic, nonthreaded `Machine::executeOne()` API remains
available to existing headless runners and core regression tests. The
`parallel_cpu_display` test exercises three workers and display snapshots.

### Stage 5: remove the old scheduler from the parallel execution path

The legacy main-CPU-cycle-coupled display scheduler has been extracted from
`Machine.cpp` into `src/core/DeterministicDisplayScheduler.h`. It is retained
only for single-threaded headless utilities and regression tests; while the
display worker runs, CPU instructions and SDL rendering never execute that
scheduler. This preserves deterministic tests without maintaining two display
clocks in the interactive frontend.

The hot path caches primary/optional CPU-card indices at attachment time,
and FD05 writes now update the active CPU selection through a dedicated
callback, independently of diagnostic observers. The active CPU's core pointer,
CPU kind and post-instruction dispatch are cached when the selected board
changes; ordinary instructions no longer perform per-instruction RTTI or
FD05 selection polling. Manual selection, reset, and board attachment
also refresh the cache. SDL's CPU
worker publishes its instruction count once per batch instead of performing
an atomic increment per instruction, and takes the diagnostic CPU-state mutex
only when a frontend speed or CPU diagnostics consumer is enabled. The display
worker amortizes its display-bus lock across at most 16 instructions, retaining
short mailbox/FD05 access opportunities for the main CPU. PTM frequency and
clock semantics are unchanged by these performance changes.

### PTM channel-specific IRQ acknowledgement

PTM status reads now acknowledge each channel's pending expiry independently.
An unrelated timer channel reaching terminal count must not reactivate an
already acknowledged IRQ from another channel. The legacy status latch remains
visible until that channel's counter register is read, preserving existing
software behavior. The `main6809_ptm` regression exercises this specific
cross-channel reassertion case. This correction does not alter the 9700 Hz
input clock or the 50 Hz OS clock reload value, and does not by itself establish
why SDL clock time was previously accelerated.

### OpenGL snapshot and display worker speed

SDL `--speed 1..100` applies the multiplier to the main CPU and
display sub-CPU, each starting from its configured 2 MHz hardware clock.
The display sub-CPU is provisionally capped at 8 MHz, so `--speed 4`
and above target the same display clock. With `--speed 0`, the main CPU
runs unlimited while the display sub-CPU remains paced at 8 MHz.
These are pacing targets, not guaranteed achieved throughput. The
headless/deterministic display scheduler remains at its configured
base clock. The PTM remains tied to host steady-clock time.

The OpenGL frontend still captures a coherent copy of display VRAM,
text/ANK data, CRTC state, display control and palette while holding
the display-bus mutex. All OpenGL texture uploads, uniforms, rendering
and buffer swaps take place after the mutex has been released.

### Independent display bus and mailbox locking

The main CPU no longer takes the display-CPU/VRAM recursive mutex when
accessing its 128-byte FC80-FCFF mailbox. The shared bytes are guarded by
a dedicated mutex on both the main-side FC80-FCFF path and the display
CPU's 9F80-9FFF memory accesses. Shared transaction tracing is serialized
under that same mutex. The shared-write revision is atomic.

FD05 BUSY status and the sub-CPU busy latch can be read atomically
without acquiring the display mutex. FD05 writes that change sub-CPU
state still use the display mutex. The wake-observer callback has a
separate lifetime mutex so mailbox writes can wake the display worker
without holding the display lock. Display-CPU instruction slices and
video snapshots retain their existing display mutex.

### Avoid redundant main-CPU PTM IRQ synchronization

The SDL main-6809 worker samples the PTM IRQ before each instruction for
SYNC/CWAI wakeup and interrupt acceptance. It now passes that fact into
`Machine::executeOne()`, which skips the redundant post-instruction PTM
aggregation only while the PTM is in independent real-time mode. Headless
and deterministic stepping retain their post-instruction synchronization,
as do optional CPU-card execution paths. The main-6809 IRQ aggregator
also avoids rebuilding FD03/FD2C when the published PTM IRQ source level
has not changed; explicit IRQ-mask and other-source updates continue to
recalculate their status at the point of the write.

### Main 6809 RAM page lookup optimization

The main MC6809/HD6309 CPU caches the host-memory base address of each
logical 4 KiB RAM page, avoiding repeated MMR context translation and
physical-memory bounds checking on every ordinary RAM access. FD80-FD8F
writes update the affected active page, while FD90 context changes and FD93
MMR enable changes refresh the 16-entry mapping. Reset and replacement of
the shared 1 MiB physical RAM backing also rebuild the cache. The boot-ROM
overlay, 7C00-7FFF window, FC00-FFFF fixed/IO aperture, and direct physical
DMA accesses continue through their existing handlers. The `main6809_mmr`
regression covers page remapping, contexts, enable toggles, window override,
and shared-memory replacement.

## Intel 8088 hardware-reference instruction tests

The standalone `fm11-8088-singlestep` runner can compare
`Main8088Board::Core::executeOne()` with the real-CPU
[SingleStepTests/8088 V2](https://github.com/SingleStepTests/8088)
instruction test suite (external data, not bundled). Build normally, then:

```sh
ctest --test-dir build -R main8088_singlestep_smoke --output-on-failure
git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
git -C external/8088-tests sparse-checkout set v2
python3 tests/run_8088_singlestep.py --opcode 00 --limit 100
python3 tests/run_8088_singlestep.py --all --limit 100
```

See [docs/8088-singlestep.md](docs/8088-singlestep.md) for full-case
execution, grouped ModR/M opcodes, failure reports and prefetch limitations.
