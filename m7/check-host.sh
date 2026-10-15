#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
g++ -std=c++11 -fsanitize=address,undefined -g "$HERE/input.cpp" "$HERE/host-check.cpp" -o /tmp/vicenano-m7-check
ASAN_OPTIONS=detect_leaks=0 /tmp/vicenano-m7-check
