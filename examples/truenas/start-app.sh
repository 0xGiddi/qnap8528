#!/bin/sh
# Run as a TrueNAS POSTINIT task after the source dataset is mounted.
set -eu

start_app() {
    app=$1
    attempt=1
    until midclt call -j app.start "$app"; do
        if [ "$attempt" -ge 12 ]; then
            echo "Unable to start $app; check the startup task and app logs" >&2
            return 1
        fi
        echo "Waiting for TrueNAS Apps before starting $app (attempt $attempt/12)" >&2
        attempt=$((attempt + 1))
        sleep 10
    done
}

module_ready() {
    test -d "$sysfs/module/qnap8528" || return 1
    for hwmon in "$sysfs"/devices/platform/qnap8528/hwmon/hwmon*; do
        test ! -d "$hwmon" || return 0
    done
    return 1
}

if [ "$#" -eq 0 ]; then
    set -- qnap-hardware
fi
start_app "$1"
shift
if [ "$#" -eq 0 ]; then
    exit 0
fi

# app.start can finish before a one-shot loader exits. Do not create the
# optional cooling container until its narrow sysfs bind source is ready.
sysfs=${QNAP_SYSFS:-/sys}
attempt=1
until module_ready; do
    if [ "$attempt" -ge 30 ]; then
        echo "QNAP module/hwmon device did not become available; check the loader logs before starting cooling" >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    sleep 1
done
for app in "$@"; do
    start_app "$app"
done
