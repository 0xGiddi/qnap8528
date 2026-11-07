#!/bin/sh
# Run as a TrueNAS POSTINIT task after the source dataset is mounted.
set -eu

app=${1:-qnap-hardware}
attempt=1
while [ "$attempt" -le 12 ]; do
    if midclt call -j app.start "$app"; then
        exit 0
    fi
    echo "Waiting for TrueNAS Apps before starting $app (attempt $attempt/12)" >&2
    attempt=$((attempt + 1))
    sleep 10
done
echo "Unable to start $app; check the startup task and app logs" >&2
exit 1
