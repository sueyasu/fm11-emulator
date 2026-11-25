# Stage 2-A: DIP switch configuration (parse-only)

The FM-11 AD2+ manual worksheet defines three banks: BootLoader (8
switches), Communication (8), and System Select (4). Only meaningful,
configurable settings are exposed in `[DIPSW]`; reserved positions
(BootLoader SW3/4, Communication SW1, System Select SW3) are omitted.
The System Select SW4 printer switch is specified as **always o** in the
manual, so it is not exposed as a variable setting.

| Bank | Switches | Configuration |
| --- | --- | --- |
| BootLoader | SW1-2 | `boot_drive = 0..3` |
| BootLoader | SW5 | `bubble = true|false` |
| BootLoader | SW6,7,8 | existing `2D_FDD`, `2HD_FDD`, `HDD` |
| Communication | SW2 | `serial_cd = ignore|cd_dsr` |
| Communication | SW3 | `serial_mode = async|sync` |
| Communication | SW4-8 | `serial_baud_sw4_8 = 0..31` (decimal) |
| System Select | SW1 | `display_lines = 200|400` |
| System Select | SW2 | existing `main_cpu = 6809|8088` |

Example:
```ini
[DIPSW]
boot_drive = 0
bubble = false
2D_FDD = true
2HD_FDD = false
HDD = false
serial_cd = ignore
serial_mode = async
serial_baud_sw4_8 = 4   # 00100 (SW4..SW8, ooxoo): ASYNC 4800 baud
display_lines = 400
main_cpu = 6809
```

The numerical baud switch value corresponds to the **five switch
positions**, with SW4 the most significant bit and SW8 the least
significant bit. `x=1`, `o=0`; for example `4 = 00100 = ooxoo`.
All 32 values are accepted, including patterns for which the manual
does not specify a baud rate. This is a switch configuration, not a
baud-rate number.

**Stage 2-C (in progress):** Communication SW2 now gates USART RxRDY and
receive IRQ when `serial_cd=cd_dsr`, using both CD and DSR inputs.
`ignore` preserves existing behavior. The policy persists through 8251
reset and works with immediate/clocked PTY transport. Physical serial ports
read TIOCMGET, and reject `cd_dsr` when modem inputs are unavailable.
SW3 synchronous mode is rejected for physical POSIX termios ports; the
existing virtual USART sync register setup remains available. SW4-8
continue to provide defined baud rates to clocked PTY and physical
serial ports. Synchronous line timing/clock selection remains to do.

The unrelated DIP keys remain configuration storage only: they do not yet
affect FD23, boot-device selection, 200/400-line hardware mode, or printer.
Previously supported keys keep their current behavior, including
`[display].height` as the SDL viewport setting. Legacy `[main]` and
`[boot]` aliases remain supported. Removed reserved/fixed-switch keys
from the preceding Stage 2-A draft are rejected as unknown settings.

```sh
cmake --build build
ctest --test-dir build -R 'machine_config|machine_dipsw_config' --output-on-failure
```
