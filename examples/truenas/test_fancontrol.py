"""Exercise cooling decisions and fault handling without hardware access."""

import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


class CoolingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.sysfs = root / "sys"
        self.platform = root / "platform"
        self.controller = self.platform / "qnap8528/hwmon/hwmon9"
        self.controller.mkdir(parents=True)
        for channel in [1, 7]:
            (self.controller / f"pwm{channel}").write_text("76")
        for channel in [1, 2, 7]:
            (self.controller / f"fan{channel}_input").write_text("1000")
        for number, name, temp in [(0, "k10temp", 49000), (1, "drivetemp", 33000)]:
            directory = self.sysfs / f"class/hwmon/hwmon{number}"
            directory.mkdir(parents=True)
            (directory / "name").write_text(name)
            (directory / "temp1_input").write_text(str(temp))
        env = {"QNAP_SYSFS": str(self.sysfs), "QNAP_PLATFORM": str(self.platform),
               "QNAP_CPU_PWM": "7", "QNAP_CASE_PWMS": "1", "QNAP_FANS": "1,2,7",
               "QNAP_STATUS": str(root / "status.json")}
        spec = importlib.util.spec_from_file_location("cooling", Path(__file__).with_name("fancontrol.py"))
        self.module = importlib.util.module_from_spec(spec)
        with patch.dict(os.environ, env):
            spec.loader.exec_module(self.module)

    def run_once(self):
        writes = []
        original = self.module.write_pwm

        def write(controller, channel, value):
            writes.append((channel, value))
            original(controller, channel, value)

        def finish(_seconds):
            self.module.stopping = True

        with patch.object(self.module, "write_pwm", side_effect=write), \
             patch.object(self.module.time, "monotonic", side_effect=[0, 40, 100, 100]), \
             patch.object(self.module.time, "sleep", side_effect=finish):
            self.module.main()
        return writes, json.loads(self.module.STATUS.read_text())

    def test_normal_cooling_and_shutdown_full_speed(self):
        writes, status = self.run_once()
        self.assertTrue(status["healthy"])
        self.assertIn((1, 115), writes)
        self.assertIn((7, 122), writes)
        self.assertEqual(writes[-2:], [(1, 255), (7, 255)])

    def test_hot_disk_demands_full_chassis_speed(self):
        (self.sysfs / "class/hwmon/hwmon1/temp1_input").write_text("105000")
        writes, status = self.run_once()
        self.assertTrue(status["healthy"])
        self.assertTrue(all(value == 255 for channel, value in writes if channel == 1))

    def test_missing_sensor_requests_full_speed(self):
        (self.sysfs / "class/hwmon/hwmon1/temp1_input").unlink()
        writes, status = self.run_once()
        self.assertFalse(status["healthy"])
        self.assertTrue(all(value == 255 for _, value in writes))

    def test_invalid_sensor_requests_full_speed(self):
        (self.sysfs / "class/hwmon/hwmon0/temp1_input").write_text("-1000")
        writes, status = self.run_once()
        self.assertFalse(status["healthy"])
        self.assertTrue(all(value == 255 for _, value in writes))

    def test_stopped_fan_requests_full_speed(self):
        (self.controller / "fan2_input").write_text("0")
        writes, status = self.run_once()
        self.assertFalse(status["healthy"])
        self.assertTrue(all(value == 255 for _, value in writes))


if __name__ == "__main__":
    unittest.main()
