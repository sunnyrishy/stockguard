#!/usr/bin/env python3
"""Local demonstration adapter. Policy and persistence always run in C++."""
import argparse
import json
import pathlib
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent.parent


def command(executable, database, request):
    result = subprocess.run(
        [str(executable), '--db', str(database)],
        input=json.dumps(request) + '\n', text=True, capture_output=True, timeout=15,
    )
    if result.returncode not in (0, 1) or len(result.stdout) > 8 * 1024 * 1024:
        raise RuntimeError('C++ backend failed')
    value = json.loads(result.stdout)
    if bool(value.get('ok')) != (result.returncode == 0):
        raise RuntimeError('Backend exit status disagrees with response')
    return value


class BoundedServer(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 16

    def __init__(self, address, handler):
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(address, handler)

    def process_request(self, request, client_address):
        self.slots.acquire()
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


def make_handler(executable, database):
    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.connection.settimeout(10)

        def respond(self, code, body, content_type='application/json'):
            self.send_response(code)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy',
                             "default-src 'self'; script-src 'self'; style-src 'self'; "
                             "frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

        def valid_origin(self):
            port = self.server.server_port
            allowed = {f'127.0.0.1:{port}', f'localhost:{port}'}
            host = self.headers.get('Host', '')
            origin = self.headers.get('Origin')
            return host in allowed and (origin is None or origin == f'http://{host}')

        def do_GET(self):
            if not self.valid_origin():
                self.respond(403, b'{"ok":false,"error":"invalid_origin"}')
                return
            assets = {'/': ('index.html', 'text/html; charset=utf-8'),
                      '/app.js': ('app.js', 'text/javascript; charset=utf-8'),
                      '/style.css': ('style.css', 'text/css; charset=utf-8'),
                      '/simulator.js': ('simulator.js', 'text/javascript; charset=utf-8')}
            if self.path == '/health':
                try:
                    result = command(executable, database, {'op': 'metrics'})
                    result['application'] = 'StockGuard'
                    self.respond(200 if result['ok'] else 503, json.dumps(result).encode())
                except (RuntimeError, subprocess.TimeoutExpired, ValueError):
                    self.respond(503, b'{"ok":false,"error":"backend_unavailable"}')
                return
            if self.path == '/benchmarks':
                report = ROOT / 'reports' / 'benchmark-10000.json'
                if report.exists():
                    self.respond(200, report.read_bytes())
                else:
                    self.respond(404, b'{"error":"No measured benchmark report"}')
                return
            if self.path not in assets:
                self.respond(404, b'{"error":"not_found"}')
                return
            filename, kind = assets[self.path]
            self.respond(200, (ROOT / 'dashboard' / filename).read_bytes(), kind)

        def do_POST(self):
            if self.path != '/api' or not self.valid_origin():
                self.respond(403, b'{"ok":false,"error":"invalid_route_or_origin"}')
                return
            if self.headers.get('Content-Type', '').split(';')[0] != 'application/json':
                self.respond(415, b'{"ok":false,"error":"json_required"}')
                return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= 1024 * 1024:
                    self.respond(413, b'{"ok":false,"error":"request_size"}')
                    return
                request = json.loads(self.rfile.read(size))
                result = command(executable, database, request)
                self.respond(200 if result['ok'] else 400, json.dumps(result).encode())
            except (ValueError, RecursionError):
                self.respond(400, b'{"ok":false,"error":"invalid_json"}')
            except (RuntimeError, subprocess.TimeoutExpired, OSError):
                self.respond(503, b'{"ok":false,"error":"backend_unavailable"}')

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=pathlib.Path, default=ROOT / 'build' / 'stockguard')
    parser.add_argument('--db', type=pathlib.Path, default=ROOT / '.runtime' / 'demo.db')
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    args.db.parent.mkdir(parents=True, exist_ok=True)
    if not args.executable.is_file():
        parser.error('Build the C++ executable first')
    # Initialize only an absent demo product and inventory. Existing catalog data is preserved.
    catalog = command(args.executable, args.db, {'op': 'products', 'id': 'milk'})
    if not catalog['ok']:
        raise RuntimeError('Cannot read catalog')
    if not catalog['result']:
        result = command(args.executable, args.db, {'op': 'product_put', 'id': 'milk',
                         'name': 'Whole milk', 'category': 'dairy', 'unit': 'carton', 'status': 'active'})
        if not result['ok']:
            raise RuntimeError('Cannot initialize demo catalog')
    inventory = command(args.executable, args.db, {'op': 'inventory'})
    if not inventory['ok']:
        raise RuntimeError('Cannot read inventory')
    if not any(row['product_id'] == 'milk' and row['location_id'] == 'demo'
               for row in inventory['result']):
        result = command(args.executable, args.db, {'op': 'initialize', 'product_id': 'milk',
                         'location_id': 'demo', 'quantity': 100, 'actor': 'demo-seed'})
        if not result['ok']:
            raise RuntimeError('Cannot initialize demo inventory')
    server = BoundedServer(('127.0.0.1', args.port), make_handler(args.executable, args.db))
    print(f'StockGuard local demo listening on port {server.server_port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
