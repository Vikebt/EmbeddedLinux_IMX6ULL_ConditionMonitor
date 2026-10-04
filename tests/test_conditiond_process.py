#!/usr/bin/env python3
"""Exercise the Linux daemon as a process, including signal-driven shutdown."""

import json
import signal
import subprocess
import sys
import time


def events(output):
    decoded = [json.loads(line) for line in output.splitlines() if line.strip()]
    for event in decoded:
        assert isinstance(event["timestamp_ns"], int)
        assert event["timestamp_ns"] > 0
        assert event["state"] in {"normal", "tilted", "impact", "sensor_fault"}
    return decoded


def main(executable):
    completed = subprocess.run(
        [executable, "--simulate", "--samples", "30"],
        capture_output=True,
        text=True,
        timeout=5,
        check=True,
    )
    states = [event["state"] for event in events(completed.stdout)]
    assert states == ["normal", "impact", "normal"], states

    for stop_signal in (signal.SIGTERM, signal.SIGINT):
        process = subprocess.Popen(
            [executable, "--simulate"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            time.sleep(0.05)
            assert process.poll() is None, "daemon exited before signal"
            process.send_signal(stop_signal)
            output, error = process.communicate(timeout=3)
            assert process.returncode == 0, (stop_signal, process.returncode, error)
            # The bounded run above checks JSON output.  Here the exit code
            # specifically proves that signalfd handled the stop request.
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    invalid = subprocess.run(
        [executable, "--samples", "1"],
        capture_output=True,
        text=True,
        timeout=3,
    )
    assert invalid.returncode == 2, invalid


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_conditiond_process.py CONDITIOND")
    main(sys.argv[1])
