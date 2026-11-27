# Build, upload, calibrate and operate

[Back to README](../README.md) · [Hardware](HARDWARE.md) · [Verification results](VALIDATION.md)

Use the **`docs/portfolio-review-2026-11-27`** branch for this organized layout; `main` is unchanged. The repository uses Arduino C++ with PJRC's Teensy support. The development baseline is [`firmware/track_following/track_following.ino`](../firmware/track_following/track_following.ino); the calibration tool has its own sketch directory. Earlier experiments live under `archive/firmware/` and remain separate programs. The preparation scripts select one program and its companions without combining multiple `setup()` and `loop()` functions.

## Previously verified tools and dependencies

The following combination was used for real Teensy 4.1 compilation during the earlier review on 27 November 2026. The original development tool versions were not recorded. These are recorded build inputs, not a reconstruction of the team's original installation or a fresh toolchain verification during the directory reorganization.

| Component | Verified version / configuration |
| --- | --- |
| Arduino CLI | 1.5.1 |
| PJRC board package | `teensy:avr@1.62.0` |
| Target | `teensy:avr:teensy41` |
| Board defaults used | USB Serial, 600 MHz, Faster/O2, US English |
| ARM compiler | GCC 15.2.1, Arm GNU Toolchain 15.2.Rel1, installed with the board package |
| Pixy2 Arduino library | Official `arduino_pixy2-1.0.3.zip`, camera-driver headers prepared as below |
| PWMServo / Servo / SPI | 2.1 / 1.1.2 / 1.0, bundled with the PJRC package |
| Python | 3.12.14 tested; scripts use the standard library |
| PixyMon and camera firmware | Required for physical camera setup; historical versions/settings not recorded and not exercised in this review |

