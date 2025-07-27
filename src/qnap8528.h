/* SPDX-License-Identifier: GPL-2.0-or-later */
#undef pr_fmt
#define pr_fmt(fmt) "%s @ %s: " fmt "\n", DRVNAME, __func__

#ifdef DEBUG
	#define DRIVER_DATA_DEBUG(data)  pr_debug("Driver data pointer: %p\n", data)
#else
	#define DRIVER_DATA_DEBUG(data)  /* do nothing */
#endif

#define DRVNAME "qnap8528"

#define QNAP8528_EC_CHIP_ID			0x8528
#define QNAP8528_EC_UDELAY          1000
#define QNAP8528_EC_MAX_RETRY       5000
#define QNAP8528_EC_CMD_PORT        0x6c
#define QNAP8528_EC_DAT_PORT        0x68

#define QNAP8528_EC_FW_VER_REG		0x308
#define QNAP8528_EC_FW_VER_LEN		8
#define QNAP8528_CPLD_VER_REG		0x320
#define QNAP8528_PWR_RECOVERY_REG	0x16
#define QNAP8528_EUP_SUPPORT_REG	0x101
#define QNAP8528_EUP_MODE_REG		0x121

/**
 * EC VPD entries
 *
 * There are 4 VPD tables, each table 512 byte in length. Each table
 * seems to hold information about a specific component of the system.
 * Only tables 0 and 1 seem to be used on the TS-x73A, being main board and
 * backplane VPD respectively. The only outliers are the case serial and system
 * nickname which are located in the backplane VPD.
 *
 * A VPD entry is located using the table number,offset withing the table,
 * length of the data and then parsed according to its type.
 *
 * These entry values are extracted from libLinux_hal, most are unknown.
 * Using values on stickers, PCB silkscreen and common sense
 * the following have been figured out and  assigned an attribute.
 *
 * The values bits are parsed as such:
 *	MSB														     LSB
 *	1111        11          11          11111111    1111111111111111
 *	Unknown	table			type		length		offset
 *				0: Main PCB		0 - str
 *				1: Backplane	1 - num
 *				2/3: Not used	2 - date
 *	Example:
 *	VPD entry 0x0c1000cb = table 3, offset 203, length 16 and type 0
 *	0000		11				00			00010000	0000000011001011
 *
 *
 *Notes:
 *	Tables 2 and 3 may be used for network and redandent power VPD
 *	as there are hints to it in hal_daemon/libLinux_hal. Maybe for the 3rd model
 *	code?
 */

#define QNAP8528_VPD_ENTRY_MAX      U8_MAX
#define QNAP8528_VPD_ENC_SERIAL     0xdeadbeef
#define QNAP8528_VPD_ENC_NICKNAME   0x001000d6
#define QNAP8528_VPD_MB_MANUF       0x0010000f
#define QNAP8528_VPD_MB_VENDOR      0x0010007c
#define QNAP8528_VPD_MB_NAME        0x00100020
#define QNAP8528_VPD_MB_MODEL       0x00200042
#define QNAP8528_VPD_MB_SERIAL      0x00100031
#define QNAP8528_VPD_MB_DATE        0x00203000
#define QNAP8528_VPD_BP_MANUF       0x04100037
#define QNAP8528_VPD_BP_VENDOR      0x04100094
#define QNAP8528_VPD_BP_NAME        0x04100048
#define QNAP8528_VPD_BP_MODEL       0x0420006a
#define QNAP8528_VPD_BP_SERIAL      0x04100059
#define QNAP8528_VPD_BP_DATE        0x06030033
#define QNAP8528_VPD_ENC_SER_MB     0x001000c3
#define QNAP8528_VPD_ENC_SER_BP     0x0410001d

#define QNAP8528_BUTTON_INPUT_REG	0x143
#define QNAP8528_INPUT_POLL_TIME	100
#define QNAP8528_INPUT_BTN_CHASSIS	BIT(0)
#define QNAP8528_INPUT_BTN_COPY		BIT(1)
#define QNAP8528_INPUT_BTN_RESET	BIT(2)

