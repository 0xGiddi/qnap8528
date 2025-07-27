// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux kernel module controlling QNAP specific hardware via IT8528 EC.
 *
 * Some QNAP services such as the TS-x73A series use a IT8528 EC with custom
 * firmware to control some hardware options and capabilities. This module
 * exposes the non standard EC interface as standard Linux interfaces
 *  to the userland.
 *
 * This driver supports the following QNAP IT8528 EC functions:
 *	- Read QNAP Vital Product Data
 *	- Set/Clear AC power recovery mode
 *	- Set/Clear Energy-using Products (EuP) mode
 *	- Expose EC FW version
 *	- Expose CPLD version
 *	- Read fan speeds in RPM
 *	- Read/write fan PWM
 *	- Read various temperature sensors
 *	- Read hardware buttons
 *	- Expose various LEDs
 *
 * Version history:
 *	v1.0-RC1: Initial RC, project renamed and rewritten.
 *	v1.0: Updated LED logic
 *	v1.1: Added config  TS464, TS-464C, TS-464C2, TS-464T4 and TS-464U
 *	v1.2: Extended delay and retry for EC read/write operations
 *	      Fixed erroneous check for slot activity support when setting ERROR off
 *  v1.3: Changed VPD entries to return an error if BP does not exist
 *		  Changed configuration search logic to support devices with MB code only
 *		  Added configuration for TS-464eU
 *  v1.9: Added TS-1277XU, TS-877XU, TS-1677XU, TS-2477XU, TS-977XU
 *  v1.10: Added more configs for TBS-464, TS-262, TS-262C, TS-264, 
 *		   TS-264C, TS-364, TS-462, TS-462C, TS-464, TS-564, TS-664
 */

#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include "qnap8528.h"

static bool qnap8528_skip_hw_check;
module_param_named(skip_hw_check, qnap8528_skip_hw_check, bool, 0);
MODULE_PARM_DESC(skip_hw_check, "Skip HW check for IT8528 device");

static bool qnap8528_blink_sw_only;
module_param_named(blink_sw_only, qnap8528_blink_sw_only, bool, 0);
MODULE_PARM_DESC(blink_sw_only, "Disable hardware blinking and only use software blinking (less quirky but ties up EC for fast blinking)");

static bool qnap8528_preserve_leds = true;
module_param_named(preserve_leds, qnap8528_preserve_leds, bool, 0);
MODULE_PARM_DESC(preserve_leds, "Preserve LED states on module unload (default on)");

static DEFINE_MUTEX(qnap8528_ec_lock);

static struct resource qnap8528_resources[] = {
	DEFINE_RES_IO_NAMED(QNAP8528_EC_CMD_PORT, 1, DRVNAME),
	DEFINE_RES_IO_NAMED(QNAP8528_EC_DAT_PORT, 1, DRVNAME),
};

static struct platform_device *qnap8528_pdevice;

static DEVICE_ATTR(fw_version, 0444, qnap8528_fw_version_attr_show, NULL);
static DEVICE_ATTR(cpld_version, 0444, qnap8528_cpld_version_attr_show, NULL);
static DEVICE_ATTR(power_recovery, 0644, qnap8528_power_recovery_attr_show, qnap8528_power_recovery_attr_store);
static DEVICE_ATTR(eup_mode, 0644, qnap8528_eup_mode_attr_show, qnap8528_eup_mode_attr_store);

static struct attribute *qnap8528_ec_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_cpld_version.attr,
	&dev_attr_power_recovery.attr,
	&dev_attr_eup_mode.attr,
	NULL
};

static const struct attribute_group qnap8528_ec_attr_group = {
	.name = "ec",
	.is_visible = qnap8528_ec_attr_check_visible,
	.attrs = qnap8528_ec_attrs
};

static QNAP8528_DEVICE_ATTR(enclosure_serial, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_ENC_SERIAL);
static QNAP8528_DEVICE_ATTR(enclosure_nickname, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_ENC_NICKNAME);
static QNAP8528_DEVICE_ATTR(mainboard_manufacturer, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_MANUF);
static QNAP8528_DEVICE_ATTR(mainboard_vendor, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_VENDOR);
static QNAP8528_DEVICE_ATTR(mainboard_name, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_NAME);
static QNAP8528_DEVICE_ATTR(mainboard_model, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_MODEL);
static QNAP8528_DEVICE_ATTR(mainboard_serial, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_SERIAL);
static QNAP8528_DEVICE_ATTR(mainboard_date, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_MB_DATE);
static QNAP8528_DEVICE_ATTR(backplane_manufacturer, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_MANUF);
static QNAP8528_DEVICE_ATTR(backplane_vendor, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_VENDOR);
static QNAP8528_DEVICE_ATTR(backplane_name, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_NAME);
static QNAP8528_DEVICE_ATTR(backplane_model, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_MODEL);
static QNAP8528_DEVICE_ATTR(backplane_serial, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_SERIAL);
static QNAP8528_DEVICE_ATTR(backplane_date, 0444, qnap8528_vpd_attr_show, NULL, QNAP8528_VPD_BP_DATE);