Arduino IDE is an alternative front end to the same Teensy toolchain. The IDE GUI and hardware upload were not tested here. Official references: [Arduino CLI installation](https://docs.arduino.cc/arduino-cli/installation/) and [PJRC Teensy installation](https://www.pjrc.com/teensy/td_download.html).

## 1. Install the board package

Install Arduino CLI 1.5.1 from its official release for your operating system and make `arduino-cli` available in the terminal. From the repository root:

```bash
arduino-cli version
arduino-cli core update-index \
  --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core install teensy:avr@1.62.0 \
  --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core list
```

These commands do not require replacing an existing Arduino configuration, but `core install` writes to the CLI's configured data directory. To retain a separate known-working Teensy installation, use an isolated CLI configuration/data directory when comparing versions. The repository review used that arrangement.

## 2. Prepare the Pixy dependency

The official 1.0.3 ZIP bundles camera headers together with Zumo robot implementations. In the tested Teensy 4.1 build, Arduino also compiled unused AVR-specific `ZumoBuzzer.cpp`, causing errors for AVR timer registers and interrupts. The car does not use those sources.

The preparation helper copies the five unchanged headers required by `Pixy2.h` into an isolated library, retaining their copyright/license notices. It does not modify the downloaded ZIP, remove an installed library or patch the driver logic.

```bash
mkdir -p build/downloads
curl -fL \
  https://github.com/charmedlabs/pixy2/raw/master/releases/arduino/arduino_pixy2-1.0.3.zip \
  -o build/downloads/arduino_pixy2-1.0.3.zip
python3 scripts/prepare_pixy2.py build/downloads/arduino_pixy2-1.0.3.zip
```

You can also download the ZIP in a browser and pass its path to the same helper. The expected archive SHA-256 is enforced by the script:

```text
fc8659d63f770374f0f6ae36213bf909ee9a9e2a9ea1c4ce1e20e7e30959c910
```

Output: `build/libraries/Pixy2/`, containing `Pixy2.h`, `TPixy2.h`, `Pixy2Line.h`, `Pixy2CCC.h` and `Pixy2Video.h`. A different archive is refused rather than silently accepted as the tested dependency. Original distribution: [Charmed Labs Pixy2](https://github.com/charmedlabs/pixy2), also linked from [Pixy downloads](https://pixycam.com/downloads-pixy2/).

## 3. Select and compile one program

```bash
python3 scripts/prepare_sketch.py --list
python3 scripts/prepare_sketch.py intersection
arduino-cli compile \
  --fqbn teensy:avr:teensy41 \
  --warnings all \
  --library build/libraries/Pixy2 \
  --build-path build/compiled/intersection \
  build/intersection
```

The helper copies the selected source byte-for-byte to an Arduino-compatible directory/name and prints its SHA-256. For `.cpp` or extensionless standalone sources, the staged name has an `.ino` extension so Arduino supplies its normal sketch preprocessing. This is not a firmware conversion or a merge of variants.

| Alias | Current source path | Use |
| --- | --- | --- |
| `intersection` | `firmware/track_following/track_following.ino` | Latest tracked intersection experiment |
| `motors` | `archive/firmware/hell1/sketch_with_motors.cpp` | Preceding motor/steering integration |
| `motors_apr05` | `archive/firmware/helemwthakek.ino` | Earlier motor/steering tuning |
| `camera_servo` | `archive/firmware/code_cam_servo.ino` | Minimal camera/servo bench test |
| `camera_servo_v3` | `archive/firmware/code_cam_servov3.ino` | Archived incomplete experiment; known compile failure |
| `servo_error` | `archive/firmware/test erreur-servo/test-erreur_servo.ino` | Camera/servo diagnostics |
| `servo_tuned` | `archive/firmware/meilleur-test-servo.ino` | Alternative steering calculation |
| `try2` | `archive/firmware/TRY2.ino` | Earlier integrated obstacle/finish experiment |
| `corrected` | `archive/firmware/nxp_cup_corrected.ino` | Alternative motor pin/interface mapping |
| `servo_calibration` | `firmware/calibration/servo_calibration/servo_calibration.ino` | Interactive steering calibration |
| `day2` | `archive/firmware/day 2/main_day2.cpp` | Earlier integrated angle-based controller |

For another alias, replace `intersection` in both preparation and compile paths. `day2` also stages its plotting header and implementation. The archived duplicate camera sketch is not a separate target. The archived empty placeholder is not the source used by the `intersection` alias. See [the migration map](../archive/PATHS.json) for every previous source path.

### Build the primary sketch directly

The track-following entry point now has an Arduino-compatible folder and filename, so it can also be compiled directly after preparing the Pixy dependency:

```bash
arduino-cli compile \
  --fqbn teensy:avr:teensy41 \
  --library build/libraries/Pixy2 \
  firmware/track_following
```

This direct command is the new layout's intended build path. No fresh Teensy compilation was performed during the reorganization; the earlier successful build used the identical source bytes through the `intersection` alias. Keep the alias workflow when comparing any archived variants.

### Working on a staged sketch

The scripts reuse identical generated files but refuse to overwrite edits or mix in unexpected files. They do not delete anything. If you edited a staged sketch, retain that work and choose a new preparation directory:

```bash
python3 scripts/prepare_sketch.py intersection --output-root build/review2
```

Compile the resulting `build/review2/intersection` path. To make a firmware change permanent, apply the reviewed change to the source listed in the alias table on a development branch, then prepare a fresh directory. Record any calibration change in the associated hardware test.

## Optional Arduino IDE workflow

1. Install Arduino IDE and add the PJRC board-manager URL above; install Teensy package 1.62.0 and select **Teensy 4.1**.
2. Use a separate sketchbook for this project if an existing full Pixy installation conflicts. For example, create a new `Arduino-Gorilla` directory and prepare the library there:

   ```bash
   python3 scripts/prepare_pixy2.py build/downloads/arduino_pixy2-1.0.3.zip \
     --output-dir ~/Arduino-Gorilla/libraries/Pixy2
   ```

3. Select that sketchbook directory in IDE Preferences, retaining your previous setting for other projects. Reopen the IDE to discover the library.
4. Open `firmware/track_following/track_following.ino` or a prepared `.ino`, such as `build/intersection/intersection.ino`, and click **Verify**. Keep USB Type **Serial**, CPU **600 MHz** and optimization **Faster** to match the recorded CLI verification.

Use the CLI with the explicit `--library` path if you want to keep the IDE's current sketchbook configuration. There is no need to delete the original Pixy ZIP or another project's library.

## 4. Upload over USB

Upload requires the physical board. Before connecting USB, follow [the VIN/VUSB and power notes](HARDWARE.md#power-arrangement-and-checks), disconnect propulsion power and raise the wheels. The motor variants can command motion automatically after setup.

- In Arduino IDE, select the connected Teensy and click **Upload**. Use the Teensy's Program button when the loader requests it.
- For CLI use, discover the actual board/port with `arduino-cli board list`, then upload the previously compiled image. Replace `PORT_FROM_BOARD_LIST` with the detected value:

  ```bash
  arduino-cli upload \
    --fqbn teensy:avr:teensy41 \
    --port PORT_FROM_BOARD_LIST \
    --input-dir build/compiled/intersection \
    build/intersection
  ```

On Linux, install the [PJRC udev rules](https://www.pjrc.com/teensy/td_download.html) if board access is unavailable. This review environment lacked those rules and had no car connected; upload/discovery behavior is therefore an instruction to verify, not a completed test.

## 5. Camera and steering calibration

1. **Check power and wiring first.** With motor power disconnected, verify the regulated supply and camera/servo connections. The standalone calibration program does not actively drive motor enables LOW; physical disconnection is required for that bench test.
2. **Check camera output in PixyMon.** Select the line-tracking program and Arduino ICSP SPI/no-SS interface. Present black track boundaries on a light surface and check both sides, one side, a curve and a crossing. Record camera firmware, height, pitch, exposure/threshold settings and illumination. The reference sketch turns both lamps on; do not assume an old camera configuration remains suitable. Follow [Pixy's line quickstart](https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:line_quickstart).
3. **Find the mechanical steering center.** Prepare and upload `servo_calibration`; open Serial Monitor at 115200 and enter an angle followed by Enter. It starts at 84° and accepts 40–128°. Stay within the linkage's actual travel. Record the angle that straightens the wheels; the sketch does not save it automatically.
4. **Verify the selected driving revision.** Newer sketches use `PWMServo` while the calibration utility uses `Servo`. Verify the mechanical center again with the chosen driving firmware. Move a track sample left/right under the camera and confirm the steering correction reduces the observed lane error. Establish center and direction before adjusting gains.
5. **Record tuning systematically.** For a future tuning session, retain the original constants, change one parameter at a time on a separate revision, and log straight/curve behavior. Pixel-error gains cannot be copied directly from the older angle-error controller. The derivative is per-update; changing the loop interval changes its interpretation.

No calibration, gain, pin or timing value was changed by the repository reorganization. The calibration utility's 84° start and 40–128° limits are its historical settings; the track-following baseline retains a 105° center and 40–150° limits. Neither those values nor the report's 98° center establish the correct mechanical center for the current car.

## Host checks for the repository tools

Run the preparation checks from the repository root:

```bash
python3 -m unittest discover -s tests -v
```

These tests stage all 11 aliases and compare source bytes, including the Day 2 companions. They also check explicit variant selection and refusal to overwrite edited, unexpected or symlinked output. They do not compile firmware, communicate with the board or measure driving behavior. See [validation](VALIDATION.md#repository-reorganization-checks) for the current result.

## 6. Operating checks

With the wheels raised, confirm channel A/B wheel direction, steering response and the observed motor commands before a controlled track test. In `intersection`, Serial Monitor at 115200 reports `Lcnt`, `Rcnt`, `LaneW`, `Err`, `Servo` and `Speed`. The usual command is PWM 200 near center and approaches 180 for a large steering deviation.

Covering the camera in this revision should reveal its existing behavior: the last steering command is held and PWM 180 continues. It is not a stop test. Do not expect ultrasonic protection or a start-button interlock from this revision.

Fastest-lap development comes first: log complete laps, failures, steering/filter changes and curve-speed trials against an exact source revision. Obstacle stopping is deferred. When that bonus is revisited, first identify an actual source with the feature and check all its state transitions with the wheels raised. A saved 10/12/50 cm threshold or a report description does not establish final physical stopping distance. Use [the short physical verification procedure](VALIDATION.md#physical-car-verification) and record the exact commit for any demonstration.

## Troubleshooting the build

| Symptom | Established cause / next action |
| --- | --- |
| Redefined `setup()`, `loop()` or global objects | Multiple standalone programs were compiled together. Use one prepared sketch directory. |
| `Pixy2.h` not found | Prepare the dependency and pass the exact `--library build/libraries/Pixy2` path. |
| AVR registers such as `TIMSK2` or `TCCR2A` fail in `ZumoBuzzer.cpp` | A full Pixy distribution's unused Zumo sources are being compiled. Use the isolated prepared camera library. |
| `Serial_Plotting.h` not found | Use the updated Day 2 source and added local header; prepare the `day2` alias so both helper files are included. |
| `cameraServo` undeclared in v3 | Preserved historical defect. Choose a documented later variant rather than assuming `USE_CAMERA_SERVO=false` makes the file compile. |
| Preparation refuses changed output | The helper is preserving existing edits. Keep them and choose a new output directory. |
| Compile succeeds but upload cannot see the board | Verify data-capable USB cable, selected board/port, Linux rules and Teensy loader state. No upload was performed during this review. |
