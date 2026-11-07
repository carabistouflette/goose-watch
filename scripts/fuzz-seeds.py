#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import struct
from pathlib import Path
out = Path('.build/fuzz-corpus'); out.mkdir(parents=True, exist_ok=True)
for name, wd, mask, length, tail in [
    ('create', 7, 256, 16, b'file.goose\0' + b'\0' * 5),
    ('overflow', -1, 16384, 0, b''),
    ('ignored', 7, 32768, 0, b''),
    ('long', 7, 128, 256, b'x' * 255 + b'\0'),
    ('bad-length', 7, 256, 0xffffffff, b''),
]:
    (out / name).write_bytes(struct.pack('=iIII', wd, mask, 0xffffffff, length) + tail)
