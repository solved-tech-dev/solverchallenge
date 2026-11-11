#!/usr/bin/env python3
"""Exercise process isolation and verification across the public run workflow."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("summary", ROOT / "scripts/summarize_results.py")
SUMMARY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUMMARY)


class IndependentRoundsTests(unittest.TestCase):
    def test_product_licenses_are_installed_as_private_regular_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for product in ("pangulu", "linglongamg", "xianzhikrylov"):
                (directory / "packages" / product).mkdir(parents=True)
                source = directory / "licenses" / f"{product}-test.key"
                source.parent.mkdir(exist_ok=True)
                source.write_text("test license placement\n")
            command = ["bash", "-c", 'source "$1"', "bash", str(ROOT / "scripts/load_products.sh")]
            import os
            environment = os.environ.copy()
            environment["SC26_LOCAL_ROOT"] = str(directory)
            for name in ("PANGULU", "LINGLONGAMG", "XIANZHIKRYLOV"):
                environment.pop(f"{name}_LICENSE_PATHS", None)
                environment.pop(f"{name}_ROOT", None)
            subprocess.run(command, env=environment, check=True)
            for product in ("pangulu", "linglongamg", "xianzhikrylov"):
                destination = directory / "packages" / product / "licenses" / f"{product}-test.key"
                self.assertFalse(destination.is_symlink())
                self.assertEqual(destination.read_text(), "test license placement\n")
                self.assertEqual(destination.stat().st_mode & 0o777, 0o600)

    def run_workflow(self, directory, failure=None):
        driver = directory / "driver.py"
        driver.write_text(
            "import json, os\n"
            "from pathlib import Path\n"
            "trial = int(os.environ['SC26_TRIAL_INDEX'])\n"
            "assert not Path('state').exists()\n"
            "Path('state').write_text('previous solve')\n"
            "Path('identity.json').write_text(json.dumps([os.getpid(), os.getcwd(), os.environ['TMPDIR']]))\n"
            "print(f'TRIAL trial={trial} raw_seconds={trial + 1}')\n"
            "print('XianzhiKrylov status=1 converged=1 iterations=7')\n"
            f"failed = trial == {failure!r}\n"
            "print('VERIFICATION relative_l2=1e-9 threshold=1e-8 ' + ('FAIL' if failed else 'PASS'))\n"
            "raise SystemExit(1 if failed else 0)\n"
        )
        process = subprocess.run(
            [sys.executable, str(ROOT / "scripts/run_independent_rounds.py"),
             sys.executable, str(driver)],
            cwd=directory, capture_output=True, text=True,
        )
        log = directory / "run.log"
        log.write_text(process.stdout + process.stderr)
        return process, SUMMARY.summarize_log(log)

    def test_ten_distinct_processes_and_last_five_mean(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            process, result = self.run_workflow(directory)
            self.assertEqual(process.returncode, 0)
            self.assertEqual(result["status"], "PASS")
            self.assertEqual(result["time_ms"], 8000.0)
            identities = [json.loads(path.read_text()) for path in directory.glob("rounds/*/identity.json")]
            for column in range(3):
                self.assertEqual(len({row[column] for row in identities}), 10)

    def test_earlier_failed_verification_is_not_hidden(self):
        with tempfile.TemporaryDirectory() as temporary:
            process, result = self.run_workflow(Path(temporary), failure=7)
            self.assertNotEqual(process.returncode, 0)
            self.assertEqual(result["status"], "FAIL")

    def test_all_hundred_rhs_checks_are_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            log = Path(temporary) / "run.log"
            lines = []
            for trial in range(10):
                lines += [f"ROUND_BEGIN trial={trial}", f"TRIAL trial={trial} raw_seconds=2.0"]
                lines += [f"RESIDUAL rhs={rhs} relative_l2=2e-8 absolute_max=7e-9 PASS" for rhs in range(1, 11)]
                lines += [f"ROUND_END trial={trial} exit_code=0"]
            log.write_text("\n".join(lines))
            self.assertEqual(SUMMARY.summarize_log(log)["status"], "PASS")
            log.write_text(log.read_text().replace("RESIDUAL rhs=4 relative_l2=2e-8 absolute_max=7e-9 PASS", "", 1))
            self.assertEqual(SUMMARY.summarize_log(log)["status"], "FAIL")


if __name__ == "__main__":
    unittest.main()
