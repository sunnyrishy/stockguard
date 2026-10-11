#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .runtime
port="${STOCKGUARD_PORT:-8080}"
if [[ ! "$port" =~ ^[0-9]+$ ]] || (( port < 1 || port > 65535 )); then
  echo 'STOCKGUARD_PORT must be 1 to 65535' >&2
  exit 1
fi
ready() {
  curl -fsS --max-time 3 "http://127.0.0.1:$port/health" 2>/dev/null |
    python3 -c 'import json,sys; r=json.load(sys.stdin);sys.exit(0 if r.get("ok") and r.get("application")=="StockGuard" else 1)' 2>/dev/null
}
if ready; then echo "StockGuard is healthy on port $port"; exit 0; fi
nohup python3 dashboard/server.py --port "$port" > .runtime/demo.log 2>&1 &
server_pid=$!
printf '%s\n' "$server_pid" > .runtime/demo.pid
for attempt in {1..30}; do
  if ready; then echo "StockGuard is healthy on port $port"; exit 0; fi
  if ! kill -0 "$server_pid" 2>/dev/null; then cat .runtime/demo.log >&2; exit 1; fi
  sleep 0.2
done
echo 'StockGuard readiness check failed, inspect .runtime/demo.log' >&2
exit 1
