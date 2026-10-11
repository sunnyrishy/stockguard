"""Verify that a successful-looking process cannot produce an unverified benchmark report."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
RUNNER = ROOT / 'scripts' / 'run-benchmarks.py'


class BenchmarkReporting(unittest.TestCase):
    def test_nonzero_exit_is_not_recorded_as_success(self):
        self.run_failed_engine(7)

    def test_valid_json_without_database_is_not_recorded_as_success(self):
        self.run_failed_engine(0)

    def run_failed_engine(self, exit_code):
        with tempfile.TemporaryDirectory(prefix='stockguard-runner-test-') as directory:
            directory = pathlib.Path(directory)
            executable = directory / 'fake-engine'
            report = {'observations': 100, 'workers': 1,
                      'throughput': {'total_applied_events': 100,
                                     'duplicate_replays_prevented': 100}}
            executable.write_text(f'#!{sys.executable}\nimport sys\n'
                                  f'print({json.dumps(json.dumps(report))})\n'
                                  f'sys.exit({exit_code})\n')
            executable.chmod(0o700)
            output = directory / 'reports'
            result = subprocess.run([sys.executable, str(RUNNER), '--executable', str(executable),
                                     '--counts', '100', '--workers', '1', '--output', str(output)],
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(list(output.glob('*.json')), [])


if __name__ == '__main__':
    unittest.main()
