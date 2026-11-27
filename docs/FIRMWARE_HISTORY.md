# Firmware history and contribution evidence

[Back to README](../README.md)

This repository preserves a sequence of experiments, not a single documented release. The review baseline is commit [`81d8022`](https://github.com/Yasyous1/Gorilla-Turbo/commit/81d802210365757ba847fa165d7b237d982bb0b5), dated **7 April 2026**. Existing source files, calibration values and historical notes are retained.

## Latest tracked source versus reported final car

The latest tracked source is [`hell1/better_intersection`](../hell1/better_intersection). It extends the preceding motor-and-steering revision with checks intended to reject misleading line candidates at intersections. It contains no ultrasonic, finish-line or latched-stop implementation. Its status is **latest tracked experiment**, not a verified final release.

The later project report, *Rapport de Projet Personnel et Professionnel — Conception et réalisation d’une voiture autonome intelligente* (`ppp.pdf`, academic year 2025–2026), describes a more complete car. Its bibliography explicitly names **`bensaid2.0.ino`** as the final source, consulted on 30 May 2026 (PDF p. 23, printed p. 16). That file is absent from the inspected repository and recovered source files. The report is supporting project evidence and is not distributed in this repository.

| Item | Latest tracked source | Later report |
|---|---|---|
| Identified source | `hell1/better_intersection` | `bensaid2.0.ino`, unavailable |
| Steering center | 105° | 98° |
| Steering limits | 40–150° | 40–150° |
| PD gains | `KP=0.65`, `KD=0.15` | `KP=0.95`, `KD=0.25` |
| Loop gate | 20 ms | 8 ms |
| Motor PWM | Base 200; minimum 180 | Maximum 200; minimum 120; finish 167 |
| Single-border lane-width factor | 0.7 | 0.75 |
| Horizontal-line threshold | 20° | 35° |
| Ultrasonic/finish behavior | Absent | Reported final-zone handling and latched stop at 10 cm |

Sources: the constants and `loop()` in [latest tracked source](../hell1/better_intersection); report PDF pp. 15, 19–20 and 26, printed pp. 8, 12–13 and 19. These differences are not settings to copy into the tracked experiment. Restoring the final version requires the actual file and physical verification.

Youssef reported a working car demonstration on **11 May 2026**, while describing an unsuccessful qualification outcome. A [prototype photograph](../assets/media/gorilla-turbo-prototype.jpg) and [track video](../assets/media/gorilla-turbo-track-demo.mp4) were subsequently supplied on **27 November 2026** and are included in this review branch. The video shows track driving and a visible stop before a white box near the end. Its recording date, relationship to the May demonstration and exact flashed source are unconfirmed; it does not establish a timed lap or measured stopping distance. See [media observations](VALIDATION.md#available-demonstrations-and-result-evidence).

## Revision inventory

Dates below are commit dates in Tunisia time. A filename containing “corrected”, “better” or “final” does not establish hardware validation. PWM numbers are command values, not measured vehicle speeds.

| Source | Added | Purpose and distinguishing configuration |
|---|---|---|
| [`day 2/main_day2.cpp`](../day%202/main_day2.cpp) | 3 Apr | Integrated camera, steering, L298N and ultrasonic experiment. `Servo.h`; center 97°, limits 40–128°; angle-based PD 1.0/8.0; PWM 200–150; 12 cm stop threshold. Its missing plotting declaration is supplied by the repository cleanup. |
| [`day 2/Serial_Plotting.cpp`](../day%202/Serial_Plotting.cpp) | 3 Apr | Serial plotting helper, not a standalone program. `sendData(char*, long)` preserves the original integer output semantics. |
| [`TRY2.ino`](../TRY2.ino) | 3 Apr | Integrated pixel-based PD, motors, ultrasonic and horizontal-marker experiment. Center 98°, limits 40–140°; PD 0.35/0.18; alpha 0.25; PWM 150/140/125, lost-line 110, finish 105; 12 cm stop threshold. |
| [`code_cam_servo.ino`](../code_cam_servo.ino) | 3 Apr | Minimal camera/steering test. Center 95°; PD 0.45/0.25; alpha 0.35; initial lane width 30 px. Recenters on missing vectors. No motor or obstacle control. |
| [`nxp_cup_corrected.ino`](../nxp_cup_corrected.ino) | 3 Apr | Alternative motor interface: PWM pins 3/4 and direction pins 5/6. Center 95°; PD 0.45/0.25; PWM 200–100. This is not the six-pin L298N configuration used by later motor sketches. |
| [`code_cam_servov3.ino`](../code_cam_servov3.ino) | 3 Apr | Camera/steering experiment with adaptive filtering. Center 95°; PD 0.45/0.25. Contains an undeclared optional `cameraServo` reference and two camera acquisitions per update. Preserved as an incomplete historical experiment. |
| [`day 2/code_cam_servo.ino`](../day%202/code_cam_servo.ino) | 3 Apr | Byte-identical duplicate of root `code_cam_servo.ino`; preserved to keep the original history and paths. |
| [`day 2/servo_calibration.cpp`](../day%202/servo_calibration.cpp) | 3 Apr | Standalone serial calibration using `Servo.h` on pin 19. Starts at 84° and accepts 40–128°. Disconnect motor power: the sketch does not actively configure or disable motor outputs. |
| [`test erreur-servo/test-erreur_servo.ino`](../test%20erreur-servo/test-erreur_servo.ino) | 3 Apr | Revised camera/steering diagnostic. Removes the second camera read and camera-servo branch; adds near/far target diagnostics. Center 95°; PD 0.45/0.25; nominal 20 ms gate. |
| [`meilleur-test-servo.ino`](../meilleur-test-servo.ino) | 3 Apr | More aggressive steering experiment. Center 95°; PD 0.95/0.35; camera offset 2 px; error = lateral error + 0.30 × heading error; correction limited to ±35°; alpha 0.50/0.80. No propulsion or ultrasonic code. |
| [`helemwthakek.ino`](../helemwthakek.ino) | 5 Apr | L298N motor/steering integration. Center 97°; PD 0.95/0.15; PWM 180–160; single-border offset uses lane width ×0.65. No ultrasonic code. |
| [`hell1/sketch_with_motors.cpp`](../hell1/sketch_with_motors.cpp) | 7 Apr | Retuned version of `helemwthakek.ino`: center 105°, `KP=0.65`, PWM 200–180 and lane-width factor 0.7. `KD=0.15` is unchanged. |
| [`hell1/better_intersection`](../hell1/better_intersection) | 7 Apr | Latest tracked source. Adds primary-vector selection and pair plausibility checks to the preceding revision. Preserves its servo, gain, motor and loop settings. |
| [`better_intersection`](../better_intersection), [`hell1/readme.md`](../hell1/readme.md) | 7 Apr | One-newline placeholders; neither contains a usable program or operating instructions. |

These programs must be built separately: several define their own `setup()`, `loop()` and globals. See [build and operation](BUILD_AND_RUN.md) for the isolated build workflow, and [engineering notes](ENGINEERING_NOTES.md) for the remaining limitations.

## Individual and team contributions

The following attribution identifies what each account contributed to this repository. A whole-file upload establishes a contribution to the shared codebase; it does not prove exclusive authorship of every line or ownership of all hardware work.

### Youssef Elloumi — `Yasyous1`

| Contribution supported by history | Evidence |
|---|---|
| Established the repository and contributed the Day 2 integrated control experiment | [`6b291e2`](https://github.com/Yasyous1/Gorilla-Turbo/commit/6b291e2f84e580887607744609b0fb5b42812f6e), [`f9d580a`](https://github.com/Yasyous1/Gorilla-Turbo/commit/f9d580aca780a28401717d2509eb00262a1f7e48) |
| Added serial plotting and an interactive steering-center calibration program | [`63f71ad`](https://github.com/Yasyous1/Gorilla-Turbo/commit/63f71adb6fbdd1a2e31db7c3f7a2481ca65a03a2), [`4884467`](https://github.com/Yasyous1/Gorilla-Turbo/commit/488446778e6ed50d94fb8d9a3f22029afb42f05b) |
| Recorded perception limitations and reviewed steering behavior and calibration | [`314fd8c`](https://github.com/Yasyous1/Gorilla-Turbo/commit/314fd8cdfaeadfadf5ce66b6d111788c1a00d688), [`ecbc6d5`](https://github.com/Yasyous1/Gorilla-Turbo/commit/ecbc6d54cd0f98e2e17a2aff5e8bdac2a34cb96c) |
| Contributed a retuned motor/steering integration based on the earlier shared implementation | [`420eba1`](https://github.com/Yasyous1/Gorilla-Turbo/commit/420eba10d6fb9ff138ad600817be070a06c04d3c) |
| Added vector-selection and pair-consistency logic intended to improve intersection handling | [`81d8022`](https://github.com/Yasyous1/Gorilla-Turbo/commit/81d802210365757ba847fa165d7b237d982bb0b5) |

### Nesrine Ben Miled — `Ness3380`

| Contribution supported by history | Evidence |
|---|---|
| Contributed `TRY2.ino` and the minimal camera/steering program | [`9652867`](https://github.com/Yasyous1/Gorilla-Turbo/commit/9652867e2f42ca7ab3717010fb119ee8b5612da9) |
| Added alternative motor integration and the camera/servo v3 experiment | [`8ced402`](https://github.com/Yasyous1/Gorilla-Turbo/commit/8ced40221bded977a7ba068cbcac0649b49faae5), [`a794261`](https://github.com/Yasyous1/Gorilla-Turbo/commit/a79426108385bbb642e45de86fc8b293497c2fa6) |
| Added revised steering diagnostics, single-frame processing and a more aggressive steering variant | [`d3668e3`](https://github.com/Yasyous1/Gorilla-Turbo/commit/d3668e3f3811121e86949456506e4d9473893946), [`e96f1b7`](https://github.com/Yasyous1/Gorilla-Turbo/commit/e96f1b748641d4f43f180457c204726c23936a2b) |
| Added the integrated L298N motor/steering source later extended by Youssef | [`b2b8161`](https://github.com/Yasyous1/Gorilla-Turbo/commit/b2b81614b2961b098cb07287c303d4ae5723c246) |

### Team credit

The project report credits **Nesrine Ben Miled, Youssef Elloumi and Ilyess Biskri**, supervised by **Abderrazek Chatti** (PDF p. 1). It describes the team's hardware integration, PLA chassis fabrication at the INSAT FabLab, firmware and testing work collectively (PDF pp. 19–22). It does not assign those tasks to individual members. Ilyess's individual role is therefore not inferred from the absence of a matching Git author.

## Evidence used and its limits

| Evidence | What it can establish | What it cannot establish alone |
|---|---|---|
| Tracked source at `81d8022` and Git history | Implemented branches, constants, dependencies, file relationships and account contributions | Which binary ran in a demonstration; physical wiring; measured reliability |
| Project report `ppp.pdf`, cited above by physical PDF page | Reported later hardware, team credits, intended/final configuration and described tests | Contents of missing `bensaid2.0.ino`; independent measured results; individual subsystem ownership |
| Dated user reports recovered during the 27 Nov 2026 review | Reported observations, including battery-related board failure and a later working demonstration | Electrical root cause without measurements; repeatability; exact source revision |
| Photograph and 39.37-second track video supplied on 27 Nov 2026 | Visible prototype layout, track driving and a stop before a box near the end | Recording date, flashed firmware, motor PWM, measured speed/distance, sensor causation or a permanently latched stop |
| Supplied `example-of-code-my-teammate-wrote.txt` | A “Day 1 Fixed Version” example and its attempted corrections | Final firmware, proven success or exclusive authorship |
| Earlier advice, comments, commit prose and proposed designs | Design intent and things to investigate | A completed feature, a tested fix or a measured result |

The report's validation table (PDF p. 20) describes tests and criteria. Its practical checklist (PDF p. 27) remains an unfilled **Yes/No** form. Neither supplies lap times, statistical repeatability or stopping-distance measurements.

Four recovered source files remain external reference material rather than new firmware releases in this repository. They all use a 20 ms loop gate and lack ultrasonic/final-stop code:

| Recovered filename | Servo center | PD gains | Base/minimum PWM |
|---|---:|---|---|
| `Pasted code.cpp` | 95° | 0.90/0.25 | 190/180 |
| `Pasted code(1).cpp` | 97° | 0.90/0.25 | 190/180 |
| `Pasted code(2).cpp` | 98° | 0.95/0.20 | 200/170 |
| `Pasted code(3).cpp` | 98° | 0.95/0.20 | 200/170 |

The last two files are byte-identical. `Pasted code(1).cpp` additionally latches a slowdown to PWM 165–155 after a horizontal marker is detected; it does not latch a stop. The recovered filenames do not establish flashing dates or successful physical tests. These variants help explain the tuning history but do not recover the configuration or obstacle implementation described in `bensaid2.0.ino`.

Before marking a final release, recover `bensaid2.0.ino` or export the actual current sketch, record tool/library versions and camera settings, identify its hardware configuration, and associate a dated physical test with an exact commit. Use the [validation procedure](VALIDATION.md).
