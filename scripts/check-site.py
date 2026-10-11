#!/usr/bin/env python3
"""Verify the published static site from a runner with access to its domain."""
import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request


def verify(base):
    expected = {'': 'id="studio"', 'app.js': 'animateScan',
                'simulator.js': 'createStockGuardSimulator', 'style.css': 'prefers-reduced-motion'}
    for relative, marker in expected.items():
        request = urllib.request.Request(urllib.parse.urljoin(base, relative),
                                         headers={'Cache-Control': 'no-cache'})
        with urllib.request.urlopen(request, timeout=20) as response:
            if response.status != 200 or marker not in response.read().decode():
                raise ValueError(f'Unexpected published asset: {relative or "index"}')
    with urllib.request.urlopen(urllib.parse.urljoin(base, 'benchmarks'), timeout=20) as response:
        if not isinstance(json.load(response), dict):
            raise ValueError('Published benchmark report is not an object')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('url')
    args = parser.parse_args()
    if urllib.parse.urlsplit(args.url).scheme != 'https':
        parser.error('Use the HTTPS deployment URL')
    base = args.url.rstrip('/') + '/'
    for attempt in range(6):
        try:
            verify(base)
            print(f'Published site verified: {base} (page, scripts, styles and benchmark report)')
            return
        except (OSError, ValueError, urllib.error.HTTPError) as error:
            if attempt == 5:
                raise SystemExit(f'Published site check failed: {error}') from error
            print(f'Waiting for deployment propagation: {error}', flush=True)
            time.sleep(3)


if __name__ == '__main__':
    main()
