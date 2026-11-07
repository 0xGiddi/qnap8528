"""Verify the POSTINIT retry path without a TrueNAS host."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class StartupTests(unittest.TestCase):
    def run_helper(self, fail_until, apps=("qnap-hardware",), hwmon="ready"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sysfs = root / "sys"
            if hwmon == "ready":
                (sysfs / "module/qnap8528").mkdir(parents=True)
                (sysfs / "devices/platform/qnap8528/hwmon/hwmon9").mkdir(parents=True)
            client = root / "midclt"
            client.write_text("""#!/bin/sh
count=0
test ! -f "$TEST_STATE" || count=$(cat "$TEST_STATE")
count=$((count + 1))
printf '%s' "$count" > "$TEST_STATE"
printf '%s\\n' "$*" >> "$TEST_ARGUMENTS"
test "$count" -gt "$TEST_FAIL_UNTIL"
""")
            client.chmod(0o755)
            sleeper = root / "sleep"
            sleeper.write_text(
                '#!/bin/sh\nif [ "$TEST_HWMON" = late ]; then\n'
                'mkdir -p "$QNAP_SYSFS/module/qnap8528" '
                '"$QNAP_SYSFS/devices/platform/qnap8528/hwmon/hwmon9"\nfi\n'
            )
            sleeper.chmod(0o755)
            env = os.environ | {
                "PATH": str(root) + os.pathsep + os.environ["PATH"],
                "TEST_STATE": str(root / "attempts"),
                "TEST_ARGUMENTS": str(root / "arguments"),
                "TEST_FAIL_UNTIL": str(fail_until),
                "QNAP_SYSFS": str(sysfs),
                "TEST_HWMON": hwmon,
            }
            result = subprocess.run(
                ["/bin/sh", str(Path(__file__).with_name("start-app.sh")), *apps],
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            return (
                result,
                int((root / "attempts").read_text()),
                (root / "arguments").read_text().splitlines(),
            )

    def test_retries_until_apps_are_ready(self):
        result, attempts, arguments = self.run_helper(2)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(attempts, 3)
        self.assertEqual(arguments, ["call -j app.start qnap-hardware"] * 3)

    def test_reports_failure_after_bounded_retries(self):
        result, attempts, arguments = self.run_helper(
            99, ("qnap-hardware", "qnap-cooling")
        )
        self.assertEqual(result.returncode, 1)
        self.assertEqual(attempts, 12)
        self.assertIn("Unable to start qnap-hardware", result.stderr)
        self.assertTrue(all("qnap-cooling" not in arg for arg in arguments))

    def test_starts_optional_cooling_only_after_loader_succeeds(self):
        result, attempts, arguments = self.run_helper(
            2, ("qnap-hardware", "qnap-cooling")
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(attempts, 4)
        self.assertEqual(
            arguments,
            ["call -j app.start qnap-hardware"] * 3
            + ["call -j app.start qnap-cooling"],
        )

    def test_defaults_to_build_load_app(self):
        result, attempts, arguments = self.run_helper(0, ())
        self.assertEqual(result.returncode, 0)
        self.assertEqual(attempts, 1)
        self.assertEqual(arguments, ["call -j app.start qnap-hardware"])

    def test_waits_for_module_after_app_start_job_succeeds(self):
        result, attempts, arguments = self.run_helper(
            0, ("qnap-hardware", "qnap-cooling"), hwmon="late"
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(attempts, 2)
        self.assertEqual(arguments[-1], "call -j app.start qnap-cooling")

    def test_successful_app_job_with_missing_module_does_not_start_cooling(self):
        result, attempts, arguments = self.run_helper(
            0, ("qnap-hardware", "qnap-cooling"), hwmon="missing"
        )
        self.assertEqual(result.returncode, 1)
        self.assertEqual(attempts, 1)
        self.assertEqual(arguments, ["call -j app.start qnap-hardware"])
        self.assertIn("check the loader logs", result.stderr)


if __name__ == "__main__":
    unittest.main()