static struct attribute *qnap8528_vpd_attrs[] = {
	&dev_attr_enclosure_serial.attr,
	&dev_attr_enclosure_nickname.attr,
	&dev_attr_mainboard_manufacturer.attr,
	&dev_attr_mainboard_vendor.attr,
	&dev_attr_mainboard_name.attr,
	&dev_attr_mainboard_model.attr,
	&dev_attr_mainboard_serial.attr,
	&dev_attr_mainboard_date.attr,
	&dev_attr_backplane_manufacturer.attr,
	&dev_attr_backplane_vendor.attr,
	&dev_attr_backplane_name.attr,
	&dev_attr_backplane_model.attr,
	&dev_attr_backplane_serial.attr,
	&dev_attr_backplane_date.attr,
	NULL
};

static const struct attribute_group qnap8528_vpd_attr_group = {
	.name = "vpd",
	.attrs = qnap8528_vpd_attrs
};

static const struct attribute_group *qnap8528_pdriver_attr_groups[] = {
	&qnap8528_ec_attr_group,
	&qnap8528_vpd_attr_group,
	NULL
};

static struct platform_driver qnap8528_pdriver = {
	.driver = {
		.name = DRVNAME,
		.dev_groups = qnap8528_pdriver_attr_groups
	},
	.probe = qnap8528_probe
};

static u32 qnap8528_hwmon_temp_config[QNAP8528_HWMON_MAX_CHANNELS + 1] = {0};
static struct hwmon_channel_info qnap8528_hwmon_temp_chan_info = {
	.type = hwmon_temp,
	.config = qnap8528_hwmon_temp_config
};

static u32 qnap8528_hwmon_fan_config[QNAP8528_HWMON_MAX_CHANNELS + 1] = {0};
static struct hwmon_channel_info qnap8528_hwmon_fan_chan_info = {
	.type = hwmon_fan,
	.config = qnap8528_hwmon_fan_config
};

static u32 qnap8528_hwmon_pwm_config[QNAP8528_HWMON_MAX_CHANNELS + 1] = {0};
static struct hwmon_channel_info qnap8528_hwmon_pwm_chan_info = {
	.type = hwmon_pwm,
	.config = qnap8528_hwmon_pwm_config
};

static const struct hwmon_channel_info *qnap8528_hwmon_chan_info[] = {
	&qnap8528_hwmon_temp_chan_info,
	&qnap8528_hwmon_fan_chan_info,
	&qnap8528_hwmon_pwm_chan_info,
	NULL
};

static const struct hwmon_ops qnap8528_hwmon_ops = {
	.is_visible = qnap8528_hwmon_is_visible,
	.read = qnap8528_hwmon_read,
	.write = qnap8528_hwmon_write
};

static struct hwmon_chip_info qnap8528_hwmon_chip_info = {
	.ops = &qnap8528_hwmon_ops,
	.info = qnap8528_hwmon_chan_info
};

static DEVICE_ATTR_WO(blink_bicolor);

static int qnap8528_ec_hw_check(void)
{
	int ret = 0;
	u16 ec_id;

	if (!request_muxed_region(0x2e, 2, DRVNAME))
		return -EBUSY;

	outb(0x20, 0x2e);
	ec_id = inb(0x2f) << 8;
	outb(0x21, 0x2e);
	ec_id |= inb(0x2f);

	if (ec_id != QNAP8528_EC_CHIP_ID)
		ret = -ENODEV;

	release_region(0x2e, 2);

	if (ret)
		pr_err("Could not locate IT8528 EC device");
	else
		pr_info("IT8528 EC device found successfully");

	return ret;
}

static int qnap8528_ec_wait_ibf_clear(void)
{
	int retries = 0;

	do {
		if (!(inb(QNAP8528_EC_CMD_PORT) & BIT(1)))
			return 0;
		udelay(QNAP8528_EC_UDELAY);
	} while (retries++ < QNAP8528_EC_MAX_RETRY);
	return -EBUSY;
}

static int qnap8528_ec_clear_obf(void)
{
	int retries = 0;

	do {
		if (!(inb(QNAP8528_EC_CMD_PORT) & BIT(0)))
			return 0;
		inb(QNAP8528_EC_DAT_PORT);
		udelay(QNAP8528_EC_UDELAY);
	} while (retries++ < QNAP8528_EC_MAX_RETRY);
	return -EBUSY;
}

static int qnap8528_ec_wait_obf_set(void)
{
	int retries = 0;

	do {
		if (inb(QNAP8528_EC_CMD_PORT) & BIT(0))
			return 0;
		udelay(QNAP8528_EC_UDELAY);
	} while (retries++ < QNAP8528_EC_MAX_RETRY);
	return -EBUSY;
}

static int qnap8528_ec_send_command(u16 command)
{
	int ret;

	ret = qnap8528_ec_wait_ibf_clear();
	if (ret)
		goto ec_send_cmd_out;
	outb(0x88, QNAP8528_EC_CMD_PORT);

	ret = qnap8528_ec_wait_ibf_clear();
	if (ret)
		goto ec_send_cmd_out;
	outb((command >> 8) & 0xff, QNAP8528_EC_DAT_PORT);

	ret = qnap8528_ec_wait_ibf_clear();
	if (ret)
		goto ec_send_cmd_out;
	outb(command & 0xff, QNAP8528_EC_DAT_PORT);

ec_send_cmd_out:
	return ret;
}

static int qnap8528_ec_read(u16 command, u8 *data)
{
	int ret;

	mutex_lock(&qnap8528_ec_lock);
	ret = qnap8528_ec_clear_obf();
	if (ret)
		goto ec_read_out;

	ret = qnap8528_ec_send_command(command);
	if (ret)
		goto ec_read_out;

	ret = qnap8528_ec_wait_obf_set();
	if (ret)
		goto ec_read_out;

	*data = inb(QNAP8528_EC_DAT_PORT);


ec_read_out:
	mutex_unlock(&qnap8528_ec_lock);
	return ret;
}

