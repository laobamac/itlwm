import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import verify_meteorlake as vm


def tlv(kind, payload, pad=b"\0" * 3):
    padding = (-len(payload)) & 3
    return struct.pack("<II", kind, len(payload)) + payload + pad[:padding]


class TlvTests(unittest.TestCase):
    def test_rejects_truncated_header(self):
        with self.assertRaisesRegex(vm.VerifyError, "truncated TLV header"):
            list(vm._tlvs(b"\x01\x00"))

    def test_rejects_payload_overrun(self):
        with self.assertRaisesRegex(vm.VerifyError, "payload overruns"):
            list(vm._tlvs(struct.pack("<II", 9, 4) + b"ab"))

    def test_rejects_missing_padding(self):
        with self.assertRaisesRegex(vm.VerifyError, "padding overruns"):
            list(vm._tlvs(struct.pack("<II", 9, 1) + b"x"))

    def test_rejects_nonzero_padding(self):
        with self.assertRaisesRegex(vm.VerifyError, "non-zero alignment padding"):
            list(vm._tlvs(tlv(9, b"x", b"\x01\0\0")))

    def test_accepts_aligned_record_without_padding(self):
        records = list(vm._tlvs(tlv(9, b"1234")))
        self.assertEqual(records[0][:2], (9, b"1234"))


class FirmwareParserTests(unittest.TestCase):
    def test_firmware_version_capability_and_selected_command_group(self):
        header = struct.pack("<II", 0, vm.MAGIC) + b"test firmware\0".ljust(64, b"\0")
        header += struct.pack("<IIQ", 0x12345678, 42, 0)
        tlvs = tlv(vm.TLV_CAPABILITIES, struct.pack("<II", 1, 1))
        tlvs += tlv(vm.TLV_CMD_VERSIONS, bytes((5, 2, 7, 8, 9, 99, 1, 1)))
        result = vm.parse_ucode(header + tlvs)
        self.assertTrue(result["capability_bit_32"])
        self.assertEqual(result["version"], 0x12345678)
        self.assertEqual(result["commands"], {(2, 5): (7, 8)})

    def test_rejects_invalid_ucode_magic(self):
        with self.assertRaisesRegex(vm.VerifyError, "bad firmware header"):
            vm.parse_ucode(bytes(96))

    def test_capabilities_across_words(self):
        header = struct.pack("<II", 0, vm.MAGIC) + bytes(64) + struct.pack("<IIQ", 1, 2, 0)
        records = tlv(vm.TLV_CAPABILITIES, struct.pack("<II", 1, (1 << 12) | (1 << 22)))
        records += tlv(vm.TLV_CAPABILITIES, struct.pack("<II", 3, (1 << 14) | (1 << 18)))
        self.assertEqual(vm.parse_ucode(header + records)["capabilities"], [44, 54, 110, 114])

    def test_rejects_capability_word_out_of_range(self):
        header = struct.pack("<II", 0, vm.MAGIC) + bytes(64) + struct.pack("<IIQ", 1, 2, 0)
        with self.assertRaisesRegex(vm.VerifyError, "out of range"):
            vm.parse_ucode(header + tlv(vm.TLV_CAPABILITIES, struct.pack("<II", 4, 1)))

    def test_rejects_partial_command_version(self):
        header = struct.pack("<II", 0, vm.MAGIC) + bytes(64) + struct.pack("<IIQ", 1, 2, 0)
        with self.assertRaisesRegex(vm.VerifyError, "partial entry"):
            vm.parse_ucode(header + tlv(vm.TLV_CMD_VERSIONS, b"abc"))

    def test_parses_multiple_skus_hardware_and_chunks(self):
        pnvm = b"".join((
            tlv(vm.TLV_SKU, struct.pack("<3I", 1, 2, 3)),
            tlv(vm.TLV_HW_TYPE, struct.pack("<HHI", 0x42, 0x10d, 0)),
            tlv(vm.TLV_VERSION, struct.pack("<I", 0x1234)),
            tlv(vm.TLV_SEC_RT, struct.pack("<I", 0x1000) + b"abcde"),
            tlv(vm.TLV_SKU, struct.pack("<3I", 4, 5, 6)),
            tlv(vm.TLV_HW_TYPE, struct.pack("<HHI", 0x43, 0x10e, 0)),
            tlv(vm.TLV_SEC_RT, struct.pack("<I", 0x2000) + b"xy"),
        ))
        skus = vm.parse_pnvm(pnvm)
        self.assertEqual(len(skus), 2)
        self.assertEqual(skus[0]["hardware"], [(0x42, 0x10d)])
        self.assertEqual(skus[0]["chunks"], [{"offset": 0x1000, "length": 5}])
        self.assertEqual(skus[1]["sku"], (4, 5, 6))

    def test_rejects_short_sku_and_section_without_offset(self):
        with self.assertRaisesRegex(vm.VerifyError, "must be 12 bytes"):
            vm.parse_pnvm(tlv(vm.TLV_SKU, b"short"))
        with self.assertRaisesRegex(vm.VerifyError, "lacks its 4-byte offset"):
            vm.parse_pnvm(tlv(vm.TLV_SKU, bytes(12)) + tlv(vm.TLV_SEC_RT, b"abc"))


if __name__ == "__main__":
    unittest.main()
