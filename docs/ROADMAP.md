# Available material and development roadmap

[Back to README](../README.md) · [Architecture](ARCHITECTURE.md) · [Verification record](VALIDATION.md)

The development priority is **the fastest reliable lap**: track following, stable steering, curve handling and adaptive motor commands. Obstacle-stop recovery and integration are a deferred bonus. A missing item below is work still needed, not an implemented or measured result.

## What the repository already contains

| Area | Available material | Limit of the evidence |
| --- | --- | --- |
| Driving baseline | [`firmware/track_following/track_following.ino`](../firmware/track_following/track_following.ino): Pixy line vectors, boundary checks, lane-width memory, filtered PD steering and steering-dependent motor PWM | Latest tracked April 7 implementation; not identified as the software running in the supplied video |
| Calibration | [Standalone servo utility](../firmware/calibration/servo_calibration/servo_calibration.ino) and documented revision-specific constants | No current physical center/direction measurement; utility starts at 84°, driving baseline retains 105° |
| Experiments and attribution | [Archived programs](../archive/), [path map](../archive/PATHS.json), [firmware history](FIRMWARE_HISTORY.md) and original Git commits | Includes duplicates and a known incomplete variant; historical names do not prove a working release |
| Build workflow | One-program preparation, pinned Pixy archive checksum, recorded Teensy setup and host preparation checks | Earlier review recorded 10/11 firmware builds; no fresh compiler run is claimed for this reorganization |
| Hardware | [Component inventory](../hardware/README.md), [source-defined pin tables and reported power arrangement](HARDWARE.md) | Exact installed part specifications and complete traced wiring remain incomplete |
| Prototype evidence | [Photo](../assets/media/gorilla-turbo-prototype.jpg) and [39.37-second track video](../assets/media/gorilla-turbo-track-demo.mp4) | Recording dates, flashed firmware, lap times, physical speed and measured stopping distance are unknown |
| Engineering context | [Notes](ENGINEERING_NOTES.md) on power, calibration, line selection, missing data, build issues and mechanical constraints | Reported observations and source-level findings are distinct from reproduced hardware faults |

## Priority 1: establish the car and lap baseline

| Missing item | Why it matters for lap performance | Evidence needed to complete it |
| --- | --- | --- |
| Exact source currently flashed on the car | The committed baseline and the demonstrated car cannot yet be equated | Export the actual source and settings, record its hash/commit and identify the board/camera/library versions; alternatively select this baseline for a new, clearly identified test |
| Complete-lap and failure records | There is no measured fastest configuration to optimize against | Repeated runs with completion/failure, lap time when completed, track conditions, source/configuration and linked recording; keep failures |
| Physical steering center and correction direction | Misalignment can dominate gain tuning | Raised-wheel calibration followed by verification with the selected driving program; record center, usable limits and sign |
| Camera setup record | Height, pitch, lighting and line settings affect the target before PD is applied | Mount dimensions/angles, Pixy firmware/program/interface, relevant camera settings and dated track images |
| Loaded power and propulsion checks | Reset, torque and battery differences can invalidate comparisons | Actual wiring, regulated voltage under steering/motor load, motor-channel direction and battery condition for each run |

The supplied video is useful prototype evidence, but its duration is not a lap time and its source is not identified. Completing this baseline does not depend on recovering the deferred stopping implementation.

## Priority 2: improve track following and curve speed

| Work still needed | What already exists | Next concrete result |
| --- | --- | --- |
| Compare steering/filter settings systematically | PD control, adaptive target filtering, deadband and angle limits | A tuning log comparing repeated straight, left/right curve and complete-lap runs; change one parameter at a time |
| Tune steering-dependent motor commands | Baseline PWM falls from 200 to 180 with steering deviation | Recorded speed-profile trials showing their effect on completed laps and failures; PWM alone is not a physical speed measurement |
| Measure control timing | A requested 20 ms gate and serial diagnostics | Actual update durations and variability, including camera/serial overhead; retest any schedule change because the derivative is per update |
| Measure line and crossing behavior | Pair-consistency checks, single-boundary fallback and lane-width memory | Repeatable cases for both missing boundaries, curves, extra markings and crossings, with success/failure records |
| Define sustained missing-data behavior | Current firmware holds steering and continues at PWM 180 | An explicit camera-fault/lost-line/arming design, replay checks and raised-wheel tests before track use |

Do not choose new gains or copy values from another revision solely because they appear newer. The recovered April 8 external source described in [firmware history](FIRMWARE_HISTORY.md) used center 98°, `KP=0.95`, `KD=0.20` and PWM 200–170. Its source bytes are not committed here, and those values differ from the current baseline. Add the actual supplied source before evaluating it as a separate candidate; do not reconstruct a supposed final program from remembered constants.

## Supporting material still missing

| Area | Missing material | Destination once available |
| --- | --- | --- |
| Electrical design | Traced schematic, annotated connector/pin photos, verified motor-side mapping, actual level conversion and VIN/VUSB arrangement | `hardware/`, linked from [hardware notes](HARDWARE.md) |
| Installed components | Exact servo/motor/battery/regulator models, ratings, gearing and wheel dimensions | The existing [component inventory](../hardware/README.md), with evidence and unknowns updated |
| Mechanical design | CAD/STL, chassis/mount dimensions, material details and dated revisions | `hardware/` with the relevant prototype source/configuration |
| Reproducible runs | Raw lap/timing/tuning logs and associated configuration/source identifiers | A run-data directory when real records exist; no invented sample performance rows |
| Algorithm regression tests | Recorded/synthetic lane-vector cases and expected outputs from the chosen baseline | `tests/` after separating hardware-independent logic as described in [architecture](ARCHITECTURE.md) |
| Build automation | A reproducible automated compiler job and persisted results for supported targets | Repository automation once the intended supported target set and toolchain are agreed |
| Release provenance | A validated source-to-car/media link and a release record | A tagged release only after its actual source/configuration and physical checks are established |

The repository already has a populated hardware inventory and a preparation test suite; those are not substitutes for the missing physical design files, algorithm fixtures or measurements.

## Deferred bonus: finish markers and obstacle stopping

The primary baseline does not implement finish-marker or ultrasonic stopping. Earlier archived experiments contain partial logic, and the report describes later final behavior. The video shows a stop before a box but does not identify its trigger or source.

When this bonus is resumed, use actual source and validate all states, including missing camera lines, no echo, finish approach, no contact and persistent stop/reset behavior. Record measured sensor-to-box and bumper-to-box distances. Threshold constants and successful compilation do not establish stopping performance.

## Completion criteria for the next useful milestone

- One explicitly identified firmware revision is built and associated with the assembled car.
- Steering, camera, power and motor-channel checks are recorded for that revision.
- Repeated lap attempts have complete outcomes, source/configuration identifiers and measured times for completed laps.
- A tuning comparison shows whether a steering/filter or curve-speed change improves completed-lap performance without hiding failed runs.

No fastest-lap time, optimum gain set or measured improvement can be filled in until those observations exist.
