# TrueNAS SCALE module build/load and optional cooling

TrueNAS kernel updates can leave a previously working `qnap8528.ko` incompatible
with the running kernel. A startup command containing a fixed kernel version then
fails with `Invalid module format` and fan telemetry disappears.

The build/load example uses the running host's installed headers in a disposable
container and keeps separate module builds for each kernel on a persistent dataset.
The loader checks the kernel identifier and module checksum before loading. It
never forces an incompatible module into the kernel. Cooling is optional: use
`compose.yaml` alone with your preferred fan-management solution.

## Build and load the module

Copy a reviewed checkout of this repository to a persistent TrueNAS dataset. Set
these variables when rendering the configuration on a workstation with Docker
Compose. Use paths as they appear on the NAS, including `QNAP_SOURCE_DIR`.

```sh
export QNAP_SOURCE_DIR=/mnt/POOL/qnap8528/source
export QNAP_OUTPUT_DIR=/mnt/POOL/qnap8528/modules
docker compose -f examples/truenas/compose.yaml config > /tmp/qnap-hardware.yaml
```

Install the rendered YAML in TrueNAS Apps as **qnap-hardware** using **Install via
YAML**. The builder and loader are short-lived containers. No fan-channel variables
are needed and no fan-control process is installed by this configuration.

The host must have matching headers, including `Module.symvers` and generated
configuration, at `/lib/modules/$(uname -r)/build`. The builder needs Internet
access to install its compiler packages on a cache miss. Debian Trixie's compiler
must be suitable for the kernel being built. A failed build prevents loading.
The host operating system stays read-only and needs no development packages.

Verify the module and its hwmon files from the NAS shell:

```sh
test -d /sys/module/qnap8528
for directory in /sys/class/hwmon/hwmon*; do
    if [ "$(cat "$directory/name")" = qnap8528 ]; then
        grep . "$directory"/fan*_input "$directory"/pwm* "$directory"/temp*_input
    fi
done
```

## Optional automatic cooling

Confirm your model in the driver's supported-model table. Read its actual hwmon
files before assigning channels. On a tested TS-873A, the chassis fans share
`pwm1`, the CPU fan uses `pwm7`, and tachometers are `fan1_input`, `fan2_input`, and
`fan7_input`.

After the module is loaded, render the separate cooling configuration:

```sh
export QNAP_CPU_PWM=7
export QNAP_CASE_PWMS=1
export QNAP_FANS=1,2,7
docker compose -f examples/truenas/compose.cooling.yaml config > /tmp/qnap-cooling.yaml
```

Install this YAML as a separate TrueNAS app named **qnap-cooling**. Its only
writable sysfs mount is `/sys/devices/platform/qnap8528`; the rest of sysfs is
read-only. That device directory must exist before Docker creates the container.
`create_host_path: false` prevents Docker from trying to create a missing sysfs
path. The boot helper below starts the module app before the cooling app.

The two files can also be merged for inspection or ordinary Docker Compose use:

```sh
docker compose -f examples/truenas/compose.yaml -f examples/truenas/compose.cooling.yaml config
```

On a host using Docker Compose directly, run the base file's `up load` to completion
before using the merged files' `up -d cooling`. On TrueNAS, keep the two apps
separate: a combined app can try to create the cooling container before the loader
has created its bind source on a cold boot. A Compose dependency alone does not
replace this prerequisite. Do not unload the module while cooling is running.

The cooling example requires an AMD `k10temp` or Intel `coretemp` CPU sensor and at
least one `drivetemp` disk sensor. It also considers NVMe composite temperatures
when available. Channel assignments are model-specific and duplicate or overlapping
PWM groups are rejected. The example uses conservative curves:

| Group | Minimum PWM | Full speed |
| --- | --- | --- |
| CPU | 40% at or below 45°C | 75°C CPU temperature |
| Chassis | 45% at or below 35°C disk temperature | 50°C disk, 70°C NVMe, or 80°C CPU temperature |

The controller waits up to 30 seconds for its hwmon device and reports a clear
startup error if it never appears. Once the device is available, fans are requested
at full speed before sensor discovery. Missing CPU/disk sensors stop startup with
an explicit error, with full speed requested again during cleanup.

Fans run at full speed during the first 30 seconds, after a missing/invalid sensor
reading, after a zero/missing fan tachometer reading, and when the controller exits
normally or receives SIGTERM/SIGINT. An abrupt process kill cannot execute cleanup.
The container restarts automatically, starting with full speed again. Verify all
fans respond to a brief increase in PWM and validate airflow and curves before
unattended use. The EC can take about 30 seconds to report a changed RPM. Do not
run another fan-control daemon alongside this controller.

## Boot startup

Register a TrueNAS **Init/Shutdown Script** with **When = POSTINIT**, **Type =
COMMAND**, and **Timeout = 600 seconds**. For build/load only, use:

```sh
/bin/sh /mnt/POOL/qnap8528/source/examples/truenas/start-app.sh qnap-hardware
```

If you installed the optional cooling app, use this instead:

```sh
/bin/sh /mnt/POOL/qnap8528/source/examples/truenas/start-app.sh qnap-hardware qnap-cooling
```

The helper starts apps in argument order, waiting for each job to finish. Before
starting any additional apps, it also waits up to 30 seconds for the module and
QNAP hwmon directory: an app-start job can finish before its one-shot loader exits.
It retries while Apps initializes and stops on failure, so a failed module
build/load does not start cooling. TrueNAS can remove containers when detaching
host-path datasets during shutdown; restart policies alone cannot recreate them. Keep the source and
output datasets mounted when Apps starts.

Replace the previous fixed-kernel startup command only after verifying the new
setup. Reboot the NAS and check that the module and tachometers return without a
manual start; with optional cooling enabled, check its health as well. An app
restart alone does not exercise this boot sequence. Stopping cooling requests full
fan speed. The module stays loaded until explicitly unloaded or the NAS restarts.

## Privileges and trusted files

The loader needs `SYS_MODULE`, which permits loading code into the host kernel;
container isolation does not make untrusted module code safe. Source, output and
startup scripts must be writable only by trusted administrators, not by ordinary
NAS shares or untrusted apps. A checksum detects accidental corruption; it does
not authenticate a module against someone who can replace its checksum too.

The loader and cooling app have no network access, read-only root filesystems,
`no-new-privileges`, and all capabilities dropped except the loader's
`SYS_MODULE`. Cooling has write access only to the QNAP device's sysfs subtree.
The builder has Internet access for compiler packages, read-only host headers and
source, and a writable module-output dataset. Review source and container image
updates before applying them. Driver kernel-API/compiler changes can still require
manual updates; this example provides no vendor support for third-party drivers.

## Tests

Run synthetic sensor, fan-fault and startup-order tests on a workstation:

```sh
python3 -m unittest discover -s examples/truenas -p 'test_*.py'
```

Compose validation tests additionally require the Docker Compose CLI (no daemon).
These tests cannot replace temperature and airflow checks on the actual hardware.
