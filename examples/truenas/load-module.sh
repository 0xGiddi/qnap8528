#!/bin/sh
set -eu

test -d /sys/module/qnap8528 && exit 0
kernel=$(uname -r)
directory=${QNAP_OUTPUT_DIR:-/output}/$kernel
test "$(cut -d ' ' -f 1 "$directory/vermagic.txt")" = "$kernel"
(cd "$directory" && sha256sum -c module.sha256)
insmod "$directory/qnap8528.ko"
test -d /sys/module/qnap8528
