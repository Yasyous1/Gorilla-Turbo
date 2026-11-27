#!/usr/bin/env python3
"""Stage one historical firmware variant as an isolated Arduino sketch."""

import argparse
import hashlib
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]

# The source paths are kept intact so that existing links and history still work.
# .cpp and extensionless standalone programs are staged as .ino for Arduino's
# normal include/prototype generation. No source text is rewritten.
VARIANTS = {
    "intersection": (
        "hell1/better_intersection",
        "Latest tracked intersection experiment; hardware validation pending",
        (),
    ),
    "motors": (
        "hell1/sketch_with_motors.cpp",
        "April 7 camera, steering and L298N motor integration",
        (),
    ),
    "motors_apr05": (
        "helemwthakek.ino",
        "April 5 camera, steering and L298N motor integration",
        (),
    ),
    "camera_servo": (
        "code_cam_servo.ino",
        "Camera and steering bench experiment; no motor control",
        (),
    ),
    "camera_servo_v3": (
        "code_cam_servov3.ino",
        "Archived camera/servo experiment; undeclared cameraServo blocks build",
        (),
    ),
    "servo_error": (
        "test erreur-servo/test-erreur_servo.ino",
        "Camera/steering experiment with adaptive filtering",
        (),
    ),
    "servo_tuned": (
        "meilleur-test-servo.ino",
        "Camera/steering experiment with heading and camera-offset terms",
        (),
    ),
    "try2": (
        "TRY2.ino",
        "Earlier motor, ultrasonic and finish-line state-machine experiment",
        (),
    ),
    "corrected": (
        "nxp_cup_corrected.ino",
        "Earlier motor variant with a different pin mapping",
        (),
    ),
    "servo_calibration": (
        "day 2/servo_calibration.cpp",
        "Standalone serial-controlled steering calibration utility",
        (),
    ),
    "day2": (
        "day 2/main_day2.cpp",
        "Earlier angle-based control and ultrasonic experiment",
        ("day 2/Serial_Plotting.cpp", "day 2/Serial_Plotting.h"),
    ),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("variant", nargs="?", choices=VARIANTS)
    parser.add_argument("--list", action="store_true", help="show source mapping")
    parser.add_argument(
        "--output-root",
        type=Path,
        default=ROOT / "build",
        help="parent of the generated sketch folder (default: repository/build)",
    )
    args = parser.parse_args()

    if args.list:
        for name, (source, description, _) in VARIANTS.items():
            print(f"{name}: {source}\n  {description}")
        return 0
    if args.variant is None:
        parser.error("choose a variant explicitly, or use --list")

    source, _, companions = VARIANTS[args.variant]
    destination = args.output_root.expanduser().absolute() / args.variant
    sources = [(source, f"{args.variant}.ino")]
    sources.extend((path, Path(path).name) for path in companions)
    contents = {name: (ROOT / path).read_bytes() for path, name in sources}

    # A prepared sketch may be opened in the IDE. Do not overwrite edits there,
    # or silently compile extra files left by a different experiment.
    if destination.is_symlink():
        raise ValueError(f"Refusing a symlinked sketch folder: {destination}")
    if destination.exists():
        if not destination.is_dir():
            raise ValueError(f"Not a directory: {destination}")
        unexpected = {p.name for p in destination.iterdir()} - contents.keys()
        if unexpected:
            raise ValueError(
                f"Unexpected files in {destination}: {', '.join(sorted(unexpected))}. "
                "Choose a new --output-root."
            )
        for name, data in contents.items():
            output = destination / name
            if output.is_symlink() or (
                output.exists() and (not output.is_file() or output.read_bytes() != data)
            ):
                raise ValueError(
                    f"Refusing to overwrite changed file: {output}. "
                    "Choose a new --output-root."
                )

    destination.mkdir(parents=True, exist_ok=True)
    for path, name in sources:
        output = destination / name
        if not output.exists():
            with output.open("xb") as stream:
                stream.write(contents[name])
        digest = hashlib.sha256(contents[name]).hexdigest()
        print(f"Source: {path}\nSHA256: {digest}")
    print(f"Sketch: {destination / (args.variant + '.ino')}")
    print("Only staged source files; nothing compiled or uploaded.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