#define QNAP8528_LED_STATUS_REG     0x155
#define QNAP8528_LED_USB_REG        0x154
#define QNAP8528_LED_IDENT_REG      0x15e
#define QNAP8528_LED_JBOD_REG       0x156
#define QNAP8528_LED_10G_REG        0x167

#define EC_LED_DISK_ACTIVE_OFF_REG  0x157
#define EC_LED_DISK_ACTIVE_ON_REG   0x15f
#define EC_LED_DISK_LOCATE_OFF_REG  0x159
#define EC_LED_DISK_LOCATE_ON_REG   0x158
#define EC_LED_DISK_PRESENT_ON_REG  0x15a
#define EC_LED_DISK_PRESENT_OFF_REG 0x15b
#define EC_LED_DISK_ERROR_ON_REG    0x15c
#define EC_LED_DISK_ERROR_OFF_REG   0x15d

#define QNAP8528_HWMON_PWM_BANKS    4
#define QNAP8528_HWMON_MAX_CHANNELS 38



struct qnap8528_device_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, struct qnap8528_device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct qnap8528_device_attribute *attr,
			 const char *buf, size_t count);
	u32 vpd_entry;
};

#define __QNAP8528_ATTR(_name, _mode, _show, _store, _entry) {	\
	.attr = {.name = __stringify(_name),				    \
		 .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		    \
	.show	= _show,						                \
	.store	= _store,						                \
	.vpd_entry = _entry,                                    \
}

#define QNAP8528_DEVICE_ATTR(_name, _mode, _show, _store, _entry) struct qnap8528_device_attribute dev_attr_##_name = __QNAP8528_ATTR(_name, _mode, _show, _store, _entry)

/*
 * struct qnap8528_features - Device supported features
 *
 * @pwr_recovery        Can AC power recovery mode be set
 * @eup_mode            Can EuP mode be set
 * @led_brightness      Has global LED brightness control
 * @led_status          Has Status LED
 * @led_10g             Has 10GiB indicator LED
 * @led_usb             Has USB led
 * @led_jbod            Has JBOD (attached storage) LED
 * @led_ident           Has enclosure ident LED
 * @enc_serial_mb       Flag for location of enclosure serial in VPD tables
						0 - backplane table, 1 - mainboard table
 */
struct qnap8528_features {
	u32 pwr_recovery:1;
	u32 eup_mode:1;
	u32 led_brightness:1;
	u32 led_status:1;
	u32 led_10g:1;
	u32 led_usb:1;
	u32 led_jbod:1;
	u32 led_ident:1;
	u32 enc_serial_mb:1;
	u32 vpd_bp_table:1;
};

/*
 * struct qnap8528_slot_config - Disk slot features configuration
 *
 * *@name               Disk name (e.g. "m2ssd1", "u2ssd1", "ssd1", "hdd1"),
						slot names are derived from the original configuration
						'SLOT_NAME' field, converted to lowercase and removing all
						non-alphanumeric characters. If the slot name is "Disk %d"
						the name is changed to "hdd%d" for clarity.
 * *@ec_index           The EC control index for disk related tasks, this is either
						the number after the colon in `EC:%d` formatted fields, or
						the disk number if the field contains only 'EC' in original config.
 * *@has_present        Has present (static green) LED feature.
 * *@has_active         Has active (blinking green) LED feature.
 * *@has_error          Has error (static red) LED feature.
 * *@has_locate         Has locate (blinking red) LED feature.
 * *@has_power_ctrl     Flag if slot disk power can be toggled.
 */
struct qnap8528_slot_config {
	char *name;
	u8 ec_index;
	u8 has_present:1;
	u8 has_active:1;
	u8 has_error:1;
	u8 has_locate:1;
	u8 has_power_ctrl:1;
};

