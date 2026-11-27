# Verification record and hardware follow-up

[Back to README](../README.md) · [Build instructions](BUILD_AND_RUN.md)

## Scope

Review date: **27 November 2026**. Original repository baseline: [`81d802210365757ba847fa165d7b237d982bb0b5`](https://github.com/Yasyous1/Gorilla-Turbo/commit/81d802210365757ba847fa165d7b237d982bb0b5). Local work branch: **`docs/portfolio-review-2026-11-27`**.

The repository was freshly cloned into the review workspace and was clean before changes. This does not inspect uncommitted work on the owner's laptop. No existing local edits were overwritten. No files were deleted or moved, no project license was added or changed, and no upload, push or publication was performed.

The review inspected all 18 original tracked files, all 19 original commits, the supplied competition rules and teammate example, the later project report and four recovered source snapshots. There was no repository `AGENTS.md`, build configuration, dependency lockfile, CI, recorded test suite, tag/release or media asset at the baseline.

## Real firmware builds

These were actual Arduino/Teensy ARM compilation and linking runs. No stub libraries or host-only syntax substitutes were used. The environment was Linux x86_64 with Arduino CLI **1.5.1**, PJRC **1.62.0**, ARM GCC **15.2.1**, and the unchanged camera headers from the checksum-pinned Pixy2 Arduino **1.0.3** archive. Target: **`teensy:avr:teensy41`**, default USB Serial / 600 MHz / Faster settings.

| Prepared alias | Result | Qualification |
| --- | --- | --- |
| `intersection` | PASS | Latest tracked source; full compile and link |
| `motors` | PASS | April 7 predecessor |
| `motors_apr05` | PASS | April 5 motor/steering revision |
| `camera_servo` | PASS | Camera/servo bench sketch |
| `camera_servo_v3` | FAIL | Existing undeclared `cameraServo` reference; retained as an archived defect |
| `servo_error` | PASS | Revised steering diagnostics |
| `servo_tuned` | PASS | Alternative steering calculation |
| `try2` | PASS | Build success does not resolve the documented obstacle/lost-line behavior |
| `corrected` | PASS | Different motor-interface pins; not a drop-in wiring match |
| `servo_calibration` | PASS | No hardware calibration performed |
| `day2` | PASS | After adding the missing plotting declaration and selecting it as a local include |

**Result: 10 of 11 distinct prepared firmware variants compile and link.** The identical duplicate camera sketch is not counted twice. The empty placeholder is not a build target. The four external recovered snapshots and the missing final firmware were not compiled as part of this matrix.

The reproducible command for the reference target is:

```bash
python3 scripts/prepare_sketch.py intersection
arduino-cli compile \
  --fqbn teensy:avr:teensy41 \
  --warnings all \
  --library build/libraries/Pixy2 \
  --build-path build/compiled/intersection \
  build/intersection
```

The review used an additional isolated `--config-file` pointing to the downloaded toolchain, rather than changing a user's existing Arduino installation. See [dependency setup](BUILD_AND_RUN.md#2-prepare-the-pixy-dependency) before reproducing the command.

### Observed failures and warnings

- The first build using the complete official Pixy ZIP failed in unused `ZumoBuzzer.cpp` on AVR-specific registers/interrupt syntax. Selecting its unchanged camera-driver headers resolved that dependency issue; the original ZIP and license notices were retained.
- Day 2 originally lacked `Serial_Plotting.h`. Adding the exact existing declaration and changing its include from angle brackets to quotes made the staged local helper discoverable. It still emits six existing string-literal-to-`char *` warnings. The original `long` conversion and integer telemetry formatting remain intact.
- `camera_servo_v3` fails because its source uses undeclared `cameraServo`, even inside an ordinary `if` whose flag is false. No runtime design decision was made on behalf of that archived experiment.
- Successful builds warned that Linux `00-teensy.rules` was absent. This did not block compilation; upload permissions were not configured or tested.

### Compiler memory report for `intersection`

```text
FLASH: code 21,376 bytes; data 5,400 bytes; headers 9,060 bytes
RAM1: variables 7,328 bytes; code 18,824 bytes; padding 13,944 bytes
RAM2: variables 12,416 bytes
```

These are compiler memory figures, not a runtime timing measurement or a driving-performance result.

## Preservation and repository checks

- Compared the original firmware/helper sources against the baseline: **12 of 13 are byte-identical**. The sole change in the remaining file is `#include <Serial_Plotting.h>` becoming `#include "Serial_Plotting.h"` in `day 2/main_day2.cpp`.
- Added a matching declaration header; no original runtime statement, pin, calibration value, control gain, timing constant or motor behavior changed.
- Checked prepared sources against their originals, including the plotting companions. All prepared copies match.
- Checked all five prepared Pixy headers against the official archive. Their content and notices match exactly; the archive checksum is unchanged.
- Exercised preparation reruns, unknown aliases, modified generated files, unexpected extra files and an incorrect dependency checksum. The helpers refused destructive replacement and left existing content intact.
- Checked documentation links and Git whitespace before handoff. Generated libraries, firmware copies and build products are excluded by `.gitignore`.
- After the initial review, added the supplied photograph and video without modifying their bytes, verified their SHA-256 hashes against the uploads, checked their image/video metadata and decoded the complete MP4 with FFmpeg without errors. This media update changes no firmware or build inputs, so it does not require repeating the unchanged compile matrix.

The original duplicate files, placeholders and old review notes are preserved and explained in [firmware history](FIRMWARE_HISTORY.md).

## Physical car verification

**Not performed:** USB flashing, electrical measurement, camera setup, servo calibration, wheel direction, control timing, track driving or obstacle tests. The following is a proposed short verification sequence, not a completed checklist.

| Step | Procedure | Record / acceptance evidence |
| --- | --- | --- |
| 1. Identify the actual firmware | Recover the exact source last flashed. Record its checksum/commit, toolchain and constants before changing anything. | A source-to-car link; no assumption based on filename or report alone. |
| 2. Inspect power and pins | Disconnect propulsion. Check regulated voltages, grounds, VIN/USB arrangement and ultrasonic level conversion against [hardware notes](HARDWARE.md). | Measured idle/load voltages, wiring photo and component labels. |
| 3. Check steering | With wheels lifted and propulsion disconnected, run calibration; verify center and correction direction again using the selected driving sketch. | Straight-wheel command, usable limits and steering response to left/right target shifts. |
| 4. Check propulsion and sensing | Lift the wheels before enabling motors. Confirm A/B wheel sides and directions; inspect Pixy vectors and serial diagnostics for straight, curved and single-border samples. | Correct wheel direction and observed target/error/servo/PWM values. |
| 5. Exercise missing data | While still lifted, remove lines and disconnect/disable camera communication in a controlled test. | For the current reference, document the existing continued-PWM behavior. Do not mark it as a safe stop. |
| 6. Test controlled track runs | After bench checks, use a clear test area with immediate power-disconnection access. Record each exact firmware/configuration, lighting, track geometry and battery condition. | Straight and left/right curve performance; repeatable crossings; lap completion/time only if actually measured. |
| 7. Verify stopping on the recovered final version | First check state transitions with wheels raised. Then measure sensor-to-box and bumper-to-box distances during controlled approach tests, including no-echo and lost-line conditions. | Final slowdown, no contact, measured stopping distance, persistent stop until the intended reset, and behavior on every failure path. This feature is absent from `intersection`. |

Do not use a fixed PWM command as a speed measurement. Do not report the 20 ms scheduling gate as measured execution time. If measuring either, record the method and raw observations.

Suggested test record fields: date, tester, source commit/hash, board/camera/library versions, servo center/limits, gains, loop setting, PWM settings, power measurements, camera mounting/settings, track/lighting, observed behavior, result, media link and any deviation from expected behavior. Keep failed runs as well as successful ones.

## Available demonstrations and result evidence

Youssef supplied a prototype photograph and track-demonstration video on **27 November 2026**, after the initial repository review. The original GitHub baseline had no media assets; the review branch now includes both files unchanged.

| Asset | File information | Evidence |
| --- | --- | --- |
| [Prototype photograph](../assets/media/gorilla-turbo-prototype.jpg) | JPEG, 1448 × 1086, 273,066 bytes | Assembled camera mast/mounts, front ultrasonic module, clear chassis plates, battery holders, controller board and wiring. Pin connections and electrical specifications are not established. |
| [Track demonstration](../assets/media/gorilla-turbo-track-demo.mp4) | MP4, 39.366667 seconds, 910 × 512, 30 fps, H.264 video/AAC audio, 24,996,204 bytes | Car travelling around the marked track and stopping before a white box near the end. No telemetry, firmware identifier or distance measurement is visible. |

### Observed sequence in the supplied video

Times are approximate positions within the clip, not measured controller timings:

- **Before approximately 32 s:** the magenta car travels along straight and curved parts of the marked track. Other people and another small vehicle are present in the scene.
- **Approximately 32–34 s:** a person places a white box on the track ahead, then steps clear.
- **Approximately 34–37 s:** the car rounds the right-hand bend and approaches the box.
- **Approximately 37–39 s:** it remains in essentially the same position relative to the box, with a visible gap. No collision is visible in this interval.

The footage provides direct visual evidence of track driving and a stop. It does **not** identify the flashed sketch, establish that the ultrasonic sensor caused the stop, prove finish-marker logic or a permanent stop latch, or measure the stopping gap. It does not independently establish the control mode or validate every autonomous behavior. The **39.37-second clip duration is not a lap time**. No repeatability rate or measured physical speed can be claimed from this single clip.

The recording dates are unconfirmed. The MP4 container has a creation timestamp on 27 November 2026; this may reflect export or transfer and is not used to date the driving test. The relationship to the demonstration mentioned in May 2026 is also unconfirmed. The later `ppp.pdf` report still provides no measured lap times, success rate or stopping-distance distribution; its validation table contains criteria and its Yes/No worksheet is unfilled.

### Original-media integrity

The repository copies retain the uploaded content, including the video audio. Only the filenames were made descriptive:

```text
gorilla-turbo-prototype.jpg
SHA-256: bcd3fb34c789b66735e8ae9819abfcc572cab6c19d773b2eab739ee784f3bfa0

gorilla-turbo-track-demo.mp4
SHA-256: c3add4c19817ea31a463339cdc4efca8109686d3accbac94527be602fdd52acd
```

The next portfolio evidence should identify the source used in this video and add an annotated wiring record and measured track/stop tests. Do not label the April 7 reference as the demonstrated firmware until that link is established.
