#!/usr/bin/env bash
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
host_build="${HOST_BUILD_DIR:-$project_root/build-linux-verify}"

cmake -S "$project_root" -B "$host_build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$host_build" --parallel 2
(
  cd "$host_build"
  ctest --output-on-failure
)
"$host_build/apps/conditiond" --simulate --samples 200

if [ -n "${CROSS_COMPILE:-}" ]; then
  arm_build="${ARM_BUILD_DIR:-$project_root/build-arm-verify}"
  cmake -S "$project_root" -B "$arm_build" \
    -DCMAKE_TOOLCHAIN_FILE="$project_root/cmake/toolchains/imx6ull-gcc.cmake" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "$arm_build" --parallel 2
  file "$arm_build/apps/conditiond"
fi
