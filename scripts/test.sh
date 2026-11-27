#!/bin/bash -e
# Run the automated tests of an existing build (see ./scripts/build.sh).
# Uses the ctest bundled with the CMake that ./cmakew downloads, so no system
# CMake is needed.

cd "$(dirname "$0")/.."

cmake_version=$(sed -n 's/^cmake_version="\(.*\)"/\1/p' cmakew)
ctest_bin="./.cmake/cmake-${cmake_version}/bin/ctest"

if [ ! -x "$ctest_bin" ]; then
    echo "ctest not found at $ctest_bin, run ./scripts/build.sh first" >&2
    exit 1
fi

"$ctest_bin" --test-dir build --output-on-failure "$@"