static int qnap8528_ec_write(u16 command, u8 data)
{
	int ret;

	mutex_lock(&qnap8528_ec_lock);
	ret = qnap8528_ec_send_command(command | 0x8000);
	if (ret)
		goto qnap8528_ec_read_out;

	ret = qnap8528_ec_wait_ibf_clear();
	if (ret)
		goto qnap8528_ec_read_out;

	outb(data, QNAP8528_EC_DAT_PORT);

qnap8528_ec_read_out:
	mutex_unlock(&qnap8528_ec_lock);
	return ret;
}

static umode_t qnap8528_ec_attr_check_visible(struct kobject *kobj, struct attribute *attr, int n)
{
	struct qnap8528_dev_data *data = dev_get_drvdata(container_of(kobj, struct device, kobj));

	if ((!data->config->features.eup_mode) && (strcmp(attr->name, "eup_mode") == 0))
		return 0;

	if ((!data->config->features.pwr_recovery) && (strcmp(attr->name, "power_recovery") == 0))
		return 0;

	return attr->mode;
}

static ssize_t qnap8528_fw_version_attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 value;
	int i;
	ssize_t ret;
	int read = 0;

	for (i = 0; i <  QNAP8528_EC_FW_VER_LEN; i++) {
		ret = qnap8528_ec_read(QNAP8528_EC_FW_VER_REG + i, &value);
		if (ret)
			return ret;
		read += scnprintf(buf + i, PAGE_SIZE - i, "%c", value);
	}

	return read;
}

static ssize_t qnap8528_cpld_version_attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 value;
	int ret;

	ret = qnap8528_ec_read(QNAP8528_CPLD_VER_REG, &value);
	if (ret)
		return ret;
	return scnprintf(buf, PAGE_SIZE, "0x%x", value);
}

static ssize_t qnap8528_power_recovery_attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	u8 val;

	ret = qnap8528_ec_read(QNAP8528_PWR_RECOVERY_REG, &val);
	if (ret)
		return ret;
	return scnprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t qnap8528_power_recovery_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret)
		return ret;

	/* Values are: 0 - Off, 1 - On, 2 - Last State */
	if ((val < 0) || (val > 2))
		return -ERANGE;

	ret = qnap8528_ec_write(QNAP8528_PWR_RECOVERY_REG, val);
	if (ret)
		return ret;

	return count;
}

static ssize_t qnap8528_eup_mode_attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 ret, val;

	ret = qnap8528_ec_read(QNAP8528_EUP_SUPPORT_REG, &val);
	if (ret)
		return ret;

	if (!(val & 0x08))
		return -ENOTSUPP;

	ret = qnap8528_ec_read(QNAP8528_EUP_MODE_REG, &val);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%d", (val & 0x08) ? 1 : 0);
}

static ssize_t qnap8528_eup_mode_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val, tmp;
	int ret;

	ret = qnap8528_ec_read(QNAP8528_EUP_SUPPORT_REG, &val);
	if (ret)
		return ret;

	if (!(val & 0x08))
		return -ENOTSUPP;

	ret = kstrtou8(buf, 10, &val);
	if (ret)
		return ret;

	ret = qnap8528_ec_read(QNAP8528_EUP_MODE_REG, &tmp);
	if (ret)
		return ret;

	tmp &= 0xf7;
	tmp |= val ? 0x08 : 0x00;

	/* Values are: 0 - Off, 1 - On*/
	ret = qnap8528_ec_write(QNAP8528_EUP_MODE_REG, tmp);
	if (ret)
		return ret;

	return count;
}

static ssize_t qnap8528_vpd_attr_show(struct device *dev, struct qnap8528_device_attribute *attr, char *buf)
{
	u16 i, reg_a, reg_b, reg_c, offs;
	char raw[QNAP8528_VPD_ENTRY_MAX + 1] = {0};
	u32 entry = attr->vpd_entry;
	struct qnap8528_dev_data *data = NULL;
	
	if (dev)
		data = dev_get_drvdata(dev);

	if (data && (((entry >> 0x1a) & 3) == 1))
		if (!data->config->features.vpd_bp_table)
			return -ENOTSUPP;

	if (data && (entry == 0xdeadbeef)) {
		if (data->config->features.enc_serial_mb)
			entry = QNAP8528_VPD_ENC_SER_MB;
		else
			entry = QNAP8528_VPD_ENC_SER_BP;
	}
	switch ((entry >> 0x1a) & 3) {
	case 0:
		reg_a = 0x56;
		reg_b = 0x57;
		reg_c = 0x58;
		break;
	case 1:
		reg_a = 0x59;
		reg_b = 0x5a;
		reg_c = 0x5b;
		break;
	case 2:
		reg_a = 0x5c;
		reg_b = 0x5d;
		reg_c = 0x5e;
		break;
	case 3:
		reg_a = 0x60;
		reg_b = 0x61;
		reg_c = 0x62;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < ((entry >> 0x10) & 0xff); i++) {
		offs = (entry & 0xffff) + i;
		if (qnap8528_ec_write(reg_a, (offs >> 8) & 0xff) || qnap8528_ec_write(reg_b, offs & 0xff) || qnap8528_ec_read(reg_c, &raw[i]))
			return -EBUSY;
		udelay(5000);
	}

	return qnap8528_vpd_parse((entry >> 0x18) & 3, (entry >> 0x10) & 0xff, raw, buf);
}

