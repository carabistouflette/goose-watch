#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Handshake-driven filesystem tests; no sleeps or assumed notification counts."""
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
COMMAND = sys.argv[1:]
assert COMMAND

class Driver:
    def __init__(self, path):
        self.proc = subprocess.Popen([*COMMAND, os.fsdecode(path)], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
        self.pending = b''
        try:
            assert self.line() == 'READY'
        except BaseException:
            self.proc.kill(); self.proc.wait(timeout=10)
            raise

    def line(self):
        deadline = time.monotonic() + 10
        while b'\n' not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0 and select.select([self.proc.stdout], [], [], remaining)[0], 'driver timeout'
            chunk = os.read(self.proc.stdout.fileno(), 65536)
            assert chunk, f'driver closed stdout (exit status {self.proc.poll()})'
            self.pending += chunk
        line, self.pending = self.pending.split(b'\n', 1)
        return line.decode('ascii')

    def call(self, command='next'):
        self.proc.stdin.write(command.encode() + b'\n')
        return self.line()

    def drain(self):
        result = []
        for _ in range(20000):
            line = self.call()
            if line in ('IDLE', 'END'): return result, line
            mask, cookie, name = line.split(' ', 2)
            result.append((int(mask), int(cookie), bytes.fromhex(name)))
        raise AssertionError('unbounded event stream')

    def close(self):
        try:
            assert self.call('close') == 'CLOSED'
            _, errors = self.proc.communicate(timeout=10)
            if 'run' in COMMAND:
                errors = errors.replace(b"building the package watch's C\n", b'')
            assert self.proc.returncode == 0 and not errors, errors
        finally:
            if self.proc.poll() is None:
                self.proc.kill(); self.proc.wait(timeout=10)

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    watched = root / 'watched'; watched.mkdir()
    driver = Driver(watched)
    try:
        start = time.monotonic(); assert driver.call('wait') == 'IDLE'
        assert time.monotonic() - start >= .015
        path = watched / 'one.goose'; path.write_bytes(b'hello')
        events, end = driver.drain()
        assert end == 'IDLE'
        for mask in (256, 2, 8):
            assert any(m & mask and n == b'one.goose' for m, _, n in events), events
        path.rename(watched / 'two.goose')
        events, _ = driver.drain()
        before = [(c, n) for m, c, n in events if m & 64]
        after = [(c, n) for m, c, n in events if m & 128]
        assert before and after and before[0][0] == after[0][0] != 0
        assert before[0][1] == b'one.goose' and after[0][1] == b'two.goose'
        (watched / 'two.goose').unlink()
        events, _ = driver.drain()
        assert any(m & 512 and n == b'two.goose' for m, _, n in events)
        for name in (b'bad-\xff\n\x1b', b'x' * 255):
            absolute = os.fsencode(watched) + b'/' + name
            fd = os.open(absolute, os.O_CREAT | os.O_WRONLY, 0o600); os.close(fd)
            events, _ = driver.drain()
            assert any(m & 256 and n == name for m, _, n in events)
            os.unlink(absolute); driver.drain()
        queue_limit = int(Path('/proc/sys/fs/inotify/max_queued_events').read_text())
        count = 2200 if queue_limit >= 8192 else 32
        if count == 32: print('large real burst skipped: max_queued_events < 8192; synthetic full-buffer tests remain enabled')
        for i in range(count): (watched / f'f{i:04}').touch()
        events, _ = driver.drain()
        created = [n for m, _, n in events if m & 256]
        assert created == [f'f{i:04}'.encode() for i in range(count)]
        for file in watched.iterdir(): file.unlink()
        driver.drain()
        sub = watched / 'sub'; sub.mkdir()
        events, _ = driver.drain()
        assert any(m & 256 and m & 1073741824 and n == b'sub' for m, _, n in events)
        (sub / 'nested').touch()
        assert driver.drain()[0] == [], 'watch unexpectedly recursive'
        (sub / 'nested').unlink(); sub.rmdir(); driver.drain()
        moved = root / 'moved'; watched.rename(moved)
        events, _ = driver.drain()
        assert any(m & 2048 and not n for m, _, n in events)
        moved.rmdir()
        events, end = driver.drain()
        assert any(m & 1024 for m, _, _ in events)
        assert any(m & 32768 for m, _, _ in events) and end == 'END'
        assert driver.call() == 'END'
    finally: driver.close()
    watched.mkdir()
    driver = Driver(watched)
    try:
        assert driver.call('reopen') == 'REOPENED'
        assert driver.call() == 'IDLE'
        assert driver.call('poison') == 'POISONED'
        assert driver.call('reopen') == 'REOPENED'
        assert driver.call() == 'IDLE'
    finally: driver.close()
    link = root / 'link'; link.symlink_to(watched, target_is_directory=True)
    for path in (link, root / 'absent'):
        result = subprocess.run([*COMMAND, str(path)], capture_output=True, timeout=15)
        assert result.returncode == 0 and result.stdout.startswith(b'OPEN_ERROR '), result
print('filesystem, byte-name, buffering, lifecycle and timeout tests passed')
