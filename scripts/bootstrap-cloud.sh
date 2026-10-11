#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# User-local installation for the restricted cloud machine. No system package writes.
tools_dir="${STOCKGUARD_TOOLS_DIR:-/workspace/.stockguard-tools}"
if [ ! -x "$tools_dir/bin/python" ]; then python3 -m venv "$tools_dir"; fi
"$tools_dir/bin/pip" install --disable-pip-version-check cmake==3.31.6 ninja==1.11.1.4 clang-format==19.1.7 clang-tidy==19.1.0.1
export PATH="$tools_dir/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j "${STOCKGUARD_BUILD_JOBS:-2}"
ctest --test-dir build --output-on-failure
