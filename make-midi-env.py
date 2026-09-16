#!/usr/bin/env python3

import zlib
from pathlib import Path

src = Path("uboot-env.bin")
dst = Path("uboot-env-midi.bin")

blob = bytearray(src.read_bytes())

if len(blob) != 4096:
    raise RuntimeError(f"Expected 4096 bytes, got {len(blob)}")

old_crc = int.from_bytes(blob[:4], "little")
data = bytes(blob[4:])

if zlib.crc32(data) & 0xffffffff != old_crc:
    raise RuntimeError("Original CRC does not match")

old = b"bootargs=console=ttyS0,115200 "
new = b"bootargs="

if data.count(old) != 1:
    raise RuntimeError(
        f"Expected exactly one bootargs console prefix, found {data.count(old)}"
    )

# Remove only "console=ttyS0,115200 " from bootargs.
modified = data.replace(old, new, 1)

# Environment must remain exactly 4092 bytes.
# Removing bytes shifts the following variables toward the beginning,
# so restore the removed space as erased/padding bytes at the end.
modified += b"\xff" * (len(data) - len(modified))

assert len(modified) == 4092

new_crc = zlib.crc32(modified) & 0xffffffff

dst.write_bytes(
    new_crc.to_bytes(4, "little") + modified
)

print(f"old CRC: 0x{old_crc:08x}")
print(f"new CRC: 0x{new_crc:08x}")
print(f"wrote:   {dst}")
