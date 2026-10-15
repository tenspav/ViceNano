#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$HERE/../vendor/librw" -B "$HERE/../m2/host-build" -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
cmake --build "$HERE/../m2/host-build" -j4
c++ -std=c++11 -DRW_GLES1 -I"$HERE" -I"$HERE/../vendor/librw" -I"$HERE/../vendor/revc-viewer/fakerw" -I/home/pavliukov/ipodwork/nano7-untethered/nanoapps/sdk "$HERE/asset-check.cpp" "$HERE/model.cpp" "$HERE/../vendor/revc-viewer/fakerw_subset.cpp" "$HERE/../m2/mock-gl.cpp" "$HERE/../m2/host-build/src/librw.a" -lm -o "$HERE/asset-check"
"$HERE/asset-check"
