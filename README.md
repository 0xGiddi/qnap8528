- [Overview](#overview)
  - [Supported features](#supported-features)
- [Installation Instructions](#installation-instructions)
  - [Install instructions using DKMS](#install-instructions-using-dkms)
  - [Installing on TrueNAS Scale](#installing-on-truenas-scale)
    - [Compiling using Kylian's Docker container](#compiling-using-kylians-docker-container)
    - [Install Procedure](#install-procedure)
    - [Uninstall procedure](#uninstall-procedure)
  - [Installing on fnOS (Feiniu OS / 飞牛 OS)](#installing-on-fnos-feiniu-os--飞牛-os)
  - [Autoload module on startup with Modprobe](#autoload-module-on-startup-with-modprobe)
  - [Autoload module on startup with Systemd](#autoload-module-on-startup-with-systemd)
  - [Removing the module](#removing-the-module)
- [How to use this module](#how-to-use-this-module)
  - [Module Parameter](#module-parameter)
  - [Vital Product Data (VPD) Entries](#vital-product-data-vpd-entries)
  - [EC Firmware version and CPLD version](#ec-firmware-version-and-cpld-version)
  - [Energy-using Products (EuP) and Power Recovery modes](#energy-using-products-eup-and-power-recovery-modes)
  - [Buttons and switches](#buttons-and-switches)
  - [Fan Reporting/Control and Temperature Sensors](#fan-reportingcontrol-and-temperature-sensors)
  - [System LEDs Control](#system-leds-control)
  - [Disk Slot LED Control](#disk-slot-led-control)
  - [Possible Future Feature](#possible-future-feature)
- [Supported Models](#supported-models)
- [Question and Answers](#question-and-answers)


## Overview

The qnap8528 project is a kernel module for exposing the ITE8528 embedded controller functionality on QNAP NAS devices via common kernel APIs and subsystems. The goal is to get as many QNAP devices that use the ITE8528 EC to be supported. This project has no affiliation with *QNAP Systems Inc.*.

### Supported features

✅ Reading EC firmware version and CPLD version\
✅ Changing AC power recovery mode\
✅ Changing EuP mode\
✅ Fan reporting and control via hwmon (see note)\
✅ Temperature reporting via hwmon (see note)\
✅ Copy/Reset/Chassis button inputs\
✅ System LED control (e.g. *status*, *usb*, *ident*, *jbod*)\
✅ Disk slot LED control  (see note)\
✅ Reading VPD entries (e.g device serial no.)

Sounds amazing right? below are instruction to install and use this module for yourself! If you install this module, please be kind and run a few tests to verify that all the features are working properly and report back either by creating a new issue or emailing me at `qnap8528 [AT] giddi.net`.

**Note**: QM2/expansion card are not supported currently (it seems that most of them are not controlled by the EC anyway), \
Waiting on hardware for testing, contributions and research comments are welcome (issue: https://github.com/0xGiddi/qnap8528/issues/6)

## Installation Instructions
Before installing, please check the *Supported Models* table and see that your device is supported by this module, if your device is not yet supported, please check seethe Q&A for more information. The following instructions have been tested on *Debian 12 x64*.

**Disclaimer:** This kernel module is provided as-is, without any warranty of functionality or fitness for a specific purpose. The developers of this kernel module accept no liability for any damage, data loss, or system instability resulting from its use, Use at your own risk .
### Install instructions using DKMS

> **_⚠️ TR-464/TR-464xx/TS-253D Users:_**  It seems the device uses an ENE EC which does not return the correct chip ID, `skip_hw_check` needs to be set to true (`insmod qnap8528.ko skip_hw_check=true`), also, add this at the end of `ExecStart` in the service unit file.

1. Download the latest release of the module from the [releases page](https://github.com/0xGiddi/tsx73a-ec/releases/latest) or clone the repository locally using `git clone https://github.com/0xgiddi/qnap8528.git`
2. Extract the zip/tarball using `unzip <file>`, `tar xzf <file>`
3. Enter the project directory `qnap8528`
4. Compile and install the module using with `make install`
5. Ensure the module is installed using `dkms status`

### Installing on TrueNAS Scale 

> **❗Important**: Updates to the TrueNAS OS will overwrite any changes made during the installation of this module, requiring the installation process to be repeated.

#### Compiling using Kylian's Docker container

Visit https://github.com/kylian-002/Truenas-qnap8528-module and follow the instructions to build the module.
This method has the benefit that it does not require a local shell and does not require enabling TrueNAS "Developer-Mode"
> Note: The repository and code linked above in this section are not controlled by me and should be verified independently.

#### Install Procedure
> **❗Important**: TrueNAS Scale is a highly restricted operating system that does not support modifications to the host OS environment. To add this module, you must enable **Developer Mode**, which allows installation of build tools and modification of the root filesystem to include the kernel module. However, enabling Developer Mode voids official support from iXsystems on their support platforms. For more information, refer to the [TrueNAS documentation](https://www.truenas.com/docs/scale/scaletutorials/systemsettings/advanced/developermode/).  

1. Connect to TrueNAS either by using the web console, SSH or the local Linux shell.
2. using `sudo install-dev-tools` disable the read protection on the root filesystem and install required tools.
3. Download the latest source tarball from the [release page](https://github.com/0xGiddi/tsx73a-ec/releases/latest) and extract using `tar xzf <filename>`.
4. Enter the `src` directory of the project `cd qnap8528-<version>/src`.
5. Run `make`, **without `sudo`** (as `truenas_admin` or `root` if using local console)
6. Check that the module compiled successfully with `echo $?` (should be `0`) and that the `qnap8528.ko` was created.
7. Copy the kernel module to the Linux modules directory `sudo cp qnap8528.ko /lib/modules/$(uname -r)/extra`
8. Run `sudo depmod -a` to updated the modules database.
9. Module is installed and can be probed, follow [autoload-module-on-startup-with-systemd](#autoload-module-on-startup-with-systemd) to autoload on boot or manually probe using `modprobe qnap8528`.

#### Uninstall procedure
1. Unload the module using `modprobe -r qnap8528` or stopping the service created in previous step 9.
2. Delete the module file `rm /lib/modules/$(uname -r)/extra/qnap8528.ko`
3. Update modules database with `depmod -a`

### Installing on fnOS (Feiniu OS / 飞牛 OS)
**fnOS** is a lightweight operating system developed in China for small and embedded systems.  
Because it is not possible to install the toolchain directly on the host machine, a Docker environment can be used to compile the module.  
For more details on how to set this up (in chinese), see the repository by [gzxiexl](https://github.com/gzxiexl/qnap8528/).
> Note: FnOS, the repository and code linked above in this section are not controlled by me and should be verified independently.

### Autoload module on startup with Modprobe
1. Create the auto load file and content with `echo qnap8528 | sudo tee /etc/modules-load.d/qnap8528.conf`
2. If load options are required, add them in an options file under `/etc/modprobe.d/qnap8528.conf` (example: `options qnap8528 skip_hw_check=true
`)

### Autoload module on startup with Systemd
1. Create a new unit file `touch /etc/systemd/system/qnap8528-load-module.service`
2. Open the file in your favorite editor and add the following:
```ini
[Unit]
Description=Load qnap8528 EC kernel module

[Service]
Type=oneshot
RemainAfterExit=yes
# Add skip_hw_check=true at the end of the following line if required (such as on TS-464)
ExecStart=/sbin/modprobe qnap8528
ExecStop=/sbin/modprobe -r qnap8528

[Install]
WantedBy=multi-user.target
```
3. Enable the service `systemctl enable --now qnap8528-load-module.service`
4. Check that module loaded properly `lsmod | grep qnap8528`

If module is not loaded, try `systemctl daemon-reload` and `systemctl start qnap8528-load-module.service`

### Removing the module
1. Stop and disable the service with `systemctl disable --now qnap8528-load-module.service`
2. Delete the service unit file `/etc/systemd/system/qnap8528-load-module.service`
3. use `make uninstall` to uninstall the kernel module
4. Verify with `dkms status` that the module is no longer installed

## How to use this module

### Module Parameter
The module currently has 3 configuration parameters that can be set at load time:

`skip_hw_check`:\
Set to `false` by default, this prevents the module checking to see if a valid *IT8528* exists, the check is put there so that the module does not interact with an unknown device I/O ports more than it needs in the case that the module is loaded on the wrong machine. This parameter should not be set under normal conditions and is used mostly for debugging.

`blink_sw_only`:\
Set to `false` by default, this prevents registering the hardware blink callbacks on the device so all blinking is done via software. In most cases this is not needed and is a remnant of testing the LED subsystem, however, since the LED bilking logic is a mess and some edge cases may exist, this parameter exists to prevent letting the EC blink the LEDs. The `ident` LED is an exception since its behavior is only to blink.

`preserve_leds`:\
Set to `true` by default, this prevents the LED subsystem from turning of the LEDS when the module is unloaded. This is useful to keep enabled so that information can be conveyed by the LEDs even when then module is not loaded (for example, when shutting down, you might want the status LED to be a specific color and stay that way until the device has turned off).

The pseudo-LED `panel_brightneess` which controls the brightness of all the LEDs is not affected by this parameter and always preserves its value on unloading the module.

### Vital Product Data (VPD) Entries
The VPD entries provide information about the device, known VPD entries can be read under `/sys/devices/platform/qnap8528/vpd`, the  following VPD entries are supported:

 - `backplane_date`
 - `backplane_manufacturer`
 - `backplane_model` - Used for locating device config
 - `backplane_name`
 - `backplane_serial`
 - `backplane_vendor`
 - `enclosure_nickname`
 - `enclosure_serial` - This is the SN on the external sticker
 - `mainboard_date`
 - `mainboard_manufacturer`
 - `mainboard_model` - Used for locating device config
 - `mainboard_name`
 - `mainboard_serial`
 - `mainboard_vendor`

### EC Firmware version and CPLD version
Located under `/sys/devices/platform/qnap8528/ec`:
 - `fw_version` - Returns the EC firmware version
 - `cpld_version` - Returns the CPLD firmware

### Energy-using Products (EuP) and Power Recovery modes
*Not avaiable on all devices - configuration dependant*
Located under `/sys/devices/platform/qnap8528/ec`, allows to control features such as Wake-on-LAN nd power recovery. When EuP mode is set to ON, Wake-on-LAN and auto power recovery is not available.

- `eup_mode` - Returns the current EuP mode on read, sets on write.
- `power_recovery` - Returns the power recovery mode on read, sets on write.

Valid Values for EuP mode:
|Value| Mode
|-|-|
|0| EuP mode is OFF (WOL/Power-Recovery available)
|1| EuP mode is ON (WOL/Power-Recovery unavailable)

Valid Values for power recovery options:
|Value| Mode
|-|-|
|0| Keep device powered down
|1| Power device back up
|2| Enter last power state

### Buttons and switches
Buttons and switches are represented as an input device. They report as `EV_KEY` events and can be read with standard software and libraries (such as `evtest`). The input device is always registered, physical buttons are dependant on QNAP device model.

 The following buttons are supported:
 |Name|Event Code| Description|
 |-|-|-
 |Chassis Open|`BTN_0`| Chassis open switch
 |Reset|`BTN_1`| Reset button
 |USB Copy|`BTN_2`| Front panel USB copy button

### Fan Reporting/Control and Temperature Sensors
The fan and temperature sensors are exposed using the hwmon subsystem.

The temperature sensors are enumerated at the module load time and are not part of the device configuration, a valid temperature sensor is a sensor with a values of between (not including) `0` and `128` (So  don't keep the NAS in the freezer when loading the module). From the research, this is the mapping between temperature sensors and their number (the exact location of the sensor depends on model and no mapping exists):

|Temp Sensor Index|QNAP "Region"
|-|-
|`temp0`, `temp1`| CPU sensors (note 1)|
|`temp5` - `temp7`| System sensors
|`temp10`, `temp11`| Redundant power sensor
|`temp15` - `temp8`| Environment sensor

Note 1: I'm not sure if this is the actual CPU temperatures on system what DO NOT have a "proper" processor (an Intel or AMD processor) as I do not own such as device, on units that have a "proper" processor, the CPU temperature unit is set to `DTS` and not `EC` and the sensor reports a value of unknown origin.

Fans inputs are registered according to device config and report their speed in RPM (under `/sys/class/hwmon/hwmonX/fanY_input`), fan control is done via PWM channels (under `/sys/class/hwmon/hwmonX/pwmX`),

An unimportant side-note: The EC controls fans in groups (multiple fans will be effected by a change to a single PWM channel), there are 4 PWM channels and for each group, if a fan exists in that group, the PWM group will have the same index as the first fan that exists in that group, so for fan #7 (which is in the second group that controls fans #6 and #7) a pwm channel `pwm7` will be created if fan #6 does not exist.:

|PWM Group | Fans Controlled
|-|-
Group 1|Fans 0-5
Group 2|Fans 6, 7
Group 3|Fans 20-25
Group 4|Fans 30-35

**Why fans are no enumerated at runtime:** \
I experimented with enumerating the fans at runtime by using a combination of checking the fan status in the fan status EC register, checking that the PWM values are between `0` and `255` and that the reported RPM is not a junk value (such as `65535` on my device), however, the RPM check is not enough and the junk value is not always a MAX_SHORT or something nice that can be detected. This method of enumeration also extends the module load. If this feature is requested it will not be hard to add, please create an issue requesting it.

### System LEDs Control

System LEDs can be controlled via the standard Linux LED subsystem. For LEDs that have more than a single color (e.g. the *Status* LED), the brightness value dictates the color of the LED, so setting *Status* to `0` will turn it off, setting it to `1` will set it green and `2` would turn it red.

To blink the LEDs, use standard Linux Kernel LED triggers such as the `ledtrig_trimer`, `ledtrig_oneshot` and so on. If the blink rate chosen (`delay_on`/`delay_off`) is close to whats possible with hardware blink +-25% the module will ignore the exact rate provided and will use the hardware to blink the LEDs.  If blink timing is important (the HW blink is only +- 200ms at most off requested value) set `blink_sw_only=true` when loading the module to disable hardware acceleration. 

If an LED has multiple color, the blink color can be controlled by setting the correct brightness value either before or while blinking. To stop the blinking, a value of `0` needs to be written as the brightness value.

Following is a list of supported leds (without the `qnap8528::` prefix):
|Name|Device Name|Brightness Values|Notes
|-|-|-|-
|Status|`status`|`0`,`1`,`2` for Off, On Green, On Red| Support blink, see note below
|USB|`usb`|`0`,`1` for Off, On (Blue)| Support blink
|Enclosure Identification|`ident`|`0`,`1` for Off, On (Flashing Green/Red) | Overrides all disk LEDs until disabled
|JBOD Expansion|`jbod`|`0`,`1` for Off, On (Orange?/Blue)|
|10GbE Expansion|`10GbE`|`0`,`1` for Off, On (Green?)|
|Panel Brightness|`panel_brightness`|`0`-`100` for Off, Max Brightness| Affects all LEDs

***Status LED Note***\
The status LED is a special case where it can blink red and green in an alternating pattern, a special attribute `blink_bicolor` is created (if hardware blink was not disabled) which when written to with any value (except an empty string) will set the the *Status* LEDs into the green-red blinking pattern. To disable this, set the brightness of the LED to anything else.

**Why not use separate LED devices for each color?**\
Since the LEDs are either ON or OFF and colors cannot be mixed, a design choice was made that each LED is represented by a single kernel LED device, this not just cuts down on the number of LED entries created in the `/sys/class/leds` directory (especially when having 2 LEDs per disk), but it also makes it easier to keep track of the states of two connected LEDs more manageable.

### Disk Slot LED Control

The disk slot LEDs are controlled in a similar way to the system *Status* LED, where the brightness dictates the color (if multiple colors are available). The slot LEDs are named according to their names in the QNAP device documentation and slot numbers are preserved matching the physical disk slot they belong to. The only name change is that when a disk slot that QNAP names with a generic *Disk X*, the module name `hddX` to denote its type. The following table shows example slot names:

|QNAP Name|Module LED Name
|-|-
|Disk X| `hddX`
|SSD X| `ssdX`
|M.2 SSD X| `m2ssdX`
|U.2 SSD X| `u2ssdX`

Apart from that, there are a few other notes about disk slot LEDs for various device models:

**Disk slots don't always have LEDs**\
Some of the LEDs for disk slots are controlled by other hardware, and if so they are not exposed my this module (e.g. the *TES-1885U* only has EC LED control for 6/18 of the disks, the remaining 12 are controlled by another device). Or, the slot LEDs may simply not exist such as for some internal M.2 drives (e.g. *TS-855EU* which as 2 internal M.2 disks). Please check the *Disk LEDs* column in the supported models table to check how many disk LEDs are available.

**Some devices do not have bi-color LEDs**\
Some devices, such as the *TS-873AEU* do not have a green LEDs for the disks (that are controlled by the EC) only a red LEDs. In such a case (and vice-versa, if no red LED exists, only green), the brightness value written to the disk slot LED would turn on/blink the LED that does exist, so if no green LED exists but the value `1` is written (and not `2`), the red LED will illuminate instead.

**Disk slot drive activity blinking**\
Some devices (such as the *TS-473A*) blink the green disk slot LED to indicate the disk is being accessed, however, due to the architecture of the backplane, this "works out-of-the-box" behavior only works for some of the disk drives (on my NAS, disks 1 and 2, and disks 3 and 4 stay static green), unfortunately there is no known way (to me, currently) to disable this blinking if it's unwanted, the blinking will happen no matter the value set to activity blink register in the EC. However, the activity blinking is effected by turning the green LED off completely.


### Possible Future Feature
- Adding SATA disk power control for hotswapping disks

## Supported Models

The following table lists all devices that have a valid configuration in the module and should
work out-of-the-box. If you cannot find your model here, it either does not yet have a configuration entry
or does not use the IT8528 chip. Please check the Q&A for more information. 

Note that some models that have "extra letters" these extra letters are not listed in this list
under the model name column if they are not reflected in the configuration under `MODEL_NAME` or `DISPLAY_FIXED_MODEL_NAME`,
the driver may still work fine detecting it as the base model without the extra letters. 

|Model Name|MB Code|BP Code|Disk LEDs|Notes
|-|-|-|-|-|
|TVS-874|B6490|Q0AA0|10/10 |
|TVS-674|B6490|Q0BK0|8/8 |
|TVS-H874T|B6491|Q0AA0|10/10 |
|TVS-H674T|B6491|Q0BK0|8/8 |
|TVS-H874X|B6492|Q0AA0|10/10 |
|TVS-H674X|B6492|Q0BK0|8/8 |
|TS-983XU|Q00I1|Q00X0|9/9 |
|TS-2888X|Q00Q0|Q00S0|28/28 | ❗3rd code `Q00R0`, See *3
|TS-2483XU|Q00V1|Q00W0|24/24 |
|TVS-872XT|Q0120|Q0160|10/10 |
|TVS-672XT|Q0120|Q0170|8/8 |
|TVS-472XT|Q0120|Q0180|6/6 |
|TVS-872X|Q0121|Q0160|10/10 |
|TVS-672X|Q0121|Q0170|8/8 |
|TS-2490FU|Q03X0|Q04K0|24/24 |
|TNS-1083X|Q0410|Q0490|10/12 | ⚠️ See *1 ❗3rd code `Q04U0`, See *3
|TNS-C1083X|Q0411|Q0490|10/12 | ⚠️ See *1 ❗3rd code `Q04U0`, See *3
|TVS-872N|Q0420|Q0160|10/10 |
|TVS-672N|Q0420|Q0170|8/8 |
|TVS-472X|Q0420|Q0180|6/6 |
|TS-1886XU|Q0470|Q04L0|18/18 |
|TS-1673AU-RP|Q0520|Q0580|16/16 |
|TS-1273AU|Q0520|Q05G0|12/12 |
|TS-873AU|Q0520|Q05G1|8/8 |
|TS-1273AU-RP|Q0520|Q0670|12/12 |
|TS-873AU-RP|Q0520|Q0671|8/8 |
|TDS-2489FU|Q0530|Q0590|24/26 | ⚠️ See *1
|TDS-2489FU R2|Q0531|Q0590|24/26 | ⚠️ See *1
|TS-886|Q05S0|Q0650|10/10 |
|TS-686|Q05S0|Q0660|8/8 |
|TS-886|Q05S1|Q0650|10/10 |
|TS-686|Q05S1|Q0660|8/8 |
|TVS-1688X|Q05T0|Q0630|18/18 |
|TVS-1288X|Q05W0|Q05K0|14/14 |
|TS-3088XU|Q06X0|Q06Y0|30/30 |
|TS-973AX|Q0711|Q0760|9/9 |
|TS-873A|Q07D0|Q07L0|10/10 |✅ Tested, Thank you @calypso
|TS-673A|Q07D0|Q07M0|8/8 |🟩 Similar to TS-473A
|TS-473A|Q07D0|Q07N0|6/6 |  ✅ Tested, by me. 
|TS-1655|Q07Z1|Q08G0|18/18 |
|TS-2287XU|Q0840|Q08A0|22/22 |
|TS-1887XU|Q0840|Q0950|18/18 |
|TVS-675|Q08B0|Q0890|8/8 |
|TS-3087XU|Q08H0|Q08Z0|30/30 |
|TS-1290FX|Q09A0|Q09C0|12/12 |
|TS-1090FU|Q09B0|Q09I0|10/10 |
|TS-873AEU|Q0AK0|Q0AO0|8/10 | ⚠️ See *1, See *2
|TS-873AEU-RP|Q0AK0|Q0AO1|8/10 | ⚠️ See *1, See *2
|TS-1886XU R2|Q0B50|Q0950|18/18 |
|TVS-474|Q0BB0|Q0BL0|6/6 |
|TS-855EU|Q0BT0|Q0BU0|8/10 | ⚠️ See *1
|TS-655X|Q0CH0|Q0CI0|8/8 |
|TS-855X|Q0CH0|Q0CJ0|10/10 |
|TES-1885U|QX540|QY270|6/18 | ⚠️ See *1
|TES-3085U|QX541|QY510|6/30 | ⚠️ See *1
|TS-1685|QY380|QY390|22/22 |
|TES-1685-SAS|QY380|QY390|22/22 |
|TS-977XU|QZ480|Q0060|9/9 |
|TS-1277XU|QZ490|QZ550|12/12 |
|TS-877XU|QZ490|QZ551|8/8 |
|TS-1677XU|QZ491|QZ540|16/16 |
|TS-2477XU|QZ500|Q0070|24/24 |
|TS-1683XU|QZ601|Q0040|16/16 |
|TS-1283XU|QZ601|Q00M0|12/12 |
|TS-883XU|QZ601|Q00M1|8/8 |
|TVS-875U|SAP00|SBO60|8/10 | ⚠️ See *1
|TVS-1275U|SAP00|SBO70|12/14 | ⚠️ See *1
|TVS-1675U|SAP00|SBO80|16/18 | ⚠️ See *1
|TS-464|Q07R1|Q08F0|4/6 |✅ Tested, thank you @adyanth; Requires `skip_hw_check` ⚠️ See *1 See *2 
|TS-464U|Q08S0|QY740|4/4 |Might require `skip_hw_check` ⚠️ See *2
|TS-464T4|Q0910|Q08F0|6/6 |Might require `skip_hw_check` ⚠️ See *2
|TS-464C|SAQ93|SBR00|6/6 |Might require `skip_hw_check` ⚠️ See *2
|TS-464C2|SAQ95|SBR00|6/6 |Might require `skip_hw_check` ⚠️ See *2
TS-1264U|Q08R0|Q08X0|12/12 | ⚠️ See *2
|TS-464U|Q08S0|QY740|4/4 | ⚠️ See *2
|TS-464EU|Q0930|*N/A*|6/6 |✅ Tested, thank you @L3nny5; Requires `skip_hw_check` ⚠️ See *2
|TS-864EU|Q0AN0|Q0AS0|8/8 | ⚠️ See *2
|TS-451D2|SAO51|SBK41|4/4 |  
|TS-451D|SAO50|SBK40|4/4 |  
|TS-453DMINI|SAO40|*N/A*|4/4 | ⚠️ See 2 
|TBS-453DX|QZ530|*N/A*|4/4 | ⚠️ See 2 
|HS-453DX|QZ290|*N/A*|4/4 | ⚠️ See 2 
|TS-ALTO4|Q05X0|Q06A0|2/2 |  
|TS-253D|Q05H0|QY570|2/2 |  ✅ Tested, thank you @Atomique13,@nader-eloshaiker; Requires `skip_hw_check`
|TS-251D|Q04W1|QY570|2/2 |  
|TS-653D|Q04O0|Q04N0|6/6 |  
|TS-453D|Q04M0|QY581|4/4 | ⚠️ See 2
|TVS-951N|SAN30|SBN10|9/9|
|TVS-951X|Q0090|Q00A0|9/9|✅ Tested
|TS-451A|QX690|QX700|4/4 | ✅ Tested; thank you @kikakeule
|TS-251A|QX690|QX710|2/2 |
|TS-453BMINI|QY600|*N/A*|4/4 |✅ Tested; thank you @gzxiexl ⚠️ See 2
|TS-453B|QZ060|QZ070|4/4 | ⚠️ See 2
|TS-253BE|QY472|QY570|2/2 |
|TS-453BE|QY472|QY580|4/4 | ⚠️ See 2
|TS-653B|QY471|QY590|6/6 | ⚠️ See 2
|TS-253B|QY470|QY570|2/2 |
|TS-1277XU|QZ490|QZ550|12/12 |
|TS-1277XU|QZ492|QZ550|12/12 |
|TS-1277XU|QZ492|QZ552|12/12 |
|TS-1277XU|QZ493|QZ550|12/12 |
|TS-1277XU|QZ494|QZ550|12/12 |
|TS-1677XU|QZ491|QZ540|16/16 |
|TS-1677XU|QZ492|QZ540|16/16 |
|TS-1677XU|QZ494|QZ540|16/16 |
|TS-2477XU|QZ500|Q0070|24/24 |
|TS-2477XU|QZ502|Q0070|24/24 |
|TS-2477XU|QZ503|Q0070|24/24 |
|TS-2477XU|QZ504|Q0070|24/24 |
|TS-877XU|QZ490|QZ551|8/8 |
|TS-877XU|QZ490|QZ560|8/8 |
|TS-877XU|QZ492|QZ551|8/8 |
|TS-877XU|QZ493|QZ551|8/8 |
|TS-877XU|QZ494|QZ551|8/8 |
|TS-977XU|QZ480|Q0060|9/9 |
|TS-977XU|QZ481|Q0060|9/9 |
|TS-977XU|QZ482|Q0060|9/9 |
|TBS-464|Q07X0|N/A|4/4 |
|TS-664|Q07R2|Q07S0|6/8 | ⚠️ See *1 ⚠️ See 2
|TS-664|Q07R7|Q07S0|6/8 | ⚠️ See *1 ⚠️ See 2
|TS-564|SAQ10|SBP90|5/5 | ⚠️ See 2
|TS-464|Q07R6|Q08F0|4/6 | ⚠️ See *1 ⚠️ See 2
|TS-462C|SAQ92|SBR00|4/4 | ⚠️ See 2
|TS-462|Q07R4|Q08F0|4/6 | ⚠️ See *1 ⚠️ See 2
|TS-364|Q08E0|Q08T0|5/5 |
|TS-264C|SAQ91|SBQ10|4/4 |
|TS-264|Q07R5|Q09N0|2/4 | ⚠️ See *1
|TS-264|Q07R0|Q09N0|2/4 | ⚠️ See *1
|TS-262C|SAQ90|SBQ10|2/2 |
|TS-262|Q07R3|Q09N0|2/4 | ⚠️ See *1
|TS-453A|QX850|QX670|4/4 | ⚠️ See *4
|TS-853A|QX470|QX520|8/8 |
|TS-253A|QX960|QY010|2/2 |
|TBS-453A|QX910|N/A|4/4 |
|TS-453MINI II|QX580|N/A|4/4 |
|TS-653A|QX470|QX660|6/6 |
|TS-453A|QX471|QX670|4/4 |
|TS-553AS|QX370|N/A|5/5 |
|TVS-473e|QY030|QY040|6/6 | ⚠️ See *5
|TVS-873e|QY030|QY060|10/10 |
|TVS-673e|QY030|QY050|8/8 |
|TS-473|QY032|QY040|6/6 |  
|TS-873|QY031|QY060|10/10 |  
|TS-673|QY032|QY050|8/8 |  
|TS-673|QY031|QY050|8/8 |  
|TS-473|QY031|QY040|6/6 |


*1 Some or all disks LEDs are managed by other hardware (not the EC), if the model is missing 2 disks (e.g `8/10`), it's most likely the internal M.2/NVME ports that do not have an LED associated with them.\
*2 Some or all of the disks do not have a present or error (green/red) LED.\
*3 This device config file contains a 3rd code number which is not checked or tested. Might hint at use of VPD table 3 and 4? \
*4 Device serial number can be either located in MB VPD or network card. \
*5 This model has 4 different configuration options (4/6 disks, 1/2 fans, partial disk blinking, CPU temp reported by EC or DTS and serial number reported from NET and VPD) due to them all having the same hardware IDs, the config in qnap8528 module supports 6 disks, 2 fans and no blinking of disk LEDs. Note that of hardware that does not have support for the 2 extra M.2 and only have a single fan there will be pseudo LEDs and a fan created. 

## Question and Answers

**Q.** **I have a device that has not been tested yet, how can I be sure that everything is working properly?**\
**A.** *Install the without the `systemd` service and check the functionality, read VPD entries, check the power recovery and EuP modes (if applicable), test setting the different LEDs and check that hwmon information seems correct, you might want to keep `dmesg` up and check that there are no errors reported in the process, then, let us know via an issue if you have any problems or that everything seems OK and it has been tested.*

**Q.** **What environment was this module developed in?**\
**A.** *This module was developed and tested primarily on Debian 12 (bookworm), Linux kernel version `6.1.0-18` on the amd64 based TS-473A NAS device, however the module was also compiled successfully under Arch Linux and Manjaro*.

**Q.** **Can I get a `.deb`, `.rpm` (or other package format) file?**\
**A.** *Hopefully Soon! I am working (as time permits) on creating packages for different NAS OS flavours (where possible)*

**Q.** **I don't see my QNAP model in the table of supported devices, what to do?**\
**A.** *Either the device does not use the IT8528 chip or I missed it when generating the config, feel free to open a new issue and report this with as much information about the model.*

**Q.** **What is this `MB Code` and `BP Code`?**\
**A.** *These are the model codes stored in the devices VPD tables, an `MB` code is the product code of the mainboard (motherboard) and the `BP` code is for the backplane. These codes are used to locate the correct configuration for the device*, some devices might share a code, such as the `TS-X73A` family that share the mainboard code of `Q07D0` but have different backplane codes depending on the number of disks, the codes in the VPD tables are actually longer, and contain a revision number, but that does not seem to change the configuration.

**Q.** **I have loaded the module but the fans speeds don't change with temperature rise/fall, what's wrong?**\
**A.** *This module does not decide how to control the fans, it only exposes the fan controls and reporting for third part scripts*

**Q.** **I have loaded the module but the disk LEDs are not blinking when disk activity is happening, what's wrong?**\
**A.** *This module does not blink the disk activity LEDs when disks are accessed, this was a possible feature, however no proper way was found to link a physical disk slot to a disk device, so this was left for the user to deice how to do it, I personally was thinking of maybe using System Taps or similar to trigger a oneshot event, in addition, changes made to the Linux kernel make it hard to hook the disk block-IO interface without inviting instability to the system*

**Q.** **There seem to be things missing (a fan, an HDD led, etc...) why?**\
**A.** *It's most likely that that specific sensor/fan/LED is not managed by the EC, if you think this is a configuration error, report it.*

**Q.** **I need/want feature XYZ, can you please add support for it?**\
**A.** *Depends on the feature, if it's part of the IT8528 EC responsibility and an API to reverse engineer is available, we can try, create an Issue contain all information.*
