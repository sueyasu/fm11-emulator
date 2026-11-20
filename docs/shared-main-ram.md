# Shared main RAM and Z80 mapping

All installed main CPU cards (6809/6309, 8088/V20, Z80) share one 1 MiB
physical RAM backing. Installed RAM is a machine setting (128..1024 KiB);
the existing mirror decoder remains in use. CPU-specific ROM overlays,
card-local SRAM, MMU registers, DMA/PIC registers and display memory are
not merged. A reset does not clear physical RAM.

Machine adopts the first attached RAM-bearing card's backing to preserve
programs preloaded for standalone tests. Later cards attach to it;
load other test programs into shared RAM after attachment. Use
Machine::setRamKiB before running to update all card decoders.

Z80 logical RAM mapping, before installed-RAM mirroring:

| Logical | 6809 master | 8088 master |
|---|---|---|
| 0000-3FFF | 00000-03FFF | 10000-13FFF |
| 4000-BFFF | FD2F[3:0] * 10000h + address | same |
| C000-FBFF | 0C000-0FBFF | 1C000-1FBFF |

FD2F resets to 0, so the 8088 master's fixed windows and banked window
are initially discontinuous. CP/M-80 selects bank 1 to make them
contiguous. The selected main card determines fixed-window mapping, not
the CPU currently executing instructions. FD05 bit 0 yields/resumes the
selected main card and optional Z80. The existing FC00-FFFF I/O handling
is unchanged. Explicit configureFd05Boards determines the master when
both main cards are installed; default wiring prefers 6809 if present.

Hardware reference: https://haserin09.la.coocan.jp/cpucard.html
(reference Z80 card mapping); previously verified on patched Windows
emulator v1.28 with FM-11 CP/M-80 2D and 2HD images.
