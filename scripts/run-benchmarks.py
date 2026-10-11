#!/usr/bin/env python3
"""Run checked, independent measurements with explicit storage and repetition metadata."""
import argparse
import hashlib
import json
import os
import pathlib
import platform
import statistics
import sqlite3
import subprocess
import tempfile
import uuid
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parent.parent


def bounded_int(low, high):
    def parse(value):
        number = int(value)
        if not low <= number <= high:
            raise argparse.ArgumentTypeError(f'value must be {low} to {high}')
        return number
    return parse


def atomic_json(target, value):
    with tempfile.NamedTemporaryFile(mode='w', dir=target.parent, delete=False) as stream:
        temporary = pathlib.Path(stream.name)
        json.dump(value, stream, indent=2)
        stream.write('\n')
    try:
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)


def verify_database(path, count, workers):
    # The benchmark has exited and closed its writers. Open read-only for independent checks.
    with sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True) as connection:
        integrity = [row[0] for row in connection.execute('PRAGMA integrity_check')]
        if integrity != ['ok'] or connection.execute('PRAGMA foreign_key_check').fetchall():
            raise RuntimeError('Persisted database integrity check failed')
        observations = connection.execute('SELECT count(*) FROM observations').fetchone()[0]
        audits = connection.execute('SELECT count(*) FROM inventory_audit_log').fetchone()[0]
        inventory = connection.execute('SELECT quantity,version FROM inventory').fetchall()
        if (observations != count or audits != count + workers or len(inventory) != workers or
                any(quantity != 100 for quantity, _ in inventory) or
                sum(version - 1 for _, version in inventory) != count):
            raise RuntimeError('Persisted inventory, observation or audit totals are inconsistent')
        return {'integrity_check': 'ok', 'foreign_key_violations': 0,
                'observations': observations, 'audit_rows': audits,
                'inventory_records': len(inventory), 'applied_events': count}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=pathlib.Path,
                        default=ROOT / 'build-release' / 'stockguard_benchmark')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'reports')
    parser.add_argument('--counts', nargs='+', type=bounded_int(100, 1000000),
                        default=[1000, 10000, 100000])
    parser.add_argument('--workers', nargs='+', type=bounded_int(1, 16), default=[1, 2, 4])
    parser.add_argument('--repeats', type=bounded_int(1, 20), default=1)
    parser.add_argument('--storage-directory', type=pathlib.Path,
                        help='Existing directory for temporary databases; default uses system temp')
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error('benchmark executable does not exist')
    if args.storage_directory is not None and not args.storage_directory.is_dir():
        parser.error('storage-directory must already exist')
    args.output.mkdir(parents=True, exist_ok=True)
    configurations = [(count, 1) for count in dict.fromkeys(args.counts)]
    configurations += [(10000, workers) for workers in dict.fromkeys(args.workers) if workers != 1]
    # --counts controls serial sizes; concurrency uses a common 10,000-event workload.
    run_id = str(uuid.uuid4())
    started = datetime.now(timezone.utc).isoformat()
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    cpu = next((line.split(':', 1)[1].strip() for line in open('/proc/cpuinfo')
                if line.startswith('model name')), '')
    cache = executable.parent / 'CMakeCache.txt'
    build = {}
    if cache.is_file():
        for line in cache.read_text().splitlines():
            if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_FLAGS:', 'CMAKE_CXX_FLAGS_RELEASE:')):
                key, value = line.split('=', 1)
                build[key.split(':')[0]] = value
    summary = {'run_id': run_id, 'started_at_utc': started, 'configurations': []}
    with tempfile.TemporaryDirectory(prefix='stockguard-benchmark-',
                                     dir=args.storage_directory) as directory:
        filesystem = subprocess.run(['stat', '-f', '-c', '%T', directory],
                                    capture_output=True, text=True, check=True).stdout.strip()
        storage_path = pathlib.Path(directory).parent.resolve()
        storage = (str(storage_path.relative_to(ROOT)) if storage_path.is_relative_to(ROOT)
                   else str(storage_path))
        for count, workers in configurations:
            name = f'benchmark-{count}' + (f'-w{workers}' if workers > 1 else '')
            runs = []
            for repetition in range(1, args.repeats + 1):
                database = pathlib.Path(directory) / f'{name}-r{repetition}.db'
                result = subprocess.run([str(executable), str(count),
                                         str(database),
                                         str(workers)], text=True, capture_output=True, check=True)
                report = json.loads(result.stdout)
                if (report['observations'] != count or report['workers'] != workers or
                        report['throughput']['total_applied_events'] != count or
                        report['throughput']['duplicate_replays_prevented'] != 100):
                    raise RuntimeError('Benchmark result does not match the selected workload')
                report['post_close_verification'] = verify_database(database, count, workers)
                report['environment'] = {
                    'platform': platform.platform(), 'cpu': cpu,
                    'available_cpus': len(os.sched_getaffinity(0)), 'build': build,
                    'database_storage_directory': storage, 'filesystem_type': filesystem,
                    'measurement': 'per-observation FULL synchronous transactions; no HTTP or CLI startup',
                    'physical_durability_verified': False,
                    'executable_sha256': digest,
                }
                report['measurement_run'] = {'run_id': run_id, 'repetition': repetition,
                                             'repetitions': args.repeats, 'started_at_utc': started}
                filename = name + (f'-r{repetition}' if args.repeats > 1 else '') + '.json'
                atomic_json(args.output / filename, report)
                runs.append({'file': filename, 'throughput': report['throughput']})
                print(f'{name} run {repetition}/{args.repeats}: '
                      f'{report["throughput"]["observations_per_second"]:.1f} observations/s', flush=True)
            rates = [run['throughput']['observations_per_second'] for run in runs]
            summary['configurations'].append({
                'observations': count, 'workers': workers, 'repetitions': args.repeats,
                'throughput_min': min(rates), 'throughput_median': statistics.median(rates),
                'throughput_max': max(rates), 'runs': runs,
            })
        summary['environment'] = {'database_storage_directory': storage,
                                  'filesystem_type': filesystem, 'executable_sha256': digest,
                                  'physical_durability_verified': False}
    summary['finished_at_utc'] = datetime.now(timezone.utc).isoformat()
    atomic_json(args.output / 'summary.json', summary)


if __name__ == '__main__':
    main()
