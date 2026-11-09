#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
cargo test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --build build --config Release --target OpenRbPcfTests
ctest --test-dir build -C Release --output-on-failure
