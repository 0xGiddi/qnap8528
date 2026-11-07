#!/bin/sh
# Run inside the builder container, with the host headers mounted read-only.
set -eu

source_dir=${QNAP_SOURCE_DIR:-/source}
output_dir=${QNAP_OUTPUT_DIR:-/output}
kernel=$(uname -r)
headers=/lib/modules/$kernel/build
destination=$output_dir/$kernel

test -r "$headers/Module.symvers"
test -r "$headers/include/generated/autoconf.h"
release=$(sed -n 's/^#define UTS_RELEASE "\(.*\)"/\1/p' "$headers/include/generated/utsrelease.h")
if [ "$release" != "$kernel" ]; then
    echo "Kernel headers do not match the running kernel: $release != $kernel" >&2
    exit 1
fi

source_hash=$(cd "$source_dir/src" && sha256sum Makefile qnap8528.c qnap8528.h | sha256sum | cut -d ' ' -f 1)
if [ -f "$destination/source.sha256" ] &&
    [ "$(cat "$destination/source.sha256")" = "$source_hash" ] &&
    [ "$(cut -d ' ' -f 1 "$destination/vermagic.txt")" = "$kernel" ] &&
    (cd "$destination" && sha256sum -c module.sha256); then
    echo "Using verified qnap8528 module for $kernel"
    exit 0
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends build-essential kmod libelf-dev
mkdir -p /build
cp -R "$source_dir/src/." /build/
make -C "$headers" M=/build -j"${QNAP_BUILD_JOBS:-2}" modules
vermagic=$(modinfo -F vermagic /build/qnap8528.ko)
if [ "${vermagic%% *}" != "$kernel" ]; then
    echo "Refusing to publish a module for a different kernel: $vermagic" >&2
    exit 1
fi

mkdir -p "$output_dir"
staging=$(mktemp -d "$output_dir/.build-XXXXXX")
trap 'rm -rf "$staging"' EXIT HUP INT TERM
cp /build/qnap8528.ko "$staging/qnap8528.ko"
printf '%s\n' "$vermagic" > "$staging/vermagic.txt"
printf '%s\n' "$source_hash" > "$staging/source.sha256"
(cd "$staging" && sha256sum qnap8528.ko > module.sha256)
chmod 755 "$staging"
chmod 644 "$staging"/*
mkdir -p "$destination"
# Publish the checksum last. An interrupted publication fails the next cache check.
mv "$staging/qnap8528.ko" "$destination/qnap8528.ko"
mv "$staging/vermagic.txt" "$destination/vermagic.txt"
mv "$staging/source.sha256" "$destination/source.sha256"
mv "$staging/module.sha256" "$destination/module.sha256"
echo "Built qnap8528 for $kernel"
