#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE"
VNC=(--vnc 127.0.0.1:5)
if [[ "${1:-}" == "--headless" ]]; then VNC=(); shift; fi
exec python3 "$HERE/../tools/emulator_check.py" --app "$HERE/vcnano.hbapp" --output "$HERE/run" --prefix vcnano "${VNC[@]}" "$@"
