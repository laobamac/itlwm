#!/usr/bin/env python3
"""Validate Meteor Lake firmware TLVs and optionally compare embedded Mach-O data."""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
import zlib
from pathlib import Path
from typing import Optional

UCODE = "iwlwifi-ma-b0-gf-a0-83.ucode"
PNVM = "iwlwifi-ma-b0-gf-a0.pnvm"
EXPECTED = {
    UCODE: "9386a8294457f527c6e0fa1eac440c50cbee801707fdb480a10889770c0d297b",
    PNVM: "b21d3257a2a434ca4660353a7144c436eafaf8e60d0542e3159eaffee4de5629",
}
MAGIC = 0x0A4C5749
UCODE_HEADER_SIZE = 88
TLV_SKU, TLV_HW_TYPE, TLV_VERSION, TLV_SEC_RT = 64, 58, 62, 19
TLV_CAPABILITIES, TLV_CMD_VERSIONS, TLV_FW_VERSION = 30, 48, 36
GROUPS = {0: "LEGACY", 1: "LONG", 2: "SYSTEM", 3: "MAC_CONF", 4: "PHY_OPS",
          5: "DATA_PATH", 7: "STA_HE_CTXT", 11: "PROT_OFFLOAD", 12: "REGULATORY_NVM"}


class VerifyError(ValueError):
    pass


def _tlvs(data: bytes, offset: int = 0):
    """Yield (type, payload, start) from little-endian, 4-byte-padded TLVs."""
    while offset < len(data):
        if len(data) - offset < 8:
            raise VerifyError(f"truncated TLV header at 0x{offset:x}")
        typ, size = struct.unpack_from("<II", data, offset)
        start, end = offset + 8, offset + 8 + size
        padded_end = end + ((-size) & 3)
        if end > len(data):
            raise VerifyError(f"TLV {typ} payload overruns input at 0x{offset:x}")
        if padded_end > len(data):
            raise VerifyError(f"TLV {typ} padding overruns input at 0x{offset:x}")
        if any(data[end:padded_end]):
            raise VerifyError(f"TLV {typ} has non-zero alignment padding at 0x{end:x}")
        yield typ, data[start:end], offset
        offset = padded_end


def parse_ucode(data: bytes):
    if len(data) < UCODE_HEADER_SIZE:
        raise VerifyError("firmware header is truncated (need 88 bytes)")
    zero, magic = struct.unpack_from("<II", data)
    if zero != 0 or magic != MAGIC:
        raise VerifyError(f"bad firmware header: zero=0x{zero:x}, magic=0x{magic:x}")
    human = data[8:72].split(b"\0", 1)[0].decode("ascii", "replace")
    version, build = struct.unpack_from("<II", data, 72)
    tlvs = list(_tlvs(data, UCODE_HEADER_SIZE))
    cap_bits = set()
    commands = {}
    fw_version = None
    for typ, payload, at in tlvs:
        if typ == TLV_CAPABILITIES:
            if len(payload) != 8:
                raise VerifyError(f"enabled-capabilities TLV at 0x{at:x} must be 8 bytes")
            index, flags = struct.unpack("<II", payload)
            if index >= 4:
                raise VerifyError(f"capability word index {index} is out of range")
            cap_bits.update(index * 32 + bit for bit in range(32) if flags & (1 << bit))
        elif typ == TLV_CMD_VERSIONS:
            if len(payload) % 4:
                raise VerifyError(f"CMD_VERSIONS TLV at 0x{at:x} has partial entry")
            for cmd, group, cmd_ver, notif_ver in struct.iter_unpack("<4B", payload):
                if group in GROUPS:
                    key = (group, cmd)
                    if key in commands:
                        raise VerifyError(f"duplicate CMD_VERSIONS entry group={group} cmd={cmd}")
                    commands[key] = (cmd_ver, notif_ver)
        elif typ == TLV_FW_VERSION:
            fw_version = payload.hex()
    return {"human": human, "version": version, "build": build, "fw_version": fw_version,
            "capability_bit_32": 32 in cap_bits, "capabilities": sorted(cap_bits),
            "commands": commands, "tlv_count": len(tlvs)}