static ssize_t qnap8528_vpd_parse(int type, int size, char *raw, char *buf)
{
	int ret = 0;
	u16 i, offs;
	time64_t ts, time_bytes = 0;

	switch (type) {
	case 0: /* String */
		strncpy(buf, raw, size);
		buf[size + 1] = '\x0';
		ret =  size + 1;
		break;
	case 1: /* Number */
		ret += scnprintf(buf, PAGE_SIZE, "0x");
		for (i = 0; i < size; i++) {
			offs = (i + 1) * 2;
			ret += scnprintf(buf + offs, PAGE_SIZE - offs, "%02x", raw[size - i - 1]);
		}
		break;
	case 2: /* Date */
		memcpy(&time_bytes, raw, size);
		ts = mktime64(2013, 1, 1, 0, 0, 0) + (time_bytes * 0x3c);
		ret += scnprintf(buf, PAGE_SIZE, "%ptTs", &ts);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t blink_bicolor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	if (count > 0)
		ret = qnap8528_ec_write(QNAP8528_LED_STATUS_REG, 5);
	return ret ? ret : count;
}

static int qnap8528_led_status_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct qnap8528_system_led *sled = container_of(cdev, struct qnap8528_system_led, cdev);

	if (cdev->flags & LED_UNREGISTERING) {
		if (!qnap8528_blink_sw_only)
			device_remove_file(cdev->dev, &dev_attr_blink_bicolor);

		if (qnap8528_preserve_leds)
			return 0;
	}

	if (brightness) {
		if(sled->is_hw_blink)
			qnap8528_ec_write(QNAP8528_LED_STATUS_REG, brightness  == 1 ? 3 : 4);
		else
			qnap8528_ec_write(QNAP8528_LED_STATUS_REG, brightness);
	} else {
		sled->is_hw_blink = false;
		qnap8528_ec_write(QNAP8528_LED_STATUS_REG, 0);
	}

	return 0;
}

static int qnap8528_led_status_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off)
{
	struct qnap8528_system_led *sled = container_of(cdev, struct qnap8528_system_led, cdev);

	/* HW blink acceptable range: measured blink rate for green and red is ~628ms on/off, tolerance of ~25% */
	if (!(*delay_on == 0 && *delay_off == 0) && (*delay_on < 470 || *delay_on > 790 || *delay_off < 470 || *delay_off > 790))
		return -EINVAL;
	
	sled->is_hw_blink = true;

	if (cdev->brightness == 2)
		qnap8528_ec_write(QNAP8528_LED_STATUS_REG, 4);
	else
		qnap8528_ec_write(QNAP8528_LED_STATUS_REG, 3);

	return 0;
}

static int qnap8528_led_usb_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	if (qnap8528_preserve_leds && (cdev->flags & LED_UNREGISTERING))
		return 0;
	return qnap8528_ec_write(QNAP8528_LED_USB_REG, brightness ? 2 : 0);
}

static int qnap8528_led_usb_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off)
{
	/* HW blink acceptable range: measured blink rate is ~376ms on/off, tolerance of ~25% */
	if (!(*delay_on == 0 && *delay_off == 0) && (*delay_on < 280 || *delay_on > 470 || *delay_off < 280 || *delay_off > 470))
		return -EINVAL;

	return qnap8528_ec_write(QNAP8528_LED_USB_REG, 1);
}

static int qnap8528_led_ident_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	if (qnap8528_preserve_leds && (cdev->flags & LED_UNREGISTERING))
		return 0;

	return qnap8528_ec_write(QNAP8528_LED_IDENT_REG, brightness ? 1 : 2);
}

static int qnap8528_led_jbod_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	if (qnap8528_preserve_leds && (cdev->flags & LED_UNREGISTERING))
		return 0;
	return qnap8528_ec_write(QNAP8528_LED_JBOD_REG, !!brightness);
}

static int qnap8528_led_10g_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	if (qnap8528_preserve_leds && (cdev->flags & LED_UNREGISTERING))
		return 0;
	return qnap8528_ec_write(QNAP8528_LED_10G_REG, !!brightness);
}

static int qnap8528_led_slot_set(struct led_classdev *cdev,	enum led_brightness brightness)
{
	struct qnap8528_slot_led *sled = container_of(cdev, struct qnap8528_slot_led, led_cdev);

	if (qnap8528_preserve_leds && (cdev->flags & LED_UNREGISTERING))
		return 0;

	/*
	 * Here comes a lot of logic, this might be unnecessary and writing to non-existent
	 * LED EC register is OK, but we dont know that, so let's start branching like crazy.
	 */ 

	/* Start with a clean state, disable all LED states */
	if (sled->is_hw_blink) {
		if (sled->slot_cfg->has_active)
			qnap8528_ec_write(EC_LED_DISK_ACTIVE_OFF_REG, sled->slot_cfg->ec_index);
		if (sled->slot_cfg->has_locate)
			qnap8528_ec_write(EC_LED_DISK_LOCATE_OFF_REG, sled->slot_cfg->ec_index);
	}

	if (sled->slot_cfg->has_present)
		qnap8528_ec_write(EC_LED_DISK_PRESENT_OFF_REG, sled->slot_cfg->ec_index);
	if (sled->slot_cfg->has_error)
		qnap8528_ec_write(EC_LED_DISK_ERROR_OFF_REG, sled->slot_cfg->ec_index);

	/* What state do we want to achieve? */
	switch((int)brightness) {
	case 0:
		/* LEDs are already OFF, just set the HW blink flag */
		sled->is_hw_blink = false;
		break;
	case 1:
		/* Do we have a present LED? Good, otherwise treat as only error 
		 * If no present, we also dont care about activity since its needs present
		 * to be ON
		 */
		if (sled->slot_cfg->has_present) {
			qnap8528_ec_write(EC_LED_DISK_PRESENT_ON_REG, sled->slot_cfg->ec_index);
			
			if (sled->is_hw_blink && sled->slot_cfg->has_active)
				qnap8528_ec_write(EC_LED_DISK_ACTIVE_ON_REG, sled->slot_cfg->ec_index);

			break;
		}
		__attribute__((__fallthrough__));
	case 2:
		/* Turn on error LED and blink if state was blinking */
		if (sled->slot_cfg->has_error)
			qnap8528_ec_write(EC_LED_DISK_ERROR_ON_REG, sled->slot_cfg->ec_index);
		
		if (sled->is_hw_blink && sled->slot_cfg->has_locate)
			qnap8528_ec_write(EC_LED_DISK_LOCATE_ON_REG, sled->slot_cfg->ec_index);
		break;
	}

	return 0;
}

