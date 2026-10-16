#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
OUTPUT="$HERE/pass"
CLOCK=MAX
case "${1:-}" in
  stock) OUTPUT="$HERE/stock-pass"; CLOCK=STOCK; shift ;;
  unlocked) shift ;;
esac
make -C "$HERE"
python3 "$HERE/emulator_check.py" --app "$HERE/vcperf.hbapp" --output "$OUTPUT" --prefix vcperf --warmup 8 --samples 24 --audio-out --clock-profile "$CLOCK" --keep-awake "$@"
python3 "$HERE/verify.py" --output "$OUTPUT"
