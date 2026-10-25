"""Native package alignment checks include RELRO protection boundaries."""
from pathlib import Path
import struct
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/generators'))
from audit_android_native import audit_elf


def elf(load_end=0x8000, alignment=0x4000, relro_end=0x8000):
    data = bytearray(0x8000)
    data[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<Q', data, 32, 64)
    struct.pack_into('<HH', data, 54, 56, 2)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 6, 0, 0, 0, min(0x8000, load_end), load_end, alignment)
    struct.pack_into('<IIQQQQQQ', data, 120, 0x6474e552, 4, 0, 0, 0, 0x1000, relro_end, 1)
    return bytes(data)


class NativeAlignmentContracts(unittest.TestCase):
    def test_aligned_elf_and_relro_only_load_are_accepted(self):
        audit_elf(elf(), 'aligned')
        audit_elf(elf(load_end=0x7100, relro_end=0x7100), 'relro-only')

    def test_4k_load_and_relro_writable_overlap_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'LOAD is not 16 KiB'):
            audit_elf(elf(alignment=0x1000), 'old')
        with self.assertRaisesRegex(ValueError, 'RELRO page overlaps'):
            audit_elf(elf(relro_end=0x7100), 'overlap')
