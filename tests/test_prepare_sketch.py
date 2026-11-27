"""Exercise the public firmware-staging commands without board dependencies.

Run from the repository root with:
    python3 -m unittest discover -s tests -v

These checks stage source files only; they do not compile or upload firmware.
"""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PREPARE = ROOT / "scripts" / "prepare_sketch.py"

# This is the supported command-to-source contract, independent of importing
# the preparation helper. Existing commands must survive repository moves.
TARGETS = {
    "intersection": "firmware/track_following/track_following.ino",
    "motors": "archive/firmware/hell1/sketch_with_motors.cpp",
    "motors_apr05": "archive/firmware/helemwthakek.ino",
    "camera_servo": "archive/firmware/code_cam_servo.ino",
    "camera_servo_v3": "archive/firmware/code_cam_servov3.ino",
    "servo_error": "archive/firmware/test erreur-servo/test-erreur_servo.ino",
    "servo_tuned": "archive/firmware/meilleur-test-servo.ino",
    "try2": "archive/firmware/TRY2.ino",
    "corrected": "archive/firmware/nxp_cup_corrected.ino",
    "servo_calibration": "firmware/calibration/servo_calibration/servo_calibration.ino",
    "day2": "archive/firmware/day 2/main_day2.cpp",
}


class PrepareSketchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="gorilla-staging-test-")
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name) / "sketches"

    def prepare(self, *arguments):
        return subprocess.run(
            [sys.executable, str(PREPARE), *arguments, "--output-root", str(self.output)],
            cwd=self.temporary.name,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_every_existing_alias_stages_the_intended_source_and_companions(self):
        for alias, source in TARGETS.items():
            with self.subTest(alias=alias):
                result = self.prepare(alias)
                self.assertEqual(result.returncode, 0, result.stderr)
                sketch = self.output / alias
                expected = {f"{alias}.ino": (ROOT / source).read_bytes()}
                if alias == "day2":
                    for name in ("Serial_Plotting.cpp", "Serial_Plotting.h"):
                        expected[name] = (ROOT / "archive/firmware/day 2" / name).read_bytes()
                actual = {p.name: p.read_bytes() for p in sketch.iterdir()}
                self.assertEqual(actual, expected)

    def test_list_reports_all_supported_commands_without_creating_sketches(self):
        result = self.prepare("--list")
        self.assertEqual(result.returncode, 0, result.stderr)
        listed = {}
        for line in result.stdout.splitlines():
            if line and not line.startswith(" "):
                alias, source = line.split(": ", 1)
                listed[alias] = source
        self.assertEqual(listed, TARGETS)
        self.assertFalse(self.output.exists())

    def test_a_variant_must_be_selected_explicitly(self):
        result = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("choose a variant explicitly", result.stderr)
        self.assertFalse(self.output.exists())

    def test_changed_prepared_source_is_preserved(self):
        self.assertEqual(self.prepare("intersection").returncode, 0)
        prepared = self.output / "intersection/intersection.ino"
        edited = prepared.read_bytes() + b"\n// Local tuning work\n"
        prepared.write_bytes(edited)

        result = self.prepare("intersection")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing to overwrite changed file", result.stderr)
        self.assertEqual(prepared.read_bytes(), edited)

    def test_extra_sketch_source_is_preserved_and_rejected(self):
        self.assertEqual(self.prepare("intersection").returncode, 0)
        unexpected = self.output / "intersection/other_experiment.ino"
        contents = b"void setup() {}\nvoid loop() {}\n"
        unexpected.write_bytes(contents)

        result = self.prepare("intersection")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unexpected files", result.stderr)
        self.assertEqual(unexpected.read_bytes(), contents)

    def test_symlinked_sketch_folder_cannot_redirect_output(self):
        self.output.mkdir()
        target = Path(self.temporary.name) / "elsewhere"
        target.mkdir()
        (self.output / "intersection").symlink_to(target, target_is_directory=True)

        result = self.prepare("intersection")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing a symlinked sketch folder", result.stderr)
        self.assertEqual(list(target.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
