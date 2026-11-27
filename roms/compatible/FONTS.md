# Compatible font ROM generation (initial implementation)

Uses Python 3.10+ only. Obtain the original upstream fonts and their
license texts; no Fujitsu ROM image is used as input.

- Terminus 8x16 regular Unicode BDF (ter-u16n.bdf):
  https://terminus-font.sourceforge.net/ (OFL 1.1)
- Shinonome 8x16 JIS X 0201 and 16x16 JIS X 0208 Gothic BDF
  (shnm8x16r.bdf, shnmk16.bdf):
  https://github.com/code4fukui/shinonome-font
- UNSCII-16 (not full), fontfiles/unscii-16.hex:
  https://github.com/viznut/unscii
- k8x12 Unicode BDF:
  https://littlelimit.net/k8x12.htm

Run from the repository root:

```sh
python3 tools/make_compatible_font_roms.py \
  --terminus /path/to/ter-u16n.bdf \
  --shinonome-ank /path/to/shnm8x16r.bdf \
  --shinonome-kanji /path/to/shnmk16.bdf \
  --unscii /path/to/unscii-16.hex \
  --k8x12 /path/to/k8x12.bdf \
  --output build/roms/compatible
```

The ANK image is 4096 bytes; both Kanji banks are 131072 bytes.
ANK uses LSB-first raster bytes. Kanji uses MSB-first interleaved
left/right bytes. First-level placement follows the observed ROM
block organization; second-level placement uses the existing firmware
selector implementation. JIS78 code swaps and four relocated
variants are handled; minor historical glyph-form differences remain.
This is an initial generator: verify real font inputs and stock
display firmware before treating the images as release-ready.

Do not distribute generated ANK data without the original copyright
and license notices of all input fonts. Name the derived ANK font
FM11 Compatible ANK, not Terminus, and distribute under OFL 1.1
subject to the upstream notices. The generator remains MIT.
