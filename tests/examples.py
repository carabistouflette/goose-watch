#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check direct argv, source filtering, finite reruns and command failure exit."""
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
RERUN = str(Path(sys.argv[1]).resolve())

def line(proc):
    # The example emits short single-line statuses; read one byte at a time so
    # select never misses data buffered in a Python stream.
    data = b''; deadline = time.monotonic() + 10
    while not data.endswith(b'\n'):
        remaining = deadline - time.monotonic()
        assert remaining > 0 and select.select([proc.stdout], [], [], remaining)[0], 'example timed out'
        chunk = os.read(proc.stdout.fileno(), 1)
        assert chunk, 'example closed stdout'
        data += chunk
        assert len(data) < 4096
    return data

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    watched = root / 'src'; watched.mkdir()
    marker = root / 'runs'
    weird = 'literal;$(touch SHOULD_NOT_EXIST)'
    script = (
        'import pathlib,sys; '
        f'assert sys.argv[2] == {weird!r}; '
        'p=pathlib.Path(sys.argv[1]); '
        'p.write_bytes((p.read_bytes() if p.exists() else b"") + b"x")'
    )
    command = [RERUN, str(watched), '2', '--', sys.executable, '-c', script, str(marker), weird]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    try:
        assert b'exit=0' in line(proc) and marker.read_bytes() == b'x'
        (watched / 'generated.o').write_bytes(b'ignored')
        assert not select.select([proc.stdout], [], [], .05)[0]
        (watched / 'change.goose').write_bytes(b'source')
        output, error = proc.communicate(timeout=10)
        assert proc.returncode == 0 and not error and b'exit=0' in output
        assert marker.read_bytes() == b'xx' and not (watched / 'SHOULD_NOT_EXIST').exists()
    finally:
        if proc.poll() is None: proc.kill(); proc.wait(timeout=10)
    result = subprocess.run([RERUN,str(watched),'1','--','/bin/false'],capture_output=True,timeout=10)
    assert result.returncode == 1 and b'exit=1' in result.stdout
    for args in ([], [str(watched), '0', '--', '/bin/true'], [str(watched),'1','--','true']):
        assert subprocess.run([RERUN,*args],capture_output=True,timeout=10).returncode == 2
print('command rerun, literal argv, source filtering and failure status tests passed')
