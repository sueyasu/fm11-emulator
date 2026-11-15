#!/usr/bin/env python3
"""Verify dynamically masked 8088 undefined OF behavior in SingleStepTests."""
import struct
import unittest

from run_8088_singlestep import pack_test

REGS = dict(ax=0x1234, bx=0, cx=0, dx=0, sp=0x1000, bp=0, si=0, di=0,
            es=0, cs=0x1000, ss=0, ds=0, ip=0, flags=0x0802)


def mask_for(opcode, cl, prefixed=False):
    regs = dict(REGS, cx=cl)
    bytes_ = ([0x2e] if prefixed else []) + [opcode, 0xc0]
    case = {
        "bytes": bytes_,
        "initial": {"regs": regs, "ram": []},
        "final": {"regs": dict(regs), "ram": []},
    }
    encoded = pack_test(case, 0xffff)
    # First 56 bytes are initial/final register snapshots (14 words each).
    return struct.unpack_from("<H", encoded, 56)[0]


class ConditionalOfMask(unittest.TestCase):
    def test_shift_undefined_for_two_or_more(self):
        for opcode in (0xd2, 0xd3):
            for count in (2, 3, 8, 31, 32, 63, 255):
                self.assertEqual(mask_for(opcode, count), 0xf7ff)

    def test_zero_and_one_keep_overflow(self):
        for opcode in (0xd2, 0xd3):
            for count in (0, 1):
                self.assertEqual(mask_for(opcode, count), 0xffff)

    def test_unrelated_instructions_keep_overflow(self):
        for opcode in (0xd0, 0xd1, 0x00, 0x27):
            self.assertEqual(mask_for(opcode, 5), 0xffff)

    def test_segment_override_preserves_detection(self):
        self.assertEqual(mask_for(0xd2, 3, prefixed=True), 0xf7ff)


if __name__ == "__main__":
    unittest.main()
