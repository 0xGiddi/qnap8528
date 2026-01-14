# QNAP8528 Example Scripts

**Note:** These scripts are provided as examples based on my personal use. They may not be fully tested and could (and probably do) contain bugs. Use them at your own risk. I hope they’re helpful, but I cannot take responsibility for any damage or data loss that may occur. 

---

### QNAP Disk LED Activity Monitor - qnap-disk-led-monitor.sh

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

### QNAP Status LED coordinator
A bash script to set and coordinate the use of the status LED between programs. It uses a priority based system where
each script can set the LED according to it's own status and the script will handle setting the LED to the highest priority state.

#### Features
- Automatic LED color/state priority 
- Multi-program safe
- Warning/Error states survive reboot (if program is run on boot)

#### Requirements
- Bash
- Mounted SysFS
- Standard Linux utilities: `mkdir`, `touch`, `flock` etc.


#### Installation
1. Copy the script `qnap-set-status.sh` to `/usr/local/bin` and make it executable
2. Create boot script to set status to OK on boot under `/etc/systemd/system/qnap-set-status.service`
```ini
# NOTE: This will cause the qnap8528 module to autoload as it loaded 
# in the script.
[Unit]
Description=Set QNAP Status LED after boot

# Mandatory: LED module must be loaded
After=multi-user.target

# Optional ordering: run after these if they exist
# This is useful if using qnap8528-grub-status to only set the LED
# after boot is complete and critical services are running.
# For example, the disk monitoring script from above.
#
# After=qnap-disk-led-monitor.service

[Service]
Type=oneshot
# No need to set a caller ID
ExecStart=/usr/local/bin/qnap-set-status.sh ok
RemainAfterExit=yes
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
3. Enable and start the service
   
#### Usage 
The script takes two parameters, the first is a state and the second is a caller ID.
The state parameter sets the LED to reflect a system status state and the caller ID allows
for separation between scripts. 

The following state commands exist, (the use cases are examples, not implemented in this script):
|Priority (Higher = More important)|State|LED Effect|Usecase Example|
|-|-|-|-|
|1|ok|GREEN - Solid|Normal operation|
|2|activity|GREEN - Blinking|Operation in progress (a backup, ZFS scrub, system update)|
|3|transition|Both GREEN and RED - Bilking Alternately |Boot/Shutdown in progress *See [GRUB boot |indicator](https://github.com/0xGiddi/qnap8528-grub-status)|
|4 (persists after reboot)|warning|RED - Blinking|High temps, On battery power, High CPU load, Network redundancy in |effect|
|5 (persists after reboot)|error|RED - Solid|Backup failure, Fan failure, Network failure, Overheat, Recovered from |unexpected shutdown|
|-|clear|Clear all states from all callers and turns LED off|Reset all states|


**Example 1** - Set the default state to show normal operation:
`qnap-set-status.sh ok`

**Example 2** - Set the state with a caller ID:
```bash
# Set indicator to indicate activity with caller ID of my-backup-script
qnap-set-status.sh activity my-backup-script
# Set default state to "ok"
# Will NOT disable blinking, as "activity" still set by my-backup-script takes priority
qnap-set-status.sh ok 
# my-backup-script releases the "activity" state and goes back to "ok"
# LED will go back to "ok" (if no other caller ID has set a higher priority state)
qnap-set-status.sh ok my-backup-script
```

**Example 3** - Set the state with a caller ID and downgrade
```bash
# Set indicator to indicate activity with caller ID of my-backup-script
qnap-set-status.sh activity my-backup-script
# Fan monitor script sets a warning state
# This will override the backup script activity state
qnap-set-status.sh warning fan-monitor-script
# Fan monitor script clear warning state, since backup script has not
# released yet, the LED will downgrade to "activity" and not "ok"
qnap-set-status.sh ok fan-monitor-script
# my-backup-script releases the "activity" state and goes back to "ok"
# LED will go back to "ok" (if no other caller ID has set a higher priority state)
qnap-set-status.sh ok my-backup-script
```

**Example 4** - Error state with reboot (and systemd unit installed)
```bash
# Fan monitor script sets a warning state
qnap-set-status.sh warning fan-monitor-script
# Backup script set indicator to indicate activity (no change, still blinks warning)
qnap-set-status.sh activity my-backup-script
#
# SYSTEM REBOOTS:
# 1. LED starts off solid green until BIOS passes control to GRUB
# 2. If GRUB has the qnap8528led.mod installed and loaded 
#    LED will now change to blink green/red alternate pattern
#    If not, it will stay solid green
# 3. Systemd `qnap-set-status.service` service restores the warning state from fanmonitor script
#     activity status from the backup script was lost due to reboot
# 4. To clear the warning either use:
#           qnap-set-status.sh ok fan-monitor-script
#       OR  qnap-set-status.sh reset && qnap-set-status.sh ok
```

**Example 5** - Set an indicator for shutdown process
Create a service file `/etc/system/systemd/qnap-set-status-shutdown.service` and enable it with the content:
```init
[Unit]
Description=Set QNAP Status LED at shutdown (transition/activity)
DefaultDependencies=no
Before=shutdown.target reboot.target halt.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/qnap-set-status.sh transition
RemainAfterExit=yes

[Install]
WantedBy=halt.target reboot.target shutdown.target
```
This will set the status LED to blink red and green in an alternating pattern when system goes down for a reboot or shutdown (similar to how the GRUB module sets the pattern on boot). It will not override a warning/error LED state.  