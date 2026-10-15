#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
c++ -std=c++11 -O2 -Wall -Wextra -fsanitize=address,undefined -DAUDIO_HOST "$HERE/audio.cpp" "$HERE/host-check.cpp" -o "$HERE/host-check"
ASAN_OPTIONS=detect_leaks=0 "$HERE/host-check"
