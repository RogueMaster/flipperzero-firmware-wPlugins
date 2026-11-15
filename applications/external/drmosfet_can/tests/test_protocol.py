"""Run the real compiled Cortex-M4 GVRET parser in Unicorn."""

import os, struct, unittest
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import (
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_SP,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
)
from elftools.elf.elffile import ELFFile


class Core:
    def __init__(self):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.uc.mem_map(0x08000000, 0x100000)
        self.uc.mem_map(0x20000000, 0x100000)
        with Path(os.environ["TEST_ELF"]).open("rb") as f:
            elf = ELFFile(f)
            self.symbols = {
                s.name: s["st_value"]
                for s in elf.get_section_by_name(".symtab").iter_symbols()
            }
            for seg in elf.iter_segments():
                if seg["p_type"] == "PT_LOAD":
                    self.uc.mem_write(seg["p_vaddr"], seg.data())
        self.call("test_reset")

    def call(self, name, *args):
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), args):
            self.uc.reg_write(reg, value)
        self.uc.reg_write(UC_ARM_REG_SP, 0x200FF000)
        self.uc.reg_write(UC_ARM_REG_LR, 0x080FF001)
        self.uc.emu_start(self.symbols[name] | 1, 0x080FF000, count=1000000)
        assert self.uc.reg_read(UC_ARM_REG_PC) == 0x080FF000
        return self.uc.reg_read(UC_ARM_REG_R0)

    def number(self, name):
        return struct.unpack("<I", self.uc.mem_read(self.symbols[name], 4))[0]

    def feed(self, data):
        self.uc.mem_write(self.symbols["scratch"], bytes(data))
        self.call("test_feed", self.symbols["scratch"], len(data))

    def replies(self):
        return bytes(
            self.uc.mem_read(self.symbols["replies"], self.number("reply_size"))
        )


def frame(id=0x123, bus=0, data=b"\x11\x22", dlc=None):
    return (
        b"\xf1\0"
        + struct.pack("<I", id)
        + bytes([bus, len(data) if dlc is None else dlc])
        + data
    )


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.c = Core()

    def test_handshakes(self):
        for cmd, expected in [
            (1, b"\xf1\1\x78\x56\x34\x12"),
            (6, b"\xf1\6\1" + struct.pack("<I", 500000) + bytes(5)),
            (7, b"\xf1\7\x60\1\1\0\0\0"),
            (9, b"\xf1\11"),
            (12, b"\xf1\14\1"),
            (13, b"\xf1\15\0"),
        ]:
            self.c = Core()
            self.c.feed(bytes([0xE7, 0xF1, cmd]))
            self.assertEqual(self.c.replies(), expected)

    def test_fragmented(self):
        for b in frame():
            self.c.feed(bytes([b]))
        self.assertEqual(self.c.number("frame_count"), 1)
        self.assertEqual(self.c.number("lastid"), 0x123)

    def test_extended_and_zero_dlc(self):
        self.c.feed(frame(0x9FFFFFFF, data=b""))
        self.assertEqual(self.c.number("lastext"), 1)
        self.assertEqual(self.c.number("lastid"), 0x1FFFFFFF)
        self.assertEqual(self.c.number("lastdlc"), 0)

    def test_invalid_dlc_consumed_and_resync(self):
        self.c.feed(frame(data=b"\xf1" * 15, dlc=15) + b"\xf1\11" + frame())
        self.assertEqual(self.c.number("frame_count"), 1)
        self.assertEqual(self.c.replies(), b"\xf1\11")

    def test_reject_other_bus_and_invalid_id(self):
        for data in [frame(bus=1), frame(bus=4), frame(id=0x800), frame(id=0xE0000123)]:
            self.c.feed(data)
        self.assertEqual(self.c.number("frame_count"), 0)
        self.c.feed(frame(id=0x7FF))
        self.assertEqual(self.c.number("frame_count"), 1)

    def test_bitrate_flags(self):
        self.c.feed(b"\xf1\5" + struct.pack("<II", 0x80000000 | 250000, 0) + b"\xf1\6")
        self.assertEqual(self.c.number("configured"), 250000)
        self.assertEqual(struct.unpack("<I", self.c.replies()[3:7])[0], 250000)

    def test_unsupported_bitrate_keeps_actual(self):
        self.c.feed(b"\xf1\5" + struct.pack("<II", 123456, 0) + b"\xf1\6")
        self.assertEqual(self.c.number("configured"), 0)
        self.assertEqual(struct.unpack("<I", self.c.replies()[3:7])[0], 500000)

    def test_encode(self):
        n = self.c.call("test_encode", 0x1FFFFFFF, 1, 8)
        data = bytes(self.c.uc.mem_read(self.c.symbols["scratch"], n))
        self.assertEqual(
            data, b"\xf1\0\x78\x56\x34\x12\xff\xff\xff\x9f\10" + bytes(range(8)) + b"\0"
        )
        self.assertEqual(self.c.call("test_encode", 0x800, 0, 8), 0)
        self.assertEqual(self.c.call("test_encode", 0x123, 0, 9), 0)

    def test_many_frames(self):
        for i in range(256):
            self.c.feed(frame(i, data=bytes(range(8))))
        self.assertEqual(self.c.number("frame_count"), 256)


if __name__ == "__main__":
    unittest.main(verbosity=2)
