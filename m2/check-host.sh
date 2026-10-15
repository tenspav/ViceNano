#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$HERE/../vendor/librw" -B "$HERE/host-build" -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
cmake --build "$HERE/host-build" -j4
c++ -DRW_GLES1 -I/home/pavliukov/ipodwork/nano7-untethered/nanoapps/sdk "$HERE/host-check.cpp" "$HERE/mock-gl.cpp" "$HERE/host-build/src/librw.a" -lm -o "$HERE/host-check"
"$HERE/host-check"
