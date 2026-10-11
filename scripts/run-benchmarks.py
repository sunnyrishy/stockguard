#!/usr/bin/env python3
"""Record fresh, checked benchmark runs without reusing persistent state."""
import argparse
import json
import os
import pathlib
import platform
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--executable', type=pathlib.Path, default=ROOT / 'build-release' / 'stockguard_benchmark')
parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'reports')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='stockguard-benchmark-') as directory:
    for count, workers in [(1000, 1), (10000, 1), (100000, 1), (10000, 2), (10000, 4)]:
        name = f'benchmark-{count}' + (f'-w{workers}' if workers > 1 else '')
        result = subprocess.run([str(args.executable.resolve()), str(count),
                                 str(pathlib.Path(directory) / (name + '.db')), str(workers)],
                                text=True, capture_output=True, check=True)
        report = json.loads(result.stdout)
        report['environment'] = {
            'platform': platform.platform(),
            'cpu': next((line.split(':', 1)[1].strip() for line in open('/proc/cpuinfo')
                         if line.startswith('model name')), ''),
            'available_cpus': len(os.sched_getaffinity(0)),
            'build': 'Release: GCC -O3 -DNDEBUG; SQLite 3.54.0 source built with release flags',
            'measurement': 'one run; /tmp tmpfs database; no HTTP or CLI process startup',
        }
        target = args.output / (name + '.json')
        target.write_text(json.dumps(report, indent=2) + '\n')
        print(f'{name}: {report["throughput"]["observations_per_second"]:.1f} observations/s', flush=True)