def parse_pnvm(data: bytes):
    top = list(_tlvs(data))
    skus = []
    current = None
    for i, (typ, payload, at) in enumerate(top):
        if typ == TLV_SKU:
            if len(payload) != 12:
                raise VerifyError(f"PNVM_SKU TLV at 0x{at:x} must be 12 bytes")
            if current:
                skus.append(current)
            current = {"sku": struct.unpack("<3I", payload), "hardware": [], "version": [], "chunks": []}
            continue
        if current is None:
            raise VerifyError(f"PNVM data before first SKU at 0x{at:x} (type {typ})")
        if typ == TLV_HW_TYPE:
            if len(payload) < 4:
                raise VerifyError(f"HW_TYPE TLV at 0x{at:x} is shorter than 4 bytes")
            current["hardware"].append(struct.unpack_from("<HH", payload))
        elif typ == TLV_VERSION:
            if len(payload) < 4:
                raise VerifyError(f"PNVM_VERSION TLV at 0x{at:x} is shorter than 4 bytes")
            current["version"].append(struct.unpack_from("<I", payload)[0])
        elif typ == TLV_SEC_RT:
            if len(payload) < 4:
                raise VerifyError(f"SEC_RT TLV at 0x{at:x} lacks its 4-byte offset")
            section_offset = struct.unpack_from("<I", payload)[0]
            # Firmware uses 0xddddeeee as a deprecated separator, not image data.
            if section_offset != 0xDDDDEEEE:
                current["chunks"].append({"offset": section_offset, "length": len(payload) - 4})
    if current:
        skus.append(current)
    if not skus:
        raise VerifyError("PNVM contains no SKU records")
    return skus


def _macho_embedded(path: Path):
    """Read fwList/fwNumber from a thin x86_64 Mach-O without scanning strings."""
    data = path.read_bytes()
    if len(data) < 32 or data[:4] != b"\xcf\xfa\xed\xfe":
        raise VerifyError("binary must be a little-endian 64-bit Mach-O (thin x86_64)")
    cpu, _, _, ncmds, sizeofcmds, _, _ = struct.unpack_from("<iiIIIII", data, 4)
    if cpu != 0x01000007:
        raise VerifyError(f"Mach-O is not x86_64 (cputype 0x{cpu & 0xffffffff:x})")
    if 32 + sizeofcmds > len(data):
        raise VerifyError("Mach-O load commands exceed file bounds")
    segments, symtab = [], None
    off = 32
    for _ in range(ncmds):
        if off + 8 > 32 + sizeofcmds:
            raise VerifyError("truncated Mach-O load command")
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmdsize < 8 or off + cmdsize > 32 + sizeofcmds:
            raise VerifyError("invalid Mach-O load command size")
        if cmd == 0x19:  # LC_SEGMENT_64
            if cmdsize < 72:
                raise VerifyError("short LC_SEGMENT_64")
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, off + 24)
            segments.append((vmaddr, vmsize, fileoff, filesize))
        elif cmd == 0x2:  # LC_SYMTAB
            if cmdsize < 24:
                raise VerifyError("short LC_SYMTAB")
            symtab = struct.unpack_from("<IIII", data, off + 8)
        off += cmdsize
    if symtab is None:
        raise VerifyError("Mach-O has no LC_SYMTAB; fwList/fwNumber cannot be verified")
    symoff, nsyms, stroff, strsize = symtab
    if symoff + nsyms * 16 > len(data) or stroff + strsize > len(data):
        raise VerifyError("Mach-O symbol table exceeds file bounds")
    symbols = {}
    for i in range(nsyms):
        ent = symoff + i * 16
        strx, n_type, _, _, value = struct.unpack_from("<I BB H Q", data, ent)
        if not n_type & 0x0e or strx >= strsize:
            continue
        end = data.find(b"\0", stroff + strx, stroff + strsize)
        if end < 0:
            raise VerifyError("unterminated Mach-O symbol name")
        name = data[stroff + strx:end].decode("utf-8", "replace").lstrip("_")
        if name in ("fwList", "fwNumber"):
            symbols[name] = value

    def read_va(va, size):
        for vmaddr, vmsize, fileoff, filesize in segments:
            delta = va - vmaddr
            if 0 <= delta and delta + size <= min(vmsize, filesize):
                pos = fileoff + delta
                if pos + size <= len(data):
                    return data[pos:pos + size]
        raise VerifyError(f"Mach-O address 0x{va:x} ({size} bytes) is not file-backed")

    if "fwList" not in symbols or "fwNumber" not in symbols:
        raise VerifyError("Mach-O does not expose both fwList and fwNumber symbols")
    count = struct.unpack("<i", read_va(symbols["fwNumber"], 4))[0]
    if count < 0 or count > 4096:
        raise VerifyError(f"implausible fwNumber {count}")
    entries = {}
    for idx in range(count):
        name_ptr, blob_ptr, size = struct.unpack("<QQi", read_va(symbols["fwList"] + idx * 24, 20))
        if size < 0 or size > len(data):
            raise VerifyError(f"fwList[{idx}] has invalid compressed size {size}")
        name_bytes = bytearray()
        for pos in range(0, 4096, 128):
            chunk = read_va(name_ptr + pos, 128)
            end = chunk.find(b"\0")
            name_bytes.extend(chunk if end < 0 else chunk[:end])
            if end >= 0:
                break
        else:
            raise VerifyError(f"fwList[{idx}] name is not NUL terminated")
        name = name_bytes.decode("utf-8", "strict")
        if name in entries:
            raise VerifyError(f"duplicate embedded firmware name {name}")
        compressed = read_va(blob_ptr, size)
        dec = zlib.decompressobj()
        try:
            raw = dec.decompress(compressed) + dec.flush()
        except zlib.error as exc:
            raise VerifyError(f"embedded {name} is not valid zlib data: {exc}") from exc
        if not dec.eof or dec.unused_data or dec.unconsumed_tail:
            raise VerifyError(f"embedded {name} has incomplete or trailing compressed data")
        entries[name] = raw
    return count, entries


