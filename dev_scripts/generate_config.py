#!/usr/bin/env python3
import os
import sys
import configparser


class QNAPDiskSlot(object):
    def __init__(self):
        self.name = ""
        self.present_led = 0
        self.error_led = 0
        self.locate_led = 0
        self.blink_led = 0
        self.has_power_control = 0

class QNAPModelConfig(object):
    def __init__(self):
        # Model info
        self.model_name = None
        self.fixed_name = None
        self.mb_code = ""
        self.bp_code = ""
        # Misc
        self.ac_recovery = False
        self.eup_mode = False
        # VPD
        self.serial_location = ""
        # Buttons
        self.button_copy = False
        self.button_reset = False
        self.button_chassis_open = False
        # LEDs
        self.led_brightness = False
        self.teng_led = False
        self.front_usb_led = False
        self.jbod_connect_led = False
        self.locate_led = False
        self.status_led = False
        # Power
        self.redundant_power_1 = False
        self.redundant_power_2 = False
        # Disks
        self.disk_slots = []  # This is now an instance variable
        self.path = ""
        self.num_disks = 0
        self.slot_blink_problems = False
        self.fans = []
        self.max_fans = 0




def main(config_path):
    m = QNAPModelConfig()
    with open(config_path, 'r') as config_fp:
        #sys.stderr.write(config_path + "\n")
        ini = configparser.ConfigParser()
        try:
            ini.read_string(config_fp.read().lower())
        except Exception as ex:
            #sys.stderr.write(f"Failed to parse config '{config_path}' - '{ex}'")
            return  None
        m.path = config_path
        #print(config_path)
        if ("system enclosure" not in ini) or ("sio_device" not in ini["system enclosure"]) or ini["system enclosure"]["sio_device"] != "it8528":
            return None
        m.model_name = ini["system enclosure"]["model"]
        m.fixed_name = ini["system enclosure"]["display_fixed_model_name"] if "display_fixed_model_name" in ini["system enclosure"] else ""
        m.mb_code = config_path.split("_")[1]
        m.bp_code = config_path.split("_")[2]
        m.num_disks = int(ini["system enclosure"]["max_disk_num"])
        m.ac_recovery = True if "pwr_recovery_unit" in ini["system enclosure"] and ini["system enclosure"]["pwr_recovery_unit"] == "ec" else False
        m.eup_mode = True if "eup_status" in ini["system enclosure"] and ini["system enclosure"]["eup_status"] == "ec" else False

        m.serial_location = ini["system enclosure"]["board_sn_device"] if "board_sn_device" in ini["system enclosure"] and ini["system enclosure"]["board_sn_device"] in ["vpd", "vpd_mb", "vpd_bp"] else ""

        m.button_copy = True if "usb_copy_button" in ini["system io"] and ini["system io"]["usb_copy_button"] == "ec" else False
        m.button_reset = True if "reset_button" in ini["system io"] and ini["system io"]["reset_button"] == "ec" else False
        m.button_chassis_open = True if "chassis_open" in ini["system io"] and ini["system io"]["chassis_open"] == "ec" else False

        m.led_brightness = True if "led_bv_interface" in ini["system io"] and ini["system io"]["led_bv_interface"] == "ec" and "led_bv_ctrl" in ini["system io"] and ini["system io"]["led_bv_ctrl"] == "pwm" else False
        m.teng_led = True if "10g_led" in ini["system io"] and ini["system io"]["10g_led"] == "ec" else False
        m.front_usb_led = True if "front_usb_led" in ini["system io"] and ini["system io"]["front_usb_led"] == "ec" else False
        m.jbod_connect_led = True if "jbod_connect_led" in ini["system io"] and ini["system io"]["jbod_connect_led"] == "ec" else False
        m.locate_led = True if "locate_led" in ini["system io"] and ini["system io"]["locate_led"] == "ec" else False
        m.status_led = True if "status_green_led" in ini["system io"] and ini["system io"]["status_green_led"] == "ec" and  "status_red_led" in ini["system io"] and ini["system io"]["status_red_led"] == "ec" else False

        #print(m.model_name)
        m.max_fans = tmp_max_fans = (int(ini["system enclosure"]["max_fan_num"]) if "max_fan_num" in ini["system enclosure"] else 0) + (int(ini["system enclosure"]["max_cpu_fan_num"]) if "max_cpu_fan_num" in ini["system enclosure"] else 0) + (int(ini["system fan region 1"]["max_fan_num"]) if "system fan region 1" in ini else 0) + (int(ini["system fan region 2"]["max_fan_num"]) if "system fan region 2" in ini else 0)
        fans = []
        if "cpu fan" in ini:
            for i in range(tmp_max_fans):
                if ini["cpu fan"]["fan_unit"] == "ec" and "fan_%d" % i in ini["cpu fan"]:
                    if ini["cpu fan"]["fan_%d" % i].startswith("i"):
                        fans.append(int(ini["cpu fan"]["fan_%d" % i][1:]))
                        #print("cpu %d" % int(ini["cpu fan"]["fan_%d" % i][1:]))
                    else:
                        raise Exception("Bad fan")
        if "system fan" in ini:
            for i in range(50):
                if ini["system fan"]["fan_unit"] == "ec" and "fan_%d" % i in ini["system fan"]:
                    if ini["system fan"]["fan_%d" % i].startswith("i"):
                        fans.append(int(ini["system fan"]["fan_%d" % i][1:]))
                        #print("system %d" % int(ini["system fan"]["fan_%d" % i][1:]))
                    else:
                        raise Exception("Bad fan")
        if "system fan region 1" in ini:
            for i in range(50):
                if ini["system fan region 1"]["fan_unit"] == "ec" and "fan_%d" % i in ini["system fan region 1"]:
                    if ini["system fan region 1"]["fan_%d" % i].startswith("i"):
                        fans.append(int(ini["system fan region 1"]["fan_%d" % i][1:]))
                        #print("reg 1 %d" % int(ini["system fan region 1"]["fan_%d" % i][1:]))
                    else:
                        raise Exception("Bad fan")
        if "system fan region 2" in ini:
            for i in range(50):
                if ini["system fan region 2"]["fan_unit"] == "ec" and "fan_%d" % i in ini["system fan region 2"]:
                    if ini["system fan region 2"]["fan_%d" % i].startswith("i"):
                        fans.append(int(ini["system fan region 2"]["fan_%d" % i][1:]))
                        #print("reg 2 %d" % int(ini["system fan region 2"]["fan_%d" % i][1:]))
                    else:
                        raise Exception("Bad fan")
        m.fans = fans
        if len(m.fans) < m.max_fans or len(m.fans) > m.max_fans + 1:
            print(config_path)
            raise Exception("Bad fan nums %d %d" % (len(m.fans), m.max_fans))
        for i in range(1, int(ini["system enclosure"]["max_disk_num"]) + 1):
            ds = ini[f"system disk {i}"]
            d = QNAPDiskSlot()
            d.name = ds["slot_name"].replace(" ", "").replace(".", "").replace("disk", "hdd") if "slot_name" in ds else ""
            d.blink_led = (i if ds["blink_led"] == "ec" else int(ds["blink_led"].split(":")[-1]) if ds["blink_led"].startswith("ec:") else 0) if "blink_led" in ds else 0
            d.error_led = (i if ds["err_led"] == "ec" else int(ds["err_led"].split(":")[-1]) if ds["err_led"].startswith("ec:") else 0) if "err_led" in ds else 0
            d.present_led = (i if ds["present_led"] == "ec" else int(ds["present_led"].split(":")[-1]) if ds["present_led"].startswith("ec:") else 0) if "present_led" in ds else 0
            d.locate_led = (i if ds["locate_led"] == "ec" else int(ds["locate_led"].split(":")[-1]) if ds["locate_led"].startswith("ec:") else 0) if "locate_led" in ds else 0
            #d.has_power_control = int(ds["slot_power_control"].split(":")[-1]) if "slot_power_control" in ds and ds["slot_power_control"].startswith("ec:") else 0

            if ({d.blink_led, d.error_led, d.present_led, d.locate_led} == {0}):
                #print(f"{m.model_name.upper()} has bad disk config! {d.blink_led},{d.error_led},{d.present_led},{d.locate_led}")
                continue
            if (d.present_led == 0 or d.error_led == 0):
                m.slot_blink_problems = True


            m.disk_slots.append(d)
        return m

