#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Retain each sample with exact source/toolchain provenance; no speed assertions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parent.parent

def run(*command, cwd=ROOT):
    return subprocess.run(command, cwd=cwd, check=True, capture_output=True, text=True, timeout=60).stdout.strip()

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--goose', required=True)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--rounds', type=int, default=5)
parser.add_argument('--output', type=Path, default=ROOT / '.build/benchmark.json')
args = parser.parse_args()
assert 1 <= args.rounds <= 30
args.goose = str(Path(args.goose).resolve())
(ROOT / '.build').mkdir(exist_ok=True)
paths = ['native/set.c', 'native/common.h', 'native/watch.c', 'watch.goose', 'watch.build', 'GOOSE_REVISION', 'bench/driver.goose',
         'bench/decoder.c', 'bench/decode_entry.c', 'bench/measure.py', 'scripts/native-manifest.py']

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def snapshot():
    return dict(commit=run('git','rev-parse','HEAD'), status=run('git','status','--porcelain'),
                sources={p:sha256(ROOT/p) for p in paths},
                goose_binary_sha256=sha256(args.goose), compiler=run(args.cc,'--version'),
                affinity=sorted(os.sched_getaffinity(0)),
                queue_limit=Path('/proc/sys/fs/inotify/max_queued_events').read_text().strip())

run('python3', 'scripts/native-manifest.py', '--check')
before = snapshot()
run(args.goose, 'build', 'bench/driver.goose', '--cc', args.cc, '-O2', '-o', '.build/bench')
run(args.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', 'bench/decoder.c', 'bench/decode_entry.c', '-o', '.build/bench-decoder')
assert int(Path('/proc/sys/fs/inotify/max_queued_events').read_text()) >= 4096, 'benchmark requires max_queued_events >= 4096'
binary_hashes = {p:sha256(ROOT/p) for p in ('.build/bench', '.build/bench-decoder')}
records = []
for round_index in range(args.rounds):
    native_ns, checksum = map(int, run('.build/bench-decoder').split())
    sample = b'sample.goose'
    expected = 1000000 * 999999 // 2 + 1000000 * 12 + sum(sample) * (1000000 // 12) + sum(sample[:1000000 % 12])
    assert checksum == expected, 'decoder benchmark produced an incorrect checksum'
    for count in (1, 64, 1024):
        with tempfile.TemporaryDirectory(prefix='goose-watch-bench-') as directory:
            idle_ns, drain_ns, events, name_bytes = map(int, run(str(ROOT / '.build/bench'), directory, str(count)).split())
        records.append(dict(round=round_index, files=count, idle_ns=idle_ns, idle_calls=10000,
                            drain_ns=drain_ns, events=events, name_bytes=name_bytes,
                            decoder_ns=native_ns, decoder_iterations=1000000, decoder_checksum=checksum))
after = snapshot()
assert before == after, 'source, compiler or Git state changed during measurement; discard this run'
assert binary_hashes == {p:sha256(ROOT/p) for p in binary_hashes}, 'benchmark binaries changed during measurement'
evidence = dict(source_commit=before['commit'], dirty=bool(before['status']),
                sources=before['sources'], binary_sha256=binary_hashes, stable_inputs=True,
                goose_revision=(ROOT/'GOOSE_REVISION').read_text().strip(),
                goose_binary_sha256=before['goose_binary_sha256'],
                compiler=before['compiler'], platform=platform.platform(), machine=platform.machine(),
                affinity=before['affinity'], queue_limit=before['queue_limit'],
                flags='-O2', samples=records,
                limits='Local samples; no competitor, latency percentile, sustained load or cross-platform speed claim. Drain excludes file creation; idle includes poll; decoder excludes Goose name copying.')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(evidence, indent=2)+'\n')
print(args.output)