static int qnap8528_led_slot_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off)
{
	struct qnap8528_slot_led *sled = container_of(cdev, struct qnap8528_slot_led, led_cdev);

	/* HW blink acceptable range: measured at ~121ms and ~108ms for green/(red/amber), assuming a rate of 110 with a tolerance of ~25% */
	if (!(*delay_on == 0 && *delay_off == 0) && (*delay_on < 80 || *delay_on > 140 || *delay_off < 80 || *delay_off > 140))
		return -EINVAL;

	sled->is_hw_blink = true;

	if ((sled->led_cdev.brightness == 2) && sled->slot_cfg->has_locate) {
		qnap8528_ec_write(EC_LED_DISK_ACTIVE_OFF_REG, sled->slot_cfg->ec_index);
		qnap8528_ec_write(EC_LED_DISK_LOCATE_ON_REG, sled->slot_cfg->ec_index);
		return 0;
	} else if (sled->slot_cfg->has_active) {
		qnap8528_ec_write(EC_LED_DISK_LOCATE_OFF_REG, sled->slot_cfg->ec_index);
		qnap8528_ec_write(EC_LED_DISK_PRESENT_ON_REG, sled->slot_cfg->ec_index);
		qnap8528_ec_write(EC_LED_DISK_ACTIVE_ON_REG, sled->slot_cfg->ec_index);
		return 0;
	}

	sled->is_hw_blink = false;
	return -EINVAL;
}

static int qnap8528_led_panel_brightness_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	int ret = -EINVAL;
	u8 tmp;

	/* Always preserve the panel brightness, no matter the param */
	if ((cdev->flags & LED_UNREGISTERING))
		return 0;

	ret = qnap8528_ec_write(0x243, brightness);
	if (ret)
		return ret;

	ret = qnap8528_ec_read(0x245, &tmp);
	if (ret)
		return ret;
	tmp |= 0x10;

	ret = qnap8528_ec_write(0x245, tmp);
	if (ret)
		return ret;

	ret = qnap8528_ec_write(0x246, brightness);
	if (ret)
		return ret;

	ret = qnap8528_ec_read(0x245, &tmp);
	if (ret)
		return ret;
	tmp |= 0xef;

	ret = qnap8528_ec_write(0x245, tmp);
	if (ret)
		return ret;

	return ret;
}

static int qnap8528_register_leds(struct device *dev)
{
	int i, ret;
	struct qnap8528_slot_config *slots;
	struct qnap8528_slot_led *sled;
	struct qnap8528_dev_data *data = dev_get_drvdata(dev);

	/* Just allocate them all synamically?  */
	if (data->config->features.led_status) {
		data->led_status.cdev.name = DRVNAME "::status";
		data->led_status.cdev.max_brightness = 2;
		data->led_status.cdev.brightness_set_blocking = qnap8528_led_status_set;
		data->led_status.cdev.blink_set = qnap8528_blink_sw_only ? NULL : qnap8528_led_status_blink;
		devm_led_classdev_register(dev, &data->led_status.cdev);
		ret = qnap8528_blink_sw_only ? 0 : device_create_file(data->led_status.cdev.dev, &dev_attr_blink_bicolor);
		if (ret)
			return ret;
	}

	if (data->config->features.led_usb) {
		data->led_usb.cdev.name = DRVNAME "::usb";
		data->led_usb.cdev.max_brightness = 1;
		data->led_usb.cdev.brightness_set_blocking = qnap8528_led_usb_set;
		data->led_usb.cdev.blink_set = qnap8528_blink_sw_only ? NULL : qnap8528_led_usb_blink;
		devm_led_classdev_register(dev, &data->led_usb.cdev);
	}

	if (data->config->features.led_ident) {
		data->led_ident.cdev.name = DRVNAME "::ident";
		data->led_ident.cdev.max_brightness = 1;
		data->led_ident.cdev.brightness_set_blocking = qnap8528_led_ident_set;
		devm_led_classdev_register(dev, &data->led_ident.cdev);
	}

	if (data->config->features.led_jbod) {
		data->led_jbod.cdev.name = DRVNAME "::jbod";
		data->led_jbod.cdev.max_brightness = 1;
		data->led_jbod.cdev.brightness_set_blocking = qnap8528_led_jbod_set;
		devm_led_classdev_register(dev, &data->led_jbod.cdev);
	}

	if (data->config->features.led_10g) {
		data->led_10g.cdev.name = DRVNAME "::10GbE";
		data->led_10g.cdev.max_brightness = 1;
		data->led_10g.cdev.brightness_set_blocking = qnap8528_led_10g_set;
		devm_led_classdev_register(dev, &data->led_10g.cdev);
	}

	if (data->config->features.led_brightness) {
		data->led_brightness.cdev.name = DRVNAME "::panel_brightness";
		data->led_brightness.cdev.max_brightness = 100;
		data->led_brightness.cdev.brightness_set_blocking = qnap8528_led_panel_brightness_set;
		devm_led_classdev_register(dev, &data->led_brightness.cdev);
	}

	if (data->config->slots) {
		slots = data->config->slots;
		for (i = 0; slots[i].name; i++) {
			/* If there is not least 1 static LED, why bother? */
			if (!(slots[i].has_present | slots[i].has_error))
				continue;

			sled = devm_kzalloc(dev, sizeof(*sled), GFP_KERNEL);
			if (!sled)
				return -ENOMEM;
			sled->slot_cfg = &slots[i];
			sled->led_cdev.name = devm_kasprintf(dev, GFP_KERNEL, DRVNAME "::%s", slots[i].name);
			if (!sled->led_cdev.name)
				return -ENOMEM;
			sled->led_cdev.max_brightness = 2;
			sled->led_cdev.brightness_set_blocking = qnap8528_led_slot_set;
			sled->led_cdev.blink_set = ((slots[i].has_active || slots[i].has_locate) && !qnap8528_blink_sw_only) ? qnap8528_led_slot_blink : NULL;
			sled->pdev = dev;
			devm_led_classdev_register(dev, &sled->led_cdev);
		}
	}
	pr_info("LED devices registered");
	return 0;
}

