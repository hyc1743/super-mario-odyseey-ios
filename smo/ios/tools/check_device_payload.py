"""Verify actual embedded iOS executable sections before packaging or signing."""
import argparse
import hashlib
from pathlib import Path
import struct


def verify_image(executable, manifest):
    data = executable.read_bytes()
    if len(data) < 32:
        raise ValueError("truncated Mach-O")
    header = struct.unpack_from("<8I", data)
    if header[0] != 0xfeedfacf or header[1] != 0x0100000c or header[3] != 2:
        raise ValueError("device executable must be ARM64 MH_EXECUTE")
    position, sections = 32, {}
    for _ in range(header[4]):
        command, size = struct.unpack_from("<II", data, position)
        if size < 8 or position + size > min(len(data), 32 + header[5]):
            raise ValueError("invalid Mach-O load command")
        if command == 0x19:
            if size < 72:
                raise ValueError("invalid segment command")
            maximum, protection, count = struct.unpack_from("<iiI", data, position + 56)
            if 72 + count * 80 > size:
                raise ValueError("invalid segment section count")
            for i in range(count):
                start = position + 72 + i * 80
                name, segment, address, length, offset = struct.unpack_from("<16s16sQQI", data, start)
                identity = (segment.rstrip(b"\0").decode(), name.rstrip(b"\0").decode())
                if identity in sections:
                    raise ValueError("invalid Mach-O section range")
                sections[identity] = (address, offset, length, maximum, protection)
        position += size
    if position != 32 + header[5]:
        raise ValueError("Mach-O command table size mismatch")
    marker = sections.get(("__DATA_CONST", "__smo_boot"))
    if not marker or data[marker[1]:marker[1] + marker[2]] != b"SMOEMB\1\0":
        raise ValueError("actual iOS loader was compiled without SMO_EMBED")
    expected = []
    for line in manifest.read_text(encoding="utf8").splitlines():
        if line and not line.startswith("#"):
            module, identity, relative, length, digest = line.split("\t")
            expected.append((module, identity, int(length, 16), digest))
    if len(expected) != 9 or {r[0] for r in expected} != {"sdk", "subsdk0", "main"}:
        raise ValueError("required three embedded modules are absent")
    for module, identity, length, digest in expected:
        section = sections.get(tuple(identity.split(",")))
        if not section or section[0] % 0x4000 or section[2] != length or section[1] + length > len(data):
            raise ValueError("embedded module section layout mismatch")
        protection = 3 if identity.startswith("__DM") else 5
        if section[3] != protection or section[4] != protection:
            raise ValueError("embedded module segment protection mismatch")
        if not hashlib.sha256(memoryview(data)[section[1]:section[1] + length]).hexdigest().startswith(digest):
            raise ValueError("embedded module bytes differ from verified build input")
    print("IOS_DEVICE_PAYLOAD_PASS modules=3 sections=9 loader=embedded protections=RX/RW")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    verify_image(args.executable, args.manifest)
