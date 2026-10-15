#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
python3 "$HERE/emulator_check.py" --app "$HERE/vcinput.hbapp" --output "$HERE/pass" --prefix vcinput --warmup 8 --samples 24 "$@"
python3 "$HERE/verify.py"