/*
 * struct qnap8528_slot_config - Disk slot features configuration
 *
 * @name:               Model name
 * @mb_code:            Mainboard model code to match
 * @bp_code:            Backplain model code to match
 * @features:           qnap8528_features struct for available features on this model
 * @fans:               0 terminated arrray of fans indexes supported by this model
 * @slots:              qnap8528_slot_config struct for creating slot related LEDs and attributes
 */
struct qnap8528_config {
	char *name;
	char *mb_model;
	char *bp_model;
	struct qnap8528_features features;
	u8 *fans;
	struct qnap8528_slot_config *slots;
};

struct qnap8528_slot_led {
	struct led_classdev led_cdev;
	struct qnap8528_slot_config *slot_cfg;
	struct device *pdev;
	bool is_hw_blink;
};

struct qnap8528_system_led {
	struct led_classdev cdev;
	bool is_hw_blink;
};

struct qnap8528_dev_data {
	struct qnap8528_config  *config;
	bool hm_pwm_channels[QNAP8528_HWMON_PWM_BANKS];
	/* Do I really need handles to all my devices?  */
	struct input_dev	    *input_dev;
	struct device           *hwmon_dev;
	struct qnap8528_system_led     led_status;
	struct qnap8528_system_led     led_usb;
	struct qnap8528_system_led     led_ident;
	struct qnap8528_system_led     led_jbod;
	struct qnap8528_system_led     led_10g;
	struct qnap8528_system_led     led_brightness;
};

static int qnap8528_ec_hw_check(void);
static int qnap8528_ec_wait_ibf_clear(void);
static int qnap8528_ec_clear_obf(void);
static int qnap8528_ec_wait_obf_set(void);
static int qnap8528_ec_send_command(u16 command);
static int qnap8528_ec_read(u16 command, u8 *data);
static int qnap8528_ec_write(u16 command, u8 data);

static umode_t qnap8528_ec_attr_check_visible(struct kobject *kobj, struct attribute *attr, int n);
static ssize_t qnap8528_fw_version_attr_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t qnap8528_cpld_version_attr_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t qnap8528_power_recovery_attr_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t qnap8528_power_recovery_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t qnap8528_eup_mode_attr_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t qnap8528_eup_mode_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t qnap8528_vpd_attr_show(struct device *dev, struct qnap8528_device_attribute *attr, char *buf);
static ssize_t qnap8528_vpd_parse(int type, int size, char *raw, char *buf);

static ssize_t blink_bicolor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static int qnap8528_led_status_set(struct led_classdev *cdev, enum led_brightness brightness);
static int qnap8528_led_status_blink(struct led_classdev *led_cdev, unsigned long *delay_on, unsigned long *delay_off);
static int qnap8528_led_usb_set(struct led_classdev *cdev, enum led_brightness brightness);
static int qnap8528_led_usb_blink(struct led_classdev *led_cdev, unsigned long *delay_on, unsigned long *delay_off);
static int qnap8528_led_slot_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off);
static int qnap8528_led_panel_brightness_set(struct led_classdev *cdev, enum led_brightness brightness);
static int qnap8528_register_leds(struct device *dev);

/* static int qnap8528_fan_status_get(unsigned int fan); */
static int qnap8528_fan_rpm_get(unsigned int fan);
static int qnap8528_fan_pwm_get(unsigned int fan);
static int qnap8528_fan_pwm_set(unsigned int fan, u8 value);
static int qnap8528_temperature_get(unsigned int sensor);

static void qnap8528_input_poll(struct input_dev *input);
static int qnap8528_register_inputs(struct device *dev);

static umode_t qnap8528_hwmon_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr, int channel);
static int qnap8528_hwmon_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *val);
static int qnap8528_hwmon_write(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long val);
static int qnap8528_register_hwmon(struct device *dev);

