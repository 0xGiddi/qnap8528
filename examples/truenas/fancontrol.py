#!/usr/bin/env python3
"""Conservative QNAP cooling example. Explicitly configure the fan channels."""

import json
import logging
import math
import os
from pathlib import Path
import signal
import time


SYSFS = Path(os.environ.get("QNAP_SYSFS", "/hostsys"))
PLATFORM = Path(os.environ.get("QNAP_PLATFORM", "/platform"))
CPU_PWM = int(os.environ["QNAP_CPU_PWM"])
CASE_PWMS = [int(value) for value in os.environ["QNAP_CASE_PWMS"].split(",")]
FANS = [int(value) for value in os.environ["QNAP_FANS"].split(",")]
STATUS = Path(os.environ.get("QNAP_STATUS", "/run/fancontrol.json"))
INTERVAL = 10
stopping = False


def temperature(path):
    value = int(path.read_text()) / 1000
    if not math.isfinite(value) or not 0 <= value <= 125:
        raise ValueError(f"Invalid temperature from {path}: {value}")
    return value


def curve(value, low, high, minimum):
    return round(min(255, max(minimum, minimum + (255 - minimum) * (value - low) / (high - low))))


def write_pwm(controller, channel, value):
    (controller / f"pwm{channel}").write_text(str(value))


def full_speed(controller):
    for channel in sorted(set([CPU_PWM, *CASE_PWMS])):
        try:
            write_pwm(controller, channel, 255)
        except OSError:
            logging.exception("Could not set fan channel %s to full speed", channel)


def stop(_signum, _frame):
    global stopping
    stopping = True


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    controller = next((PLATFORM / "qnap8528/hwmon").glob("hwmon*"))
    full_speed(controller)
    cpu = next(path / "temp1_input" for path in (SYSFS / "class/hwmon").glob("hwmon*")
               if (path / "name").read_text().strip() in ("k10temp", "coretemp"))
    # Keep the original paths so a disappearing drive sensor triggers full speed.
    disks = []
    nvmes = []
    for path in (SYSFS / "class/hwmon").glob("hwmon*"):
        name = (path / "name").read_text().strip()
        if name == "drivetemp":
            disks.append(path / "temp1_input")
        elif name == "nvme":
            nvmes.append(path / "temp1_input")
    if not disks:
        full_speed(controller)
        raise RuntimeError("No disk temperature sensors found")

    previous = {}
    warmup_until = time.monotonic() + 30
    last_log = 0
    try:
        while not stopping:
            healthy = False
            readings = {}
            try:
                readings = {"cpu": temperature(cpu),
                            "disk": max(temperature(path) for path in disks),
                            "nvme": max((temperature(path) for path in nvmes), default=0)}
                cpu_duty = curve(readings["cpu"], 45, 75, 102)
                case_duty = max(curve(readings["disk"], 35, 50, 115),
                                curve(readings["nvme"], 50, 70, 115),
                                curve(readings["cpu"], 60, 80, 115))
                speeds = {channel: int((controller / f"fan{channel}_input").read_text()) for channel in FANS}
                if time.monotonic() < warmup_until:
                    cpu_duty = case_duty = 255
                elif any(speed <= 0 for speed in speeds.values()):
                    raise RuntimeError(f"Fan stopped or RPM unavailable: {speeds}")
                duties = {channel: case_duty for channel in CASE_PWMS}
                duties[CPU_PWM] = cpu_duty
                for channel, requested in duties.items():
                    old = previous.get(channel, 255)
                    # Increases apply immediately. A 5% band prevents small downward oscillations.
                    if requested > old or old - requested >= 13:
                        write_pwm(controller, channel, requested)
                        previous[channel] = requested
                healthy = True
                if time.monotonic() - last_log >= 60:
                    logging.info("Temperatures=%s RPM=%s PWM=%s", readings, speeds, previous)
                    last_log = time.monotonic()
            except (OSError, ValueError, RuntimeError):
                logging.exception("Cooling check failed; requesting full fan speed")
                full_speed(controller)
                previous.clear()
            temporary_status = STATUS.with_suffix(".tmp")
            temporary_status.write_text(json.dumps({
                "updated": time.time(), "healthy": healthy, "temperatures": readings,
            }))
            temporary_status.replace(STATUS)
            for _ in range(INTERVAL):
                if stopping:
                    break
                time.sleep(1)
    finally:
        full_speed(controller)


if __name__ == "__main__":
    main()
