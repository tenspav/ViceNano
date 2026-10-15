#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
exec python3 "$HERE/emulator_check.py" "$@"
