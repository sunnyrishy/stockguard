# Verification
Final environment: Linux x86_64, GCC 14.2, CMake 3.31.6, SQLite 3.54.0, GoogleTest 1.16.0.
Default build uses -Wall -Wextra -Wpedantic -Werror for all StockGuard C++ targets.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 scripts/smoke.py
scripts/check-format.sh
scripts/check-analysis.sh
```

Use separate directories for sanitizers:

```sh
cmake -S . -B build-asan -G Ninja -DSTOCKGUARD_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j 2
ctest --test-dir build-asan --output-on-failure
cmake -S . -B build-tsan -G Ninja -DSTOCKGUARD_TSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan -j 2
ctest --test-dir build-tsan -R 'Concurrent|SharedStore' --output-on-failure
```

TSan uses the single upstream WAL-header suppression described in
[ADR 002](decisions/002-sqlite-sanitizers.md). No StockGuard function is suppressed.
ASan/UBSan instruments the application and compiled SQLite source. Never combine ASan and TSan.

The 33 GoogleTests cover rule boundaries, malformed fields, public API arithmetic safety,
SQLite integration, immutable decisions, replay across restarts, scan-key duplication,
partial batch success, movements, catalog-unit invariants, review correction, inventory-independent rejection, durable rejection replay, conflicting
reviewers, foreign keys, migrations, pagination and ledger/audit chains. Fault injection
throws after observation insertion, inventory update and before commit. A child exits in
an uncommitted write and the parent verifies recovery and safe replay. Concurrent tests
use independent connections as well as a shared Store, with explicit exception counters.

The HTTP smoke uses a fresh temporary database and starts and stops its own service. It
checks backend health, accepted scan, replay, review correction, unchanged inventory on
REVIEW, audit count, Host/Origin/Content-Type rejection, oversized input and JSON depth.
It preserves C++ exit status. CLI input errors return structured results and a failed exit.

An optional real-browser test drives DOM controls and creates the documented screenshot.
Install development-only packages in an ignored directory:

```sh
npm install --prefix .tools/browser @sparticuz/chromium@153.0.0 playwright-core@1.61.1
NODE_PATH="$PWD/.tools/browser/node_modules" node scripts/browser-smoke.cjs
```

Node 22+ and a compatible Linux headless Chromium runtime are needed for this optional
check. This is not a production browser dependency. The cloud check used the same versions
and verified no page JavaScript exceptions. The older Chromium 133 runtime crashed before
rendering, so it was replaced rather than counted as a passing UI check.

CI defines Debug, Release, ASan/UBSan and scoped TSan jobs, HTTP smoke, formatting and
clang-tidy checks. These commands were validated locally, but hosted GitHub Actions have
not run because no remote push or publication occurred. WSL2 instructions have not been
independently executed on the owner's Windows machine.

Final local results: Debug 30/30, Release 30/30, ASan/UBSan 30/30, scoped TSan 4/4;
HTTP smoke, browser workflow, formatting and selected static analysis passed.

Benchmark reporting guards: `python3 -m unittest discover -s tests -p 'test_*.py' -v`.
Two Python tests reject false successful reporting. They complement the 33 GoogleTests;
no GoogleTest was removed or skipped. A real 100-event native run additionally verified
closed-database audit and inventory totals. The two reporting guards run in the Debug CI job.
