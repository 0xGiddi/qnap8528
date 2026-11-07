"""Verify the POSTINIT retry path without a TrueNAS host."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class StartupTests(unittest.TestCase):
    def run_helper(self, fail_until):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            client = root / "midclt"
            client.write_text('''#!/bin/sh
count=0
test ! -f "$TEST_STATE" || count=$(cat "$TEST_STATE")
count=$((count + 1))
printf '%s' "$count" > "$TEST_STATE"
printf '%s\\n' "$*" >> "$TEST_ARGUMENTS"
test "$count" -gt "$TEST_FAIL_UNTIL"
''')
            client.chmod(0o755)
            sleeper = root / "sleep"
            sleeper.write_text("#!/bin/sh\nexit 0\n")
            sleeper.chmod(0o755)
            env = os.environ | {
                "PATH": str(root) + os.pathsep + os.environ["PATH"],
                "TEST_STATE": str(root / "attempts"),
                "TEST_ARGUMENTS": str(root / "arguments"),
                "TEST_FAIL_UNTIL": str(fail_until),
            }
            result = subprocess.run(
                ["/bin/sh", str(Path(__file__).with_name("start-app.sh")), "qnap-hardware"],
                env=env, capture_output=True, text=True, check=False,
            )
            return result, int((root / "attempts").read_text()), (root / "arguments").read_text().splitlines()

    def test_retries_until_apps_are_ready(self):
        result, attempts, arguments = self.run_helper(2)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(attempts, 3)
        self.assertEqual(arguments, ["call -j app.start qnap-hardware"] * 3)

    def test_reports_failure_after_bounded_retries(self):
        result, attempts, _ = self.run_helper(99)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(attempts, 12)
        self.assertIn("Unable to start qnap-hardware", result.stderr)


if __name__ == "__main__":
    unittest.main()
