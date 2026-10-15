#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
c++ -std=c++11 -DRW_GLES1 -I"$HERE" -I"$HERE/../vendor/librw" -I"$HERE/../vendor/revc-viewer/fakerw" -I/home/pavliukov/ipodwork/nano7-untethered/nanoapps/sdk "$HERE/asset-check.cpp" "$HERE/model.cpp" "$HERE/stream.cpp" "$HERE/fakerw.cpp" "$HERE/rw_helpers.cpp" "$HERE/nano.cpp" "$HERE/../m2/mock-gl.cpp" "$HERE/../m2/host-build/src/librw.a" -lm -o "$HERE/asset-check"
"$HERE/asset-check"
