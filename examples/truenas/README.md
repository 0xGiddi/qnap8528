# TrueNAS SCALE build, load and cooling example

TrueNAS kernel updates can leave a previously working `qnap8528.ko` incompatible
with the running kernel. A startup command containing a fixed kernel version then
fails with `Invalid module format` and fan telemetry disappears.

This example builds against the running host's installed headers in a disposable
container. It keeps modules in separate kernel-version directories on a persistent
dataset. The loader checks the kernel identifier and module checksum before
loading. It never forces an incompatible module into the kernel.

## Prepare the source and paths

Copy a reviewed checkout of this repository to a persistent TrueNAS dataset. Set
these variables when rendering `compose.yaml` on a workstation with Docker Compose.
Use paths as they appear on the NAS, including for `QNAP_SOURCE_DIR`.

```sh
export QNAP_SOURCE_DIR=/mnt/POOL/qnap8528/source
export QNAP_OUTPUT_DIR=/mnt/POOL/qnap8528/modules
```

The host must have matching headers, including `Module.symvers` and generated
configuration, at `/lib/modules/$(uname -r)/build`. The builder needs Internet
access to install its compiler packages on a cache miss. Debian Trixie's compiler
must be suitable for the kernel being built. A failed build prevents loading.
The host operating system stays read-only and needs no development packages.

## Select the cooling channels

Confirm your model in the driver's supported-model table. Read its actual hwmon
files before assigning channels. On a tested TS-873A, the chassis fans share
`pwm1`, the CPU fan uses `pwm7`, and tachometers are `fan1_input`, `fan2_input`, and
`fan7_input`.

```sh
export QNAP_CPU_PWM=7
export QNAP_CASE_PWMS=1
export QNAP_FANS=1,2,7
docker compose -f examples/truenas/compose.yaml config > /tmp/qnap-hardware.yaml
```

These channel assignments are model-specific. The cooling example requires an
AMD `k10temp` or Intel `coretemp` CPU sensor and at least one `drivetemp` disk sensor.
It also considers NVMe composite temperatures when available.

The example uses conservative curves:

| Group | Minimum PWM | Full speed |
| --- | --- | --- |
| CPU | 40% at or below 45°C | 75°C CPU temperature |
| Chassis | 45% at or below 35°C disk temperature | 50°C disk, 70°C NVMe, or 80°C CPU temperature |

Fans run at full speed during the first 30 seconds, after a missing/invalid sensor
reading, after a zero/missing fan tachometer reading, and when the controller exits
normally or receives SIGTERM/SIGINT. An abrupt process kill cannot execute cleanup.
The container restarts automatically, starting with full speed again. Validate
airflow, all tachometers and the curves on your hardware before unattended use.

## Install and verify

In TrueNAS Apps, use **Install via YAML** with the rendered configuration. The
builder and loader are short-lived containers. The cooling container stays
running and has a health check. It has no network access. Loading requires
`SYS_MODULE`. Cooling requires write access to the platform sysfs tree, and has
all Linux capabilities dropped.

Verify the driver, temperatures and fan readings from the NAS shell:

```sh
test -d /sys/module/qnap8528
for directory in /sys/class/hwmon/hwmon*; do
    if [ "$(cat "$directory/name")" = qnap8528 ]; then
        grep . "$directory"/fan*_input "$directory"/pwm* "$directory"/temp*_input
    fi
done
```

Check that all fans respond to a brief increase in PWM, then confirm normal
cooling behavior. The EC can take about 30 seconds to report a changed RPM.
Replace any existing fixed-version module-loading startup task only after these
checks pass. Do not run another fan-control daemon alongside this controller.

After a kernel update, the builder compiles a matching module from the same local
source checkout. Changing to a reviewed source version also invalidates the
source cache. Keep the source and output datasets mounted when Apps start.
When troubleshooting a build failure, inspect the build container's logs and
confirm that the host's headers match its kernel and compiler requirements.

Stopping the app requests full fan speed. The module stays loaded until explicitly
unloaded or the NAS restarts. Kernel updates still require the driver to support
that kernel's API. This example does not modify the TrueNAS host installation or
provide vendor support for third-party drivers.

## Cooling tests

Run the synthetic sensor and fan-fault tests on a workstation:

```sh
python3 -m unittest discover -s examples/truenas -p 'test_*.py'
```
