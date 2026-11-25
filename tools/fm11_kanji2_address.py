"""Actual FM-11 second-level JIS78 address conversion.

Reconstructed from the 20,480-byte reference SubRom.h, mapped at
sub-CPU B000-FFFF: E8D4-E964, particularly E928-E963.  AFF4 bit4 is
ROM A16; ROM images store the lower 128 KiB independently.
"""
FIRST_KU, LAST_KU = 48, 83
FIRST_TEN, LAST_TEN = 1, 94

def jis_to_fm11_level2_selector(ku: int, ten: int) -> int:
    if not FIRST_KU <= ku <= LAST_KU or not FIRST_TEN <= ten <= LAST_TEN:
        raise ValueError(f"JIS78 level-2 ku/ten outside ROM range: {ku}-{ten}")
    a = ku + 0x20
    b = ten
    group = 3 * ((a - 0x50) >> 4) + (b >> 5)
    shifted = group << 1
    high = 0x1d if shifted & 0x10 else ((shifted & 0x0e) | 0x10)
    offset = ((a & 0x0f) << 5) + (b & 0x1f)
    selector = (high << 8) | offset
    assert 0x1000 <= selector < 0x2000
    return selector

def jis_to_fm11_level2_slot(ku: int, ten: int) -> int:
    return jis_to_fm11_level2_selector(ku, ten) & 0x0fff