def create_struct(m):
    config_template = """\t{
        "%s", "%s", "%s",
        {
            .pwr_recovery   = %d,
            .eup_mode       = %d,
            .led_brightness = %d,
            .led_status     = %d,
            .led_10g        = %d,
            .led_usb        = %d,
            .led_jbod       = %d,
            .led_ident      = %d,
            .enc_serial_mb  = %d,
            .vpd_bp_table   = 1,
        },
        .fans = (u8[])%s
        .slots = (struct qnap8528_slot_config[]){%s
            { NULL }
        }
    },"""
    def create_disks(d):
        disk_template = """\n\t\t\t{ .name = "%s", .ec_index = %d, .has_present = %d, .has_active = %d, .has_error = %d, .has_locate = %d},"""
        return disk_template % (d.name, max(d.present_led, d.blink_led, d.error_led, d.locate_led), 1 if d.present_led else 0, 1 if d.blink_led else 0, 1 if d.error_led else 0, 1 if d.locate_led else 0)
    return config_template % (
        m.model_name.upper(),
        m.mb_code,
        m.bp_code ,
        1 if m.ac_recovery else 0,
        1 if m.eup_mode else 0,
        1 if m.led_brightness else 0,
        1 if m.status_led else 0,
        1 if m.teng_led else 0,
        1 if m.front_usb_led else 0,
        1 if m.jbod_connect_led else 0,
        1 if m.locate_led else 0, 
        1 if m.serial_location == "vpd:mb" else 0,
        "{ " + ", ".join([str(f) for f in m.fans]) + ", 0},",
        "".join([create_disks(d) for d in m.disk_slots]))


