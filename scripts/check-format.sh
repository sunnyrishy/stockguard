#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
formatter="${CLANG_FORMAT:-clang-format}"
"$formatter" --dry-run --Werror include/stockguard/*.hpp src/*.cpp tests/*.cpp benchmarks/*.cpp