/* Keeping this just in case we need it later on */

/**
 *
 *static int qnap8528_fan_status_get(unsigned int fan)
 *{
 *	u16 reg;
 *	u8 value;
 *
 *	if (fan >=0 && fan <= 5)
 *		reg = 0x242;
 *	else if (fan >= 6 && fan <= 7)
 *		reg = 0x244;
 *	else if (fan >= 0x14 && fan <= 0x19)
 *		reg = 0x259;
 *	else if (fan >= 0x1e && fan <= 0x23)
 *		reg = 0x25a;
 *	else
 *		return -EINVAL;
 *
 *	if (qnap8528_ec_read(reg, &value))
 *		return -EBUSY;
 *
 *	if (fan >=0 && fan <= 5)
 *		return ((value >> (fan & 0x1f)) & 1) == 0 ? -ENODEV : 0;
 *	if (fan >= 6 && fan <= 7)
 *		return ((value >> ((fan - 0x06) & 0x1f)) & 1) == 0 ? -ENODEV : 0;
 *	if (fan >= 0x14 && fan <= 0x19)
 *		return ((value >> ((fan - 0x14) & 0x1f)) & 1) == 0 ? -ENODEV : 0;
 *	if (fan >= 0x1e && fan <= 0x23)
 *		return ((value >> ((fan - 0x1e) & 0x1f)) & 1) == 0 ? -ENODEV : 0;
 *
 *	return -EINVAL;
 * }
 */

static int qnap8528_fan_rpm_get(unsigned int fan)
{
	u8 tmp;
	u16 value, reg_a, reg_b;
	int ret;

	if (fan >= 0 && fan <= 5) {
		reg_a = (fan + 0x312) * 2;
		reg_b = (fan * 2) + 0x625;
	} else if (fan == 6 || fan == 7) {
		reg_a = (fan + 0x30a) * 2;
		reg_b = ((fan - 6) * 2) + 0x621;
	} else if (fan == 0x0a) {
		reg_a = 0x65b;
		reg_b = 0x65a;
	} else if (fan == 0x0b) {
		reg_a = 0x65e;
		reg_b = 0x65d;
	} else if (fan >= 0x14 && fan <= 0x19) {
		reg_a = (fan + 0x30e) * 2;
		reg_b = ((fan - 0x14) * 2) + 0x645;
	} else if (fan >= 0x1e && fan <= 0x23) {
		reg_a = (fan + 0x2f8) * 2;
		reg_b = ((fan - 0x1e) * 2) + 0x62d;
	} else {
		return -EINVAL;
	}

	ret = qnap8528_ec_read(reg_a, &tmp);
	if (ret)
		return ret;
	value = tmp << 8;

	ret = qnap8528_ec_read(reg_b, &tmp);
	if (ret)
		return ret;
	return value | tmp;
}

static int qnap8528_fan_pwm_get(unsigned int fan)
{
	u16 reg;
	u8 value;
	int ret;

	if (fan >= 0 && fan <= 5)
		reg = 0x22e;
	else if (fan == 6 || fan == 7)
		reg = 0x24b;
	else if (fan >= 0x14 && fan <= 0x19)
		reg = 0x22f;
	else if (fan >= 0x1e && fan <= 0x23)
		reg = 0x23b;
	else
		return -EINVAL;

	ret = qnap8528_ec_read(reg, &value);
	if (ret)
		return ret;
	return (value * 0x100 - value) / 100;
}

