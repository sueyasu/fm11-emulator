# Stage 2-A: DIP switch configuration (parse-only)

The FM-11 AD2+ manual worksheet defines three banks: BootLoader (8
switches), Communication (8), and System Select (4). Every position can
now be expressed in `[DIPSW]` using the example configuration.

| Bank | Switches | Configuration |
| --- | --- | --- |
| BootLoader | SW1-2 | `boot_drive = 0..3` |
| BootLoader | SW3, SW4 | `boot_sw3`, `boot_sw4 = x|o` (reserved) |
| BootLoader | SW5 | `bubble = true|false` |
| BootLoader | SW6,7,8 | existing `2D_FDD`, `2HD_FDD`, `HDD` |
| Communication | SW1 | `serial_sw1 = x|o` (reserved) |
| Communication | SW2 | `serial_cd = ignore|cd_dsr` |
| Communication | SW3 | `serial_mode = async|sync` |
| Communication | SW4-8 | `serial_baud_sw4_8 = xxxxx` (five x/o characters) |
| System Select | SW1 | `display_lines = 200|400` |
| System Select | SW2 | existing `main_cpu = 6809|8088` |
| System Select | SW3 | `system_sw3 = x|o` (reserved) |
| System Select | SW4 | `printer_sw4 = o|x` (manual specifies o always) |

The raw five-position serial baud selector accepts all 32 combinations,
including those without defined baud rates. The documentation's x/o
notation is preserved literally, without guessing electrical polarity.

**New keys are configuration storage only.** They do not yet affect FD23,
boot-device selection, USART, 200/400-line hardware mode, or printer.
Previously supported keys keep their current behavior, including
`[display].height` as the SDL viewport setting. Legacy `[main]` and
`[boot]` aliases remain supported.

```sh
cmake --build build
ctest --test-dir build -R 'machine_config|machine_dipsw_config' --output-on-failure
```
