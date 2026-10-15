#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
python3 "$HERE/extract.py" --model nbdecoshop01 --txd nbdecoshop01 "$@"
cmake -S "$HERE/../vendor/librw" -B "$HERE/host-build" -DLIBRW_PLATFORM=NULL -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
cmake --build "$HERE/host-build" -j4
c++ -std=c++11 -I"$HERE/../vendor/librw" "$HERE/converter.cpp" "$HERE/host-build/src/librw.a" -lm -o "$HERE/converter"
mapfile -t INPUTS < <(python3 -c 'import json,sys; print("\n".join(json.load(open(sys.argv[1]))))' "$HERE/generated/selection.json")
"$HERE/converter" "$HERE/generated/${INPUTS[0]}" "$HERE/generated/${INPUTS[1]}" "$HERE/generated/asset.h"