static int qnap8528_fan_pwm_set(unsigned int fan, u8 value)
{
	u16 reg_a, reg_b;
	int ret;

	if (value > 255)
		value = 255;

	value = (value * 100) / 0xff;

	if (fan >= 0 && fan <= 5) {
		reg_a = 0x220;
		reg_b = 0x22e;
	} else if (fan == 6 || fan == 7) {
		reg_a = 0x223;
		reg_b = 0x24b;
	} else if (fan >= 0x14 && fan <= 0x19) {
		reg_a = 0x221;
		reg_b = 0x22f;
	} else if (fan >= 0x1e && fan <= 0x23) {
		reg_a = 0x222;
		reg_b = 0x23b;
	} else {
		return -EINVAL;
	}

	ret = qnap8528_ec_write(reg_a, 0x10);
	if (ret)
		return ret;

	ret = qnap8528_ec_write(reg_b, value);
	if (ret)
		return ret;

	return 0;
}

static int qnap8528_temperature_get(unsigned int sensor)
{
	u8 value;
	int ret;
	u16 reg = 0;

	if (sensor == 0 || sensor == 1)				/* CPU temp only if CPU_TEMP_UNIT == "EC" else ??? */
		reg = 0x600 + sensor;
	else if (sensor >= 5 && sensor <= 7)		/* System temp unit */
		reg = 0x5fd + sensor;
	else if (sensor == 0x0a)					/* Only if redandnat power */
		reg = 0x659;
	else if (sensor == 0x0b)					/* Only if redandnat power */
		reg = 0x65c;
	else if (sensor >= 0xf && sensor <= 0x26)	/* Env temp unit */
		reg = 0x5f7 + sensor;


	ret = reg ? qnap8528_ec_read(reg, &value) : -EBUSY;
	if (ret)
		return ret;

	return (value < 128) && (value > 0) ? value : -ENODEV;
}

static void qnap8528_input_poll(struct input_dev *input)
{
	u8 val = 0;

	if (qnap8528_ec_read(QNAP8528_BUTTON_INPUT_REG, &val))
		return;

	input_event(input, EV_KEY, BTN_0, !!(val & QNAP8528_INPUT_BTN_CHASSIS));
	input_event(input, EV_KEY, BTN_1, !!(val & QNAP8528_INPUT_BTN_COPY));
	input_event(input, EV_KEY, BTN_2, !!(val & QNAP8528_INPUT_BTN_RESET));
	input_sync(input);
}

static int qnap8528_register_inputs(struct device *dev)
{
	int ret;
	struct qnap8528_dev_data *data = dev_get_drvdata(dev);

	data->input_dev = devm_input_allocate_device(dev);
	if (IS_ERR(data->input_dev))
		return PTR_ERR(data->input_dev);

	data->input_dev->name = DRVNAME;
	data->input_dev->dev.parent = dev;
	data->input_dev->phys = DRVNAME "/input0";
	data->input_dev->id.bustype = BUS_HOST;

	input_set_capability(data->input_dev, EV_KEY,  BTN_0);
	input_set_capability(data->input_dev, EV_KEY,  BTN_1);
	input_set_capability(data->input_dev, EV_KEY,  BTN_2);

	ret = input_setup_polling(data->input_dev, qnap8528_input_poll);
	if (ret)
		return ret;

	input_set_poll_interval(data->input_dev, QNAP8528_INPUT_POLL_TIME);
	ret = input_register_device(data->input_dev);
	if (ret)
		return ret;

	pr_info("Buttons input device registered");
	return 0;
}

static umode_t qnap8528_hwmon_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr, int channel)
{
	u8 val;
	int i;
	umode_t mode = 0;
	struct qnap8528_dev_data *dev_data = (struct qnap8528_dev_data *)data;

	switch (type) {
	case hwmon_temp:
		val = qnap8528_temperature_get(channel);
		mode = ((val > 0) && (val < 128)) ? 0444 : 0;
		break;
	case hwmon_fan:
		channel += 1;
		if (!dev_data->config->fans)
			break;

		for (i = 0; dev_data->config->fans[i]; i++) {
			if (dev_data->config->fans[i] == channel)
				mode = 0444;
		}
		break;
	case hwmon_pwm:
		channel += 1;
		if (!dev_data->config->fans)
			break;

		for (i = 0; dev_data->config->fans[i]; i++) {
			if (dev_data->config->fans[i] == channel) {
				if (channel >= 0 && channel <= 5) {
					if (!dev_data->hm_pwm_channels[0]) {
						dev_data->hm_pwm_channels[0] = true;
						mode = 0644;
						break;
					}
				}

				if (channel == 6 || channel == 7) {
					if (!dev_data->hm_pwm_channels[1]) {
						dev_data->hm_pwm_channels[1] = true;
						mode = 0644;
						break;
					}
				}

				if (channel >= 0x14 && channel <= 0x19) {
					if (!dev_data->hm_pwm_channels[2]) {
						dev_data->hm_pwm_channels[2] = true;
						mode = 0644;
						break;
					}
				}

				if (channel >= 0x1e && channel <= 0x23) {
					if (!dev_data->hm_pwm_channels[3]) {
						dev_data->hm_pwm_channels[3] = true;
						mode = 0644;
						break;
					}
				}
			}
		}
	default:
	}
	return mode;
}

static int qnap8528_hwmon_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_temp:
		*val = qnap8528_temperature_get(channel) * 1000;
		return 0;
	case hwmon_fan:
		*val = qnap8528_fan_rpm_get(channel);
		return 0;
	case hwmon_pwm:
		*val = qnap8528_fan_pwm_get(channel);
		return 0;
	default:
	}
	return -ENOTSUPP;
}

