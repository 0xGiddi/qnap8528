"""Validate optional cooling and container boundaries without a Docker daemon."""

import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path


class ComposeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("docker"):
            raise unittest.SkipTest("Docker Compose CLI not installed")
        result = subprocess.run(
            ["docker", "compose", "version"], capture_output=True, timeout=15
        )
        if result.returncode:
            raise unittest.SkipTest("Docker Compose CLI not installed")

    def render(self, files, cooling=False):
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith("QNAP_")
        }
        env.update(
            QNAP_SOURCE_DIR="/mnt/test/source", QNAP_OUTPUT_DIR="/mnt/test/modules"
        )
        if cooling:
            env.update(QNAP_CPU_PWM="7", QNAP_CASE_PWMS="1", QNAP_FANS="1,2,7")
        command = ["docker", "compose", "--env-file", os.devnull]
        for filename in files:
            command.extend(["-f", str(Path(__file__).with_name(filename))])
        return subprocess.run(
            command + ["config", "--format", "json"],
            env=env,
            capture_output=True,
            text=True,
            timeout=15,
        )

    def test_build_load_needs_no_cooling_variables(self):
        result = self.render(["compose.yaml"])
        self.assertEqual(result.returncode, 0, result.stderr)
        services = json.loads(result.stdout)["services"]
        self.assertEqual(set(services), {"build", "load"})
        loader = services["load"]
        self.assertTrue(loader["read_only"])
        self.assertEqual(loader["network_mode"], "none")
        self.assertEqual(loader["cap_drop"], ["ALL"])
        self.assertEqual(loader["cap_add"], ["SYS_MODULE"])
        self.assertIn("no-new-privileges:true", loader["security_opt"])

    def test_optional_cooling_requires_explicit_channels(self):
        result = self.render(["compose.cooling.yaml"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("QNAP_CPU_PWM", result.stderr)

    def test_cooling_only_writes_qnap_device(self):
        result = self.render(["compose.cooling.yaml"], cooling=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        services = json.loads(result.stdout)["services"]
        self.assertEqual(set(services), {"cooling"})
        cooling = services["cooling"]
        writable = [mount for mount in cooling["volumes"] if not mount.get("read_only")]
        self.assertEqual(len(writable), 1)
        self.assertEqual(writable[0]["source"], "/sys/devices/platform/qnap8528")
        self.assertEqual(writable[0]["target"], "/controller")
        self.assertFalse(writable[0]["bind"]["create_host_path"])
        self.assertTrue(cooling["read_only"])
        self.assertEqual(cooling["network_mode"], "none")
        self.assertEqual(cooling["cap_drop"], ["ALL"])
        self.assertFalse(cooling.get("cap_add"))
        self.assertIn("no-new-privileges:true", cooling["security_opt"])

    def test_optional_file_can_be_merged(self):
        result = self.render(["compose.yaml", "compose.cooling.yaml"], cooling=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            set(json.loads(result.stdout)["services"]), {"build", "load", "cooling"}
        )


if __name__ == "__main__":
    unittest.main()
