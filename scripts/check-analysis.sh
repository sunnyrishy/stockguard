#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for source in src/*.cpp; do
  "${CLANG_TIDY:-clang-tidy}" -p build "$source" --extra-arg=-Wno-unknown-warning-option
 done
