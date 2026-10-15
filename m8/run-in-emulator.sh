#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
python3 "$HERE/emulator_check.py" --app "$HERE/vcperf.hbapp" --output "$HERE/pass" --prefix vcperf --warmup 8 --samples 24 --audio-out "$@"
python3 "$HERE/verify.py"
