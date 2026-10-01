#!/usr/bin/env bash
set -eu

: "${KERNEL_SRC:?set KERNEL_SRC to the ALIENTEK Linux 4.1.15 tree}"
: "${CROSS_COMPILE:?set CROSS_COMPILE to the arm-linux-gnueabihf- prefix}"

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module_dir="$project_root/kernel"
mkdir -p "$module_dir/.tmp_versions"

# Source/API verification for a vendor tree whose current .config may not yet
# enable IIO buffers.  A deployable .ko still requires rebuilding the target
# kernel with deploy/imx6ull_iio_defconfig.fragment.
make -B -C "$KERNEL_SRC" \
  M="$module_dir" \
  ARCH=arm \
  CROSS_COMPILE="$CROSS_COMPILE" \
  KCFLAGS="-DCONFIG_IIO_BUFFER -DCONFIG_IIO_TRIGGER -DCONFIG_IIO_TRIGGERED_BUFFER -DCONFIG_IIO_CONSUMERS_PER_TRIGGER=2" \
  icm20608_iio.o

