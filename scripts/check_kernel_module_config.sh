#!/usr/bin/env bash
set -eu

: "${KERNEL_BUILD:?set KERNEL_BUILD to the configured vendor kernel build directory}"

config="$KERNEL_BUILD/.config"
symbols="$KERNEL_BUILD/Module.symvers"

test -f "$config" || { echo "missing kernel .config: $config" >&2; exit 1; }
test -f "$symbols" || { echo "missing Module.symvers: build the configured kernel/modules first" >&2; exit 1; }

for setting in \
    CONFIG_IIO=y \
    CONFIG_IIO_BUFFER=y \
    CONFIG_IIO_TRIGGER=y \
    CONFIG_IIO_TRIGGERED_BUFFER=m \
    CONFIG_IIO_KFIFO_BUF=m \
    CONFIG_REGMAP_SPI=y; do
    grep -qx "$setting" "$config" || {
        echo "kernel configuration mismatch: expected $setting" >&2
        exit 1
    }
done

for symbol in iio_triggered_buffer_setup iio_triggered_buffer_cleanup; do
    awk -v name="$symbol" '$2 == name { found = 1 } END { exit !found }' "$symbols" || {
        echo "kernel build does not export $symbol" >&2
        exit 1
    }
done

echo "kernel IIO configuration and triggered-buffer exports: PASS"
