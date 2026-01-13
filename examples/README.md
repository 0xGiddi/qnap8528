# QNAP8528 Example Scripts

**Note:** These scripts are provided as examples based on my personal use. They may not be fully tested and could contain bugs. Use them at your own risk. I hope they’re helpful, but I cannot take responsibility for any damage or data loss that may occur. 

---

### QNAP Disk LED Activity Monitor

A Bash script to monitor disk I/O on QNAP devices and blink the corresponding drive LEDs in real time—for LEDs that do not blink automatically. Supports both standard sysfs block stats and `blktrace` for catching all disk activity, including small operations from SMART/drivetemp. 

The script is structured in a way that it should not spawn many processes and essentially none monitoring loop to not spam PIDs and to keep CPU use low (no busy loop). The unit file is configured to restart the process every 10 seconds, in case the qnap8528 module unloaded (for updates, debug, etc.) If you unload the module often, I suggest not using the qnap8528 autoload feature as this can load the module when unwanted. 

#### Features
- Map specific LEDs to disks using `/dev/disk/by-id/` (keeps mapping consistent across reboots).
- Monitor I/O via sysfs (`/sys/block/.../stat`) or `blktrace`.
- Configurable polling interval and LED idle timeout.
- Optional auto-loading of required kernel modules (`qnap8528` and `ledtrig-timer`).
- Can run as a systemd service with automatic restart.

#### Requirements
- Bash
- Mounted SysFS
- udev running for `/dev/disk/by-id/`
- `modprobe` for module auto-loading
- `blktrace` (if using `USE_BLKTRACE=1`)
- Standard Linux utilities: `readlink`, `basename`, `kill`

#### Installation:
1. Copy the script `qnap-disk-led-monitor.sh` to `/usr/local/bin` and make it executable
2. Create a configuration file at `/etc/qnap-disk-led-monitor.conf`
```ini
#Sample Config file:
AUTOLOAD_QAP8528_MODULE=1
AUTOLOAD_LEDTRIG_MODULE=1

# A note about timing: other scripts that use the EC also want to communicate with it 
# (fan control, status LED, USB LED, VPD) so keep times reasonable to not tie up the EC.
# (*technically* EC read/write can be blocking up to 5 seconds, but this is rarely the case,
# values of 0.3 and 500 have been used successfully)
#
# When using blktrace to catch all activity, a sensors program/module (such as drivetemp) may
# poll the disk every second, so a timing of 0.3 and 500 may be needed.

# Polling interval (seconds)
POLL_INTERVAL=0.5
# LED idle timeout (milliseconds)
IDLE_TIMEOUT_MS=1500
# LED brightness when ON
LED_ON_BRIGHTNESS=1
# Use blktrace instead of sysfs stats
USE_BLKTRACE=0

# Disk to LED mappings (use the disk, not the partition!)
m2ssd1:nvme-XXXXXXXX
hdd3:ata-XXXXXXXX
```
3. Create a systemd service at `/etc/systemd/system/qnap-disk-led-monitor.service`
```ini
[Unit]
Description=QNAP Disk LED Activity Monitor
After=local-fs.target

[Service]
Type=simple
ExecStart=/usr/local/bin/qnap-disk-led-monitor.sh
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```
4. Enable and start the service.

#### Configuration values:
|Configuration  |Description
|-|-|
AUTOLOAD_QAP8528_MODULE| 1 to auto-load qnap8528, 0 to skip
AUTOLOAD_LEDTRIG_MODULE |1 to auto-load ledtrig-timer, 0 to skip
POLL_INTERVAL	|Seconds between IO polls (default 0.5)
IDLE_TIMEOUT_MS	|Milliseconds to keep LED on after last activity (default 1500)
LED_ON_BRIGHTNESS	|LED brightness when ON (default 1)
USE_BLKTRACE	| 1 to use blktrace for detecting all IO
\<LED>:\<DISK>	|Map LED name to /dev/disk/by-id/ device (one per disk/LED)