#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$HERE/../vendor/librw" -B "$HERE/../m2/host-build" -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
cmake --build "$HERE/../m2/host-build" -j4
c++ -std=c++11 -DRW_GLES1 -I"$HERE" -I"$HERE/../vendor/librw" -I"$HERE/../vendor/revc-viewer/fakerw" -I/home/pavliukov/ipodwork/nano7-untethered/nanoapps/sdk "$HERE/asset-check.cpp" "$HERE/model.cpp" "$HERE/fakerw.cpp" "$HERE/rw_helpers.cpp" "$HERE/nano.cpp" "$HERE/../m2/mock-gl.cpp" "$HERE/../m2/host-build/src/librw.a" -lm -o "$HERE/asset-check"
"$HERE/asset-check"

# The same reduced adapter must still accept M3's cone pack.
if [[ -f "$HERE/../m3/generated/asset.h" ]]; then
 c++ -std=c++11 -DRW_GLES1 '-DASSET_HEADER="../m3/generated/asset.h"' -I"$HERE" -I"$HERE/../vendor/librw" -I"$HERE/../vendor/revc-viewer/fakerw" -I/home/pavliukov/ipodwork/nano7-untethered/nanoapps/sdk "$HERE/asset-check.cpp" "$HERE/model.cpp" "$HERE/fakerw.cpp" "$HERE/rw_helpers.cpp" "$HERE/nano.cpp" "$HERE/../m2/mock-gl.cpp" "$HERE/../m2/host-build/src/librw.a" -lm -o "$HERE/cone-check"
 "$HERE/cone-check"
fi
