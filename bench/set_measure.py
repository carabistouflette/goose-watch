#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Measure public set open/idle/burst ownership at fixed directory counts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--goose', required=True)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--rounds', type=int, default=5)
parser.add_argument('--output', type=Path, default=ROOT / '.build/set-benchmark.json')
args = parser.parse_args()
assert 1 <= args.rounds <= 30
args.goose = str(Path(args.goose).resolve())
(ROOT / '.build').mkdir(exist_ok=True)


def run(*command):
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                          check=True, timeout=90).stdout.strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


inputs = [ROOT / p for p in (
    'watch.goose', 'watch.build', 'GOOSE_REVISION', 'scripts/native-manifest.py',
    'bench/set_driver.goose', 'bench/set_measure.py',
)] + sorted((ROOT / 'native').glob('*'))


def snapshot():
    return dict(
        commit=run('git', 'rev-parse', 'HEAD'),
        status=run('git', 'status', '--porcelain'),
        sources={str(p.relative_to(ROOT)): sha(p) for p in inputs},
        goose=sha(args.goose), cc=run(args.cc, '--version'),
        queue_limit=int(Path('/proc/sys/fs/inotify/max_queued_events').read_text()),
        affinity=sorted(os.sched_getaffinity(0)),
    )


run('python3', 'scripts/native-manifest.py', '--check')
before = snapshot()
assert before['queue_limit'] >= 16384, 'set benchmark requires max_queued_events >= 16384'
run(args.goose, 'build', 'bench/set_driver.goose', '--cc', args.cc, '-O2', '-o', '.build/set-bench')
binary = sha(ROOT / '.build/set-bench')
samples = []
for iteration in range(args.rounds):
    for count in (1, 16, 64, 256):
        with tempfile.TemporaryDirectory(prefix='watch-set-bench-') as temp:
            root = Path(temp)
            for i in range(count):
                (root / f'd{i}').mkdir()
            opened, idle, drain, events, names = map(int, run(
                str(ROOT / '.build/set-bench'), temp, str(count)).split())
        samples.append(dict(
            round=iteration, directories=count, files_per_directory=16,
            open_ns=opened, idle_ns=idle, idle_calls=1000, drain_ns=drain,
            events=events, name_bytes=names,
        ))
assert before == snapshot(), 'source/compiler/resource inputs changed; discard this run'
assert binary == sha(ROOT / '.build/set-bench'), 'benchmark binary changed; discard this run'
report = dict(
    source_commit=before['commit'], dirty=bool(before['status']), sources=before['sources'],
    goose_revision=(ROOT / 'GOOSE_REVISION').read_text().strip(),
    goose_binary_sha256=before['goose'], compiler=before['cc'], binary_sha256=binary,
    platform=platform.platform(), affinity=before['affinity'], queue_limit=before['queue_limit'],
    flags='-O2', stable_inputs=True, samples=samples,
    limits='Shared-host local samples; no comparative ranking or latency-percentile guarantee. '
           'Drain excludes generation and includes lookup, refill, decoding and Goose name copying. '
           'Idle includes poll. Open includes input packing, validation, syscalls and sorting.',
)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(args.output)
