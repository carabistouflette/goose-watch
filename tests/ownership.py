#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Ensure the compiler rejects copying or aggregating a resource owner."""
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parent.parent
(ROOT / '.build').mkdir(exist_ok=True)
for name, body, message in [
    ('copy', 'var a = watch::Watcher { .. }; var b: watch::Watcher = a;', 'root owners cannot be copied'),
    ('assignment', 'var a = watch::Watcher { .. }; var b = watch::Watcher { .. }; b = a;', 'root'),
    ('array', 'var owners: watch::Watcher[2];', 'root'),
]:
    path = ROOT / '.build' / f'reject-{name}.goose'
    path.write_text('import watch from "..";\nfn main() { ' + body + ' }\n')
    result = subprocess.run([sys.argv[1], 'check', str(path)], capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and message in result.stderr, result.stderr


for name, source in [
    ('embedded', 'struct Box { owner: watch::Watcher } fn main() { var box: Box; }'),
    ('event-view', 'fn main() { var owner = watch::Watcher { .. }; var event = watch::Event { .. }; let view = event.name[..]; let status = watch::next(owner,0,event); print(view,status.ready); }'),
    ('by-value', 'fn take(owner: watch::Watcher) {} fn main() { var owner = watch::Watcher { .. }; take(owner); }'),
]:
    path = ROOT / '.build' / f'reject-{name}.goose'
    path.write_text('import watch from ".."; import std;\n' + source + '\n')
    result = subprocess.run([sys.argv[1], 'check', str(path)], capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and ('shrinks' if name == 'event-view' else 'root') in result.stderr, result.stderr
print('six resource ownership rejections passed')