static struct qnap8528_config *qnap8528_find_config(void);
static int qnap8528_probe(struct platform_device *pdev);
static void __exit qnap8528_exit(void);
static int __init qnap8528_init(void);

static struct qnap8528_config qnap8528_configs[] = {
		{
		"TDS-2489FU", "Q0530", "Q0590",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 5, 6, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TDS-2489FU R2", "Q0531", "Q0590",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 5, 6, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TES-1885U", "QX540", "QY270",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TES-3085U", "QX541", "QY510",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TNS-1083X", "Q0410", "Q0490",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TNS-C1083X", "Q0411", "Q0490",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1090FU", "Q09B0", "Q09I0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 5, 4, 2, 1, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd9", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd10", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1273AU", "Q0520", "Q05G0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1273AU-RP", "Q0520", "Q0670",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1277XU", "QZ490", "QZ550",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1283XU", "QZ601", "Q00M0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 6, 1, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1290FX", "Q09A0", "Q09C0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_usb        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd9", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd10", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd11", .ec_index = 11, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd12", .ec_index = 12, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1655", "Q07Z1", "Q08G0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1673AU-RP", "Q0520", "Q0580",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1677XU", "QZ491", "QZ540",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1683XU", "QZ601", "Q0040",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 6, 1, 2, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1685", "QY380", "QY390",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd3", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd4", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd5", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd6", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1886XU", "Q0470", "Q04L0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 17, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 18, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1886XU R2", "Q0B50", "Q0950",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 17, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 18, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1887XU", "Q0840", "Q0950",
		.features = {
			.eup_mode       = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-2287XU", "Q0840", "Q08A0",
		.features = {
			.eup_mode       = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-2477XU", "QZ500", "Q0070",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-2483XU", "Q00V1", "Q00W0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 6, 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-2490FU", "Q03X0", "Q04K0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-2888X", "Q00Q0", "Q00S0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 3, 4, 21, 22, 31, 32, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "u2ssd1", .ec_index = 25, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd2", .ec_index = 26, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd3", .ec_index = 27, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "u2ssd4", .ec_index = 28, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd7", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd8", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd9", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd10", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd11", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd12", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd13", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd14", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd15", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd16", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-3087XU", "Q08H0", "Q08Z0",
		.features = {
			.eup_mode       = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 30, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 29, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 28, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 27, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 26, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 25, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-3088XU", "Q06X0", "Q06Y0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_10g        = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "ssd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd25", .ec_index = 25, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd26", .ec_index = 26, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd27", .ec_index = 27, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd28", .ec_index = 28, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd29", .ec_index = 29, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd30", .ec_index = 30, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-473A", "Q07D0", "Q07N0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-655X", "Q0CH0", "Q0CI0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-673A", "Q07D0", "Q07M0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-686", "Q05S0", "Q0660",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 8, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-855EU", "Q0BT0", "Q0BU0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-855X", "Q0CH0", "Q0CJ0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-873A", "Q07D0", "Q07L0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-873AEU", "Q0AK0", "Q0AO0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-873AEU-RP", "Q0AK0", "Q0AO1",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-873AU", "Q0520", "Q05G1",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-873AU-RP", "Q0520", "Q0671",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-877XU", "QZ490", "QZ551",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-883XU", "QZ601", "Q00M1",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 6, 1, 4, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-886", "Q05S0", "Q0650",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 8, 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-973AX", "Q0711", "Q0760",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-977XU", "QZ480", "Q0060",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-983XU", "Q00I1", "Q00X0",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 6, 3, 2, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd5", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-1275U", "SAP00", "SBO70",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 3, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-1288X", "Q05W0", "Q05K0",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-1675U", "SAP00", "SBO80",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 3, 2, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd13", .ec_index = 13, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd14", .ec_index = 14, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd15", .ec_index = 15, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd16", .ec_index = 16, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-1688X", "Q05T0", "Q0630",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-472X", "Q0420", "Q0180",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-472XT", "Q0120", "Q0180",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-474", "Q0BB0", "Q0BL0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-672N", "Q0420", "Q0170",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-672X", "Q0121", "Q0170",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-672XT", "Q0120", "Q0170",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-674", "B6490", "Q0BK0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-675", "Q08B0", "Q0890",
		.features = {
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-872N", "Q0420", "Q0160",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-872X", "Q0121", "Q0160",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-872XT", "Q0120", "Q0160",
		.features = {
			.pwr_recovery   = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 8, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-874", "B6490", "Q0AA0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-875U", "SAP00", "SBO60",
		.features = {
			.pwr_recovery   = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 3, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-951N", "SAN30", "SBN10",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-951X", "Q0090", "Q00A0",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd3", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd4", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-H674T", "B6491", "Q0BK0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-H674X", "B6492", "Q0BK0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-H874T", "B6491", "Q0AA0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TVS-H874X", "B6492", "Q0AA0",
		.features = {
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464", "Q07R1", "Q08F0",
		{
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464C", "SAQ93", "SBR00",
		{
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464C2", "SAQ95", "SBR00",
		{
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd1", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464T4", "Q0910", "Q08F0",
		{
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464U", "Q08S0", "QY740",
		{
			.eup_mode       = 1,
			.led_status     = 1,
			.led_ident      = 1,
			.vpd_bp_table	= 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-1264U", "Q08R0", "Q08X0",
		{
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_error = 1, .has_locate = 1},
			{ .name = "hdd9", .ec_index = 9, .has_error = 1, .has_locate = 1},
			{ .name = "hdd10", .ec_index = 10, .has_error = 1, .has_locate = 1},
			{ .name = "hdd11", .ec_index = 11, .has_error = 1, .has_locate = 1},
			{ .name = "hdd12", .ec_index = 12, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464U", "Q08S0", "QY740",
		{
			.eup_mode       = 1,
			.led_status     = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-864EU", "Q0AN0", "Q0AS0",
		{
			.led_status     = 1,
			.led_jbod       = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
			{ .name = "hdd7", .ec_index = 7, .has_error = 1, .has_locate = 1},
			{ .name = "hdd8", .ec_index = 8, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-251D", "Q04W1", "QY570",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-253D", "Q05H0", "QY570",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-451D", "SAO50", "SBK40",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-451D2", "SAO51", "SBK41",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-453D", "Q04M0", "QY581",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
		{
		"TS-686", "Q05S1", "Q0660",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 8, 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
		{
		"TS-886", "Q05S1", "Q0650",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 8, 7, 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 9, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 10, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd1", .ec_index = 7, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "ssd2", .ec_index = 8, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-653D", "Q04O0", "Q04N0",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-ALTO4", "Q05X0", "Q06A0",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
			{
		"TS-251A", "QX690", "QX710",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
		{
		"TS-451A", "QX690", "QX700",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.led_ident      = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	

	{
		"TS-253B", "QY470", "QY570",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-253BE", "QY472", "QY570",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
		{
		"TS-453B", "QZ060", "QZ070",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
		{
		"TS-453BE", "QY472", "QY580",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-653B", "QY471", "QY590",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 2, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 6, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 5, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd5", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd6", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},

{
        "TS-1277XU", "QZ492", "QZ552",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1277XU", "QZ492", "QZ550",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1277XU", "QZ494", "QZ550",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1277XU", "QZ490", "QZ550",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1277XU", "QZ493", "QZ550",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1677XU", "QZ492", "QZ540",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1677XU", "QZ494", "QZ540",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-1677XU", "QZ491", "QZ540",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-2477XU", "QZ503", "Q0070",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-2477XU", "QZ500", "Q0070",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-2477XU", "QZ504", "Q0070",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-2477XU", "QZ502", "Q0070",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd9", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd10", .ec_index = 10, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd11", .ec_index = 11, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd12", .ec_index = 12, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd13", .ec_index = 13, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd14", .ec_index = 14, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd15", .ec_index = 15, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd16", .ec_index = 16, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd17", .ec_index = 17, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd18", .ec_index = 18, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd19", .ec_index = 19, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd20", .ec_index = 20, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd21", .ec_index = 21, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd22", .ec_index = 22, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd23", .ec_index = 23, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd24", .ec_index = 24, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-877XU", "QZ490", "QZ560",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-877XU", "QZ493", "QZ551",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-877XU", "QZ490", "QZ551",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-877XU", "QZ492", "QZ551",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-877XU", "QZ494", "QZ551",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 4, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd7", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd8", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-977XU", "QZ482", "Q0060",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd1", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd2", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd3", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd4", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd5", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-977XU", "QZ480", "Q0060",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd1", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd2", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd3", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd4", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd5", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
        {
        "TS-977XU", "QZ481", "Q0060",
        {
            .pwr_recovery   = 1,
            .eup_mode       = 1,
            .led_status     = 1,
            .led_jbod       = 1,
            .led_ident      = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 3, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd1", .ec_index = 5, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd2", .ec_index = 6, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd3", .ec_index = 7, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd4", .ec_index = 8, .has_present = 1, .has_error = 1, .has_locate = 1},
                        { .name = "ssd5", .ec_index = 9, .has_present = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-262", "Q07R3", "Q09N0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-262C", "SAQ90", "SBQ10",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-264", "Q07R0", "Q09N0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-264", "Q07R5", "Q09N0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-264C", "SAQ91", "SBQ10",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd1", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd2", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-364", "Q08E0", "Q08T0",
        {
            .eup_mode       = 1,
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd1", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd2", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-462", "Q07R4", "Q08F0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-462C", "SAQ92", "SBR00",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-464", "Q07R6", "Q08F0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-564", "SAQ10", "SBP90",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
		/* No names in config, generic HHD1-5 */
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-664", "Q07R7", "Q07S0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{
        "TS-664", "Q07R2", "Q07S0",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 2, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
                        { .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
                        { .name = "hdd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
                        { .name = "hdd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
                        { .name = "hdd5", .ec_index = 5, .has_error = 1, .has_locate = 1},
                        { .name = "hdd6", .ec_index = 6, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	/* Here add models without BP code */
	{
		"TS-453BMINI", "QY600", "",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
			.vpd_bp_table   = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-453DMINI", "SAO40", "",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "hdd1", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TBS-453DX", "QZ530", "",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
			.led_usb        = 1,
		},
		.fans = (u8[]){ 1, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd3", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd4", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"HS-453DX", "QZ290", "",
		{
			.pwr_recovery   = 1,
			.eup_mode       = 1,
			.led_brightness = 1,
			.led_status     = 1,
		},
		.fans = NULL,
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 3, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 4, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
		"TS-464EU", "Q0930", "",
		{
			.eup_mode       = 1,
			.led_status     = 1,
			.led_jbod       = 1,
		},
		.fans = (u8[]){ 1, 2, 3, 0},
		.slots = (struct qnap8528_slot_config[]){
			{ .name = "m2ssd1", .ec_index = 5, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "m2ssd2", .ec_index = 6, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd1", .ec_index = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd2", .ec_index = 2, .has_error = 1, .has_locate = 1},
			{ .name = "hdd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ .name = "hdd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
			{ NULL }
		}
	},
	{
        "TBS-464", "Q07X0", "",
        {
            .led_brightness = 1,
            .led_status     = 1,
            .led_usb        = 1,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[]){ 1, 0},
        .slots = (struct qnap8528_slot_config[]){
                        { .name = "m2ssd1", .ec_index = 1, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd2", .ec_index = 2, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd3", .ec_index = 3, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
                        { .name = "m2ssd4", .ec_index = 4, .has_present = 1, .has_active = 1, .has_error = 1, .has_locate = 1},
            { NULL }
        }
    },
	{ NULL }
};
