#!/usr/bin/env python3
"""Stage unchanged Pixy2 1.0.3 camera headers for a Teensy Arduino build.

The official ZIP also bundles AVR-only Zumo example implementations. Arduino
compiles these .cpp files even when the car does not use them. This helper copies
the camera driver's complete header dependency set into an isolated library;
the downloaded ZIP and any installed libraries remain untouched.
"""

import argparse
import hashlib
from pathlib import Path
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE_URL = (
    "https://github.com/charmedlabs/pixy2/raw/master/releases/arduino/"
    "arduino_pixy2-1.0.3.zip"
)
ARCHIVE_SHA256 = "fc8659d63f770374f0f6ae36213bf909ee9a9e2a9ea1c4ce1e20e7e30959c910"
HEADERS = ("Pixy2.h", "TPixy2.h", "Pixy2Line.h", "Pixy2CCC.h", "Pixy2Video.h")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="official arduino_pixy2-1.0.3.zip")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "build" / "libraries" / "Pixy2",
        help="isolated library directory (default: repository/build/libraries/Pixy2)",
    )
    args = parser.parse_args()
    archive = args.archive.expanduser()
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != ARCHIVE_SHA256:
        raise ValueError(
            f"Unexpected Pixy2 ZIP SHA256: {digest}. "
            f"Expected {ARCHIVE_SHA256}. Use the tested archive from {SOURCE_URL}"
        )

    with zipfile.ZipFile(archive) as source:
        contents = {name: source.read("Pixy2/" + name) for name in HEADERS}
        # Keep a top-level license document if the verified archive has one.
        # The 1.0.3 camera headers themselves also retain their GPLv2 notices.
        for entry in source.namelist():
            path = Path(entry)
            if path.parent not in (Path("."), Path("Pixy2")):
                continue
            if path.stem.casefold() in ("license", "copying"):
                contents[path.name] = source.read(entry)

    destination = args.output_dir.expanduser().absolute()
    if destination.is_symlink():
        raise ValueError(f"Refusing a symlinked library directory: {destination}")
    if destination.exists():
        if not destination.is_dir():
            raise ValueError(f"Not a directory: {destination}")
        unexpected = {p.name for p in destination.iterdir()} - contents.keys()
        if unexpected:
            raise ValueError(
                f"Unexpected files in {destination}: {', '.join(sorted(unexpected))}. "
                "Choose a new --output-dir."
            )
        for name, data in contents.items():
            output = destination / name
            if output.is_symlink() or (
                output.exists() and (not output.is_file() or output.read_bytes() != data)
            ):
                raise ValueError(
                    f"Refusing to overwrite changed file: {output}. "
                    "Choose a new --output-dir."
                )

    destination.mkdir(parents=True, exist_ok=True)
    for name, data in contents.items():
        output = destination / name
        if not output.exists():
            with output.open("xb") as stream:
                stream.write(data)
    print(f"Source: {SOURCE_URL}\nSHA256: {digest}")
    print(f"Library: {destination}")
    print("Copied unchanged camera headers and their attribution; no files deleted.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
