#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""A real public Goose consumer inherits descriptors across pathname replacement."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

command = sys.argv[1:]
assert command
with tempfile.TemporaryDirectory(prefix='watch-fds-') as raw:
    root = Path(raw)
    current = root / 'current'
    moved = root / 'moved'
    current.mkdir()
    descriptors = []
    try:
        old = os.open(current, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
        descriptors.append(old)
        current.rename(moved)
        current.mkdir()
        new = os.open(current, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
        descriptors.append(new)
        alias = os.dup(old)
        descriptors.append(alias)
        regular = os.open('/dev/null', os.O_RDONLY | os.O_CLOEXEC)
        descriptors.append(regular)
        # The original spelling now refers to another inode; fd0 remains moved.
        result = subprocess.run([*command, *map(str, descriptors)], pass_fds=descriptors,
                                capture_output=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        assert b'borrowed fd identity' in result.stdout, result.stdout
        assert (moved / 'old.goose').read_bytes() == b'still borrowed'
        assert (current / 'new.goose').read_bytes() == b'still borrowed'
        assert not (current / 'old.goose').exists()
        assert not (moved / 'new.goose').exists()
        print(result.stdout.decode().strip())
    finally:
        for descriptor in descriptors:
            os.close(descriptor)
