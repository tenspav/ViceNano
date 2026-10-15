#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
exec python3 "$HERE/../tools/emulator_check.py" --app "$HERE/vcstream.hbapp" --output "$HERE/run" --prefix vcstream --keep-awake --warmup 30 --samples 16 "$@"
