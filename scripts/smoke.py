#!/usr/bin/env python3
"""Functional HTTP check using a disposable database and a server owned by this test."""
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
executable = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'build' / 'stockguard'
with tempfile.TemporaryDirectory(prefix='stockguard-smoke-') as tmp:
    process = subprocess.Popen([sys.executable, str(ROOT / 'dashboard' / 'server.py'),
                                '--executable', str(executable), '--db', tmp + '/demo.db',
                                '--port', '0'], stdout=subprocess.PIPE, text=True)
    try:
        line = process.stdout.readline().strip()
        assert 'listening on port' in line, line
        port = int(line.rsplit(' ', 1)[1])
        base = f'http://127.0.0.1:{port}'

        def api(request):
            req = urllib.request.Request(base + '/api', data=json.dumps(request).encode(),
                                         headers={'Content-Type': 'application/json'})
            with urllib.request.urlopen(req, timeout=20) as response:
                data = json.load(response)
                assert data['ok'], data
                return data['result']

        with urllib.request.urlopen(base + '/health') as response:
            assert json.load(response)['ok']
        with urllib.request.urlopen(base + '/') as response:
            assert b'StockGuard' in response.read()
            assert "frame-ancestors 'none'" in response.headers['Content-Security-Policy']
        inventory = api({'op': 'inventory'})[0]
        o = {'id': 'smoke-scan', 'scan_id': 'smoke-batch', 'product_id': 'milk',
             'location_id': 'demo', 'quantity': 101, 'confidence': 0.99, 'complete': True,
             'expected_version': inventory['version'], 'observed_at': int(time.time()),
             'source': 'smoke'}
        assert api({'op': 'ingest', 'observation': o})['status'] == 'ACCEPT'
        assert api({'op': 'ingest', 'observation': o})['replayed']
        o.update(id='smoke-review', scan_id='smoke-review-batch', quantity=160, expected_version=2)
        assert api({'op': 'ingest', 'observation': o})['status'] == 'REVIEW'
        assert api({'op': 'inventory'})[0]['quantity'] == 101
        assert len(api({'op': 'pending'})) == 1
        assert api({'op': 'review', 'observation_id': 'smoke-review', 'actor': 'smoke-reviewer',
                    'action': 'correct', 'quantity': 99, 'expected_version': 2,
                    'verified_at': int(time.time())})['quantity'] == 99
        assert api({'op': 'inventory'})[0]['version'] == 3
        assert len(api({'op': 'audit'})) == 3
        assert len(api({'op': 'pending'})) == 0
        for headers, expected in [({'Host': 'evil.example'}, 403),
                                  ({'Content-Type': 'text/plain'}, 415),
                                  ({'Content-Type': 'application/json', 'Origin': 'https://evil.example'}, 403)]:
            try:
                urllib.request.urlopen(urllib.request.Request(base + '/api', data=b'{}', headers=headers))
                raise AssertionError('Unsafe request was accepted')
            except urllib.error.HTTPError as error:
                assert error.code == expected, error.code
        # Parser-level failures preserve the executable exit code and return structured errors.
        for payload in ['{"op":', '[' * 100 + '0' + ']' * 100, 'x' * (1048576 + 1)]:
            result = subprocess.run([str(executable), '--db', tmp + '/demo.db'],
                                    input=payload + '\n', text=True, capture_output=True, timeout=10)
            assert result.returncode == 1, result.stderr
            assert not json.loads(result.stdout)['ok']
        print('HTTP smoke passed: ingestion, replay, review, audit, origin checks, parser bounds')
    finally:
        process.terminate()
        process.wait(timeout=10)
