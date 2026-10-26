"""Compile the production ID macro without importing kernel-only structures."""
import re
import subprocess
import unittest
from pathlib import Path


class FirmwareIdTests(unittest.TestCase):
    def test_receive_group_survives_failed_flag_mask(self):
        header = (Path(__file__).resolve().parents[1] /
                  "itlwm/hal_iwx/if_iwxreg.h").read_text()
        names = ("IWX_WIDE_ID", "IWX_CMD_FAILED_MSK")
        definitions = [re.search(r"^#define " + name + r"\b.*$", header,
                                 re.MULTILINE).group(0) for name in names]
        source = "\n".join(definitions) + "\n"
        # Use the exact masked group expression from the receive dispatcher.
        # PNVM 0x0cfe used to become 0x00fe because << bound before &.
        for group in (0, 1, 2, 3, 4, 5, 12, 16):
            for failed in (0, 0x40):
                for opcode in (0, 1, 0x18, 0xfe, 0xff):
                    source += (
                        "static_assert(IWX_WIDE_ID("
                        f"{group | failed} & ~IWX_CMD_FAILED_MSK, {opcode})"
                        f" == {group * 256 + opcode}, \"RX group lost\");\n")
        source += ('static_assert(IWX_WIDE_ID(1 + 2, true ? 2 : 3) == 0x302,'
                   ' "macro arguments must be parenthesized");\n')
        result = subprocess.run(
            ["clang++", "-std=c++11", "-x", "c++", "-fsyntax-only", "-"],
            input=source, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
