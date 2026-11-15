#!/usr/bin/env python3
"""V20 ROL4/ROR4: AL high nibble is undefined, not an exact result."""
import struct
import unittest
from run_v20_singlestep import pack_test

REGS={
    "ax":0x1234,"bx":0,"cx":0,"dx":0,
    "sp":0x8000,"bp":0,"si":0,"di":0,
    "es":0,"cs":0x1000,"ss":0,"ds":0,
    "ip":0,"flags":0x0002,
}

def ax_mask(data):
    case={
        "bytes":data,
        "initial":{"regs":REGS,"ram":[]},
        "final":{"regs":{},"ram":[]},
    }
    packed=pack_test(case,0xffff)
    # 28 initial/expected 16-bit register words precede the masks.
    assert struct.unpack_from("<H",packed,56)[0]==0xffff
    return struct.unpack_from("<H",packed,58)[0]

class V20NibbleRotateMasks(unittest.TestCase):
    def test_undefined_upper_al(self):
        for opcode in (0x28,0x2a):
            with self.subTest(opcode=opcode):
                self.assertEqual(ax_mask([0x0f,opcode,0xc0]),0xff0f)
                self.assertEqual(ax_mask([0x2e,0x0f,opcode,0x06,0,0]),0xff0f)

    def test_defined_bits_preserved_elsewhere(self):
        for data in ([0x0f,0x10,0xc0],[0x0f,0x31,0xc0],
                     [0x28,0xc0],[0x2a,0xc0],[0x90]):
            with self.subTest(data=data):
                self.assertEqual(ax_mask(data),0xffff)

if __name__=="__main__":
    unittest.main()
