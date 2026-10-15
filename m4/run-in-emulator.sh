#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
exec python3 "$HERE/../tools/emulator_check.py" --app "$HERE/vcnano.hbapp" --output "$HERE/run" --prefix vcnano "$@"