def verify(directory: Path, binary: Optional[Path] = None):
    raw = {}
    for name, expected in EXPECTED.items():
        path = directory / name
        if not path.is_file():
            raise VerifyError(f"missing source firmware: {path}")
        payload = path.read_bytes()
        actual = hashlib.sha256(payload).hexdigest()
        if actual != expected:
            raise VerifyError(f"{name} SHA256 mismatch: {actual} (expected {expected})")
        raw[name] = payload
    fw = parse_ucode(raw[UCODE])
    pnvm = parse_pnvm(raw[PNVM])
    embedded_result = None
    if binary:
        count, embedded = _macho_embedded(binary)
        for name, payload in raw.items():
            if name not in embedded:
                raise VerifyError(f"{binary}: embedded fwList is missing {name}")
            if embedded[name] != payload:
                raise VerifyError(f"embedded {name} differs byte-for-byte from source firmware")
        embedded_result = {"count": count, "files": [UCODE, PNVM]}
    return fw, pnvm, embedded_result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware_dir", type=Path, help="directory containing the two source firmware files")
    parser.add_argument("--binary", type=Path, help="optional x86_64 Mach-O Kext executable to inspect")
    args = parser.parse_args(argv)
    try:
        fw, skus, embedded = verify(args.firmware_dir, args.binary)
    except (OSError, VerifyError, struct.error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"firmware: {fw['human']} (version=0x{fw['version']:08x}, build={fw['build']}, tlvs={fw['tlv_count']})")
    print(f"FW_VERSION TLV: {fw['fw_version'] or 'absent'}")
    print(f"enabled capability bit 32 (PNVM): {'yes' if fw['capability_bit_32'] else 'no'}")
    print("MLD port capabilities: " + ", ".join(
        f"{bit}={'yes' if bit in fw['capabilities'] else 'no'}"
        for bit in (44, 54, 110, 114)))
    print("CMD_VERSIONS (selected command groups):")
    for group in sorted({g for g, _ in fw["commands"]}):
        cmds = ", ".join(f"{cmd:02x}:{v[0]}/{v[1]}" for (g, cmd), v in sorted(fw["commands"].items()) if g == group)
        print(f"  {group:02x} {GROUPS[group]}: {cmds}")
    print(f"PNVM SKU records: {len(skus)}")
    for index, sku in enumerate(skus):
        hw = ", ".join(f"MAC=0x{mac:04x}/RF=0x{rf:04x}" for mac, rf in sku["hardware"]) or "none"
        chunks = ", ".join(f"@0x{c['offset']:08x}:{c['length']}B" for c in sku["chunks"]) or "none"
        vers = ", ".join(f"0x{v:08x}" for v in sku["version"]) or "none"
        print(f"  [{index}] SKU=" + "/".join(f"0x{x:08x}" for x in sku["sku"]) +
              f" HW=[{hw}] version=[{vers}] chunks=[{chunks}]")
    if embedded:
        print(f"Mach-O fwList/fwNumber: {embedded['count']} entries; {UCODE} and {PNVM} match source byte-for-byte")
    print("SHA256: both expected firmware hashes match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