static int qnap8528_hwmon_write(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long val)
{
	if (type == hwmon_pwm)
		return qnap8528_fan_pwm_set(channel, (val > 255) ? 255 : val);
	return -ENOTSUPP;
}

static int qnap8528_register_hwmon(struct device *dev)
{
	int i;
	struct qnap8528_dev_data *data = dev_get_drvdata(dev);

	for (i = 0; i < QNAP8528_HWMON_MAX_CHANNELS + 1; i++) {
		qnap8528_hwmon_temp_config[i] = HWMON_T_INPUT;
		qnap8528_hwmon_fan_config[i] = HWMON_F_INPUT;
		qnap8528_hwmon_pwm_config[i] = HWMON_PWM_INPUT;
	}

	for (i = 0; i < QNAP8528_HWMON_PWM_BANKS; i++)
		data->hm_pwm_channels[i] = false;

	data->hwmon_dev = devm_hwmon_device_register_with_info(dev, DRVNAME, data, &qnap8528_hwmon_chip_info, NULL);
	if (IS_ERR(data->hwmon_dev))
		return PTR_ERR(data->hwmon_dev);

	pr_info("Hwmon device registered");
	return 0;
}

static struct qnap8528_config *qnap8528_find_config(void)
{
	int i;
	size_t bp_s_size;
	char mb_model[QNAP8528_VPD_ENTRY_MAX + 1];
	char bp_model[QNAP8528_VPD_ENTRY_MAX + 1];

	qnap8528_vpd_attr_show(NULL, &(struct qnap8528_device_attribute){.vpd_entry = QNAP8528_VPD_MB_MODEL}, mb_model);
	// CR: Should we really be doing this if there is no backplane table?
	qnap8528_vpd_attr_show(NULL, &(struct qnap8528_device_attribute){.vpd_entry = QNAP8528_VPD_BP_MODEL}, bp_model);

	if (!strnlen(mb_model, 32))
		return 0;
	bp_s_size = strnlen(bp_model, 32);

	pr_info("Searching configs for a match with MB=%s", mb_model);

	//  && qnap8528_configs[i].bp_model
	for (i = 0; qnap8528_configs[i].mb_model; i++) {
		//  || 
		if (!strnlen(qnap8528_configs[i].mb_model, 32))
			continue;

		// &&
		if (strstr(mb_model, qnap8528_configs[i].mb_model)) {
			pr_info("Model MB code match found");
			if (strnlen(qnap8528_configs[i].bp_model, 32)) {
				pr_info("Model config requires BP match too"); 
				if (bp_s_size && strstr(bp_model, qnap8528_configs[i].bp_model)) {
					pr_info("Model BP code match found, device model is %s", qnap8528_configs[i].name);
					return &qnap8528_configs[i];
				}
			} else {
				pr_info("Device model is %s", qnap8528_configs[i].name);
				return &qnap8528_configs[i];
			}
		}
	}
	pr_err("Could not find configuration for device, please report this issue");
	return 0;
}

static int qnap8528_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct qnap8528_dev_data *data;

	/*
	 * Skip HW check for the EC
	 * Not really usefull, but left in as a feature from debugging
	 */
	if (!qnap8528_skip_hw_check)
		ret = qnap8528_ec_hw_check();
	else
		pr_warn("Skipping HW check for IT8528");

	if (ret)
		return ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, data);

	data->config = qnap8528_find_config();
	if (!data->config)
		return -ENOTSUPP;

	ret = qnap8528_register_inputs(&pdev->dev);
	if (ret)
		return ret;

	ret = qnap8528_register_leds(&pdev->dev);
	if (ret)
		return ret;

	ret = qnap8528_register_hwmon(&pdev->dev);
	if (ret)
		return ret;

	return 0;
}

static void __exit qnap8528_exit(void)
{
	if (qnap8528_pdevice)
		platform_device_unregister(qnap8528_pdevice);
	platform_driver_unregister(&qnap8528_pdriver);
	pr_info("Module unloaded");
}

static int __init qnap8528_init(void)
{
	int ret = 0;

	/* Create a platform driver and a (pseudo) platform device to probe the driver */
	ret = platform_driver_register(&qnap8528_pdriver);
	if (ret)
		goto qnap8528_init_ret;

	qnap8528_pdevice = platform_device_register_simple(DRVNAME, PLATFORM_DEVID_NONE, qnap8528_resources, ARRAY_SIZE(qnap8528_resources));
	if (IS_ERR(qnap8528_pdevice)) {
		ret = PTR_ERR(qnap8528_pdevice);
		goto qnap8528_init_driver_unregister;
	}

	/* If the driver failed to probe, no point of having the module loaded */
	if (!qnap8528_pdevice->dev.driver) {
		pr_err("qnap8528 device driver failed to probe, unloading");
		ret = -ENODEV;
		goto qnap8528_init_device_unregister;
	}

	goto qnap8528_init_ret;

qnap8528_init_device_unregister:
	platform_device_unregister(qnap8528_pdevice);
qnap8528_init_driver_unregister:
	platform_driver_unregister(&qnap8528_pdriver);
qnap8528_init_ret:
	return ret;
}

MODULE_AUTHOR("0xGiddi <qnap8528@giddi.net>");
MODULE_DESCRIPTION("QNAP IT8528 EC driver");
MODULE_VERSION("1.10");
MODULE_LICENSE("GPL");

module_init(qnap8528_init);
module_exit(qnap8528_exit);