def compare_qnap_configs(config1, config2):
    differences = []

    if (config1.bp_code != config2.bp_code) or (config1.mb_code != config2.mb_code):
        return False

    for attr in vars(config1):
        if attr == "path":
            continue
        value1 = getattr(config1, attr)
        value2 = getattr(config2, attr)

        # Special handling for 'disk_slots' attribute
        if attr == "disk_slots":
            if len(value1) != len(value2):
                differences.append(f"{attr} length: {len(value1)} != {len(value2)}")
            else:
                for i, (slot1, slot2) in enumerate(zip(value1, value2)):
                    for slot_attr in vars(slot1):
                        slot_value1 = getattr(slot1, slot_attr)
                        slot_value2 = getattr(slot2, slot_attr)
                        if slot_value1 != slot_value2:
                            differences.append(
                                f"{attr}[{i}].{slot_attr}: {slot_value1} != {slot_value2}"
                            )

        # General comparison for other attributes
        elif value1 != value2:
            differences.append(f"{attr}: {value1} != {value2}")

    if differences:
        print(f"Differences found ({config1.path} - {config2.path}):")
        for difference in differences:
            print("\t" + difference)

    return True if differences else False


if __name__ == "__main__":
    #if len(sys.argv) < 2:
    #    print(f"Usage: {os.path.basename(__file__)} <config file>")
    #    sys.exit(1)
    models = []
    for f in os.listdir("moreconfigs"):
        m = main("moreconfigs/" + f)  # Assuming `main()` loads a QNAPModelConfig instance
        if m:
            # Update model_name if fixed_name is not empty and different
            if m.fixed_name != m.model_name and m.fixed_name != "":
                m.model_name = m.fixed_name

            # Check if the model already exists in the list
            model_exists = False
            for em in models:
                if (em.model_name == m.model_name and em.mb_code == m.mb_code and em.bp_code == m.bp_code):
                #if em.model_name == m.model_name:
                    # If the model exists, compare with the existing model
                    if not compare_qnap_configs(em, m):
                        model_exists = True  # If they are the same, we don't need to append
                        break

            # Append the model only if it doesn't already exist
            if not model_exists:
                models.append(m)


    print("\n".join(create_struct(i) for i in sorted(models, key=lambda device: device.model_name)))
    #print("|Model Name|MB Code|BP Code|Disk LEDs|Notes\n|-|-|-|-|-|-|")
    #for x in models:
    #    print(f"|{x.model_name.upper()}|{x.mb_code}|{x.bp_code}|{len(x.disk_slots)}/{x.num_disks} | { '' if len(x.disk_slots) == x.num_disks else '⚠️ See *1 ' }{ '' if not x.slot_blink_problems  else '⚠️ See 2' } ") 
    #print(len(models))
    
