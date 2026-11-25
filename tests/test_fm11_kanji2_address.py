"""Check level-2 map independently of any font or proprietary ROM."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from fm11_kanji2_address import jis_to_fm11_level2_selector, jis_to_fm11_level2_slot

cases = {
    (48,1):0x1001,
    (48,32):0x1200,
    (48,33):0x1201,
    (48,65):0x1401,
    (63,94):0x15FE,
    (64,1):0x1601,
    (80,1):0x1C01,
    (83,94):0x1D7E,
}
for code, expected in cases.items():
    assert jis_to_fm11_level2_selector(*code) == expected, (code, expected)
occupied = set()
for ku in range(48,84):
    for ten in range(1,95):
        selector=jis_to_fm11_level2_selector(ku,ten)
        assert (selector & 0x1000) and not (selector & 0xe000)
        slot=jis_to_fm11_level2_slot(ku,ten)
        assert slot not in occupied, (ku,ten,slot)
        occupied.add(slot)
assert len(occupied)==3384
for code in [(47,1),(84,1),(48,0),(83,95)]:
    try:
        jis_to_fm11_level2_slot(*code)
    except ValueError:
        continue
    raise AssertionError(f"incorrectly accepted JIS78 code {code}")
print("PASS: 3384 FM-11 JIS78 level-2 glyph addresses (E8D4-E964)")
