# Engineering notes and unresolved issues

[Back to README](../README.md) · [Firmware history and attribution](FIRMWARE_HISTORY.md)

These notes separate observations, source-level findings and reported outcomes. They do not claim that the documentation review reproduced a physical fault or validated a track run. The inspected baseline is [`81d8022`](https://github.com/Yasyous1/Gorilla-Turbo/commit/81d802210365757ba847fa165d7b237d982bb0b5). See [validation](VALIDATION.md) for checks actually performed and the remaining hardware procedure.

## 1. Power worked on USB, then a board failed on battery power

**Symptom and evidence.** In a dated report on 24 March 2026, Youssef said the components worked while the Teensy was USB-powered and the board failed immediately after batteries were connected. The following day's discussion moved to Teensy 4.1. This is an observed sequence reported by the user; the available record does not include voltage measurements, a verified wiring diagram or a failed-board diagnosis.

**Cause and attempted response.** The electrical root cause is unknown. Earlier discussions considered several power arrangements; those proposals should not be presented as the final wiring. The later project report attributes supply instability to the L298N module's limited 5 V output current and describes using an **LM2596 adjusted to 5 V** for the Teensy, Pixy2.1 and steering servo, with a separate battery-to-L298N motor path and common grounds (`ppp.pdf`, PDF pp. 9, 15 and 19). That report diagnosis does not establish why the earlier board was damaged.

**Outcome and lesson.** The report lists power stability among the difficulties addressed and describes checking for controller resets during actuator operation (PDF p. 20). It provides no oscilloscope traces, peak-current measurements or proof that a particular earlier wiring error caused the damaged board. The defensible lesson is to validate the actual power distribution and voltage levels before integration, then check the regulated rail under load. See [hardware](HARDWARE.md); a regulator name alone does not establish that the supply is adequate.

## 2. Steering center and direction changed across experiments

**Symptom and evidence.** The source history contains several steering centers: 84° in the [calibration sketch](../day%202/servo_calibration.cpp), 97° in [Day 2](../day%202/main_day2.cpp), 95° in several camera tests, 98° in [TRY2](../TRY2.ino), and 105° in [the latest motor revision](../hell1/better_intersection). The existing [servo review](../test%20erreur-servo/Read%20me.md) explicitly asks for physical confirmation of both center and steering sign. The later report describes steering oscillations as a project difficulty (`ppp.pdf`, PDF p. 20).

**Established cause versus possible causes.** A center offset or reversed correction sign can make a correctly calculated error produce the wrong mechanical response. The available evidence does not establish which mechanism caused every reported steering problem. Different revision constants are evidence of experimentation, not proof that one universal center is correct.

**What was tried.** Youssef added serial entry of steering angles in [commit `4884467`](https://github.com/Yasyous1/Gorilla-Turbo/commit/488446778e6ed50d94fb8d9a3f22029afb42f05b). The team explored proportional/derivative gains, a pixel deadband, adaptive filtering and bounded steering. Later motor revisions adjust PWM with steering angle. These mechanisms are present in source; their improvement in physical stability is not quantified.

**Outcome and lesson.** Keep each revision's settings intact. Establish the straight-wheel command and correction direction on the assembled car before tuning gains. The calibration utility uses `Servo.h`, while newer driving sketches use `PWMServo.h`; verify the physical center with the selected driving sketch as well. Disconnect propulsion during calibration because the utility does not actively disable the motor driver. The report's 98° center and PD 0.95/0.25 belong to the unavailable final source, not to the latest tracked experiment.

## 3. A loop checked one camera acquisition and processed another

**Evidence and established cause.** In [`code_cam_servov3.ino`](../code_cam_servov3.ino), `loop()` obtains features, then `computeTargetX()` calls `filterLines()`, which obtains features again. The validity check and lane calculation can therefore refer to different acquisitions. The same file also refers to an undeclared `cameraServo`; setting the runtime boolean `USE_CAMERA_SERVO` to false does not make that identifier valid C++.

**What changed.** The later [`test-erreur_servo.ino`](../test%20erreur-servo/test-erreur_servo.ino), contributed in [commit `d3668e3`](https://github.com/Yasyous1/Gorilla-Turbo/commit/d3668e3f3811121e86949456506e4d9473893946), removes the camera-servo branch and leaves one camera acquisition in `loop()`. Filtering uses that acquired frame. The subsequent motor and intersection revisions retain this structure.

**Outcome and lesson.** The single-acquisition change is demonstrable from source. No available log isolates its effect on lap performance. Acquire once, validate that result and use the same frame throughout a control update. Preserve the incomplete v3 file as history rather than silently treating it as a working release.

## 4. Crossings and partial lines made lane selection ambiguous

**Symptom and evidence.** The existing [Day 2 review](../day%202/read.md) identifies vector-to-lane association as a weakness around intersections, extra markings, occlusion and short segments. Earlier sketches select a candidate by proximity to image center or vector length. Those rules can choose unrelated markings; this is a limitation of the algorithm, not evidence that every crossing failed.

**What was tried.** The team normalized vector endpoints, filtered orientation, separated left/right candidates and remembered lane width for single-border tracking. The later motor code weights the estimated target toward the upper/lookahead point: **0.35 × near center + 0.65 × far center**. In [commit `81d8022`](https://github.com/Yasyous1/Gorilla-Turbo/commit/81d802210365757ba847fa165d7b237d982bb0b5), Youssef added selection of vectors reaching farther down the image and checks on pair width, width continuity, relative length and endpoint-height consistency.

**Implemented response.** [`hell1/better_intersection`](../hell1/better_intersection) accepts a coherent pair, falls back to a more plausible single border, or requests an image-center target when neither candidate is preferred. This is a geometric heuristic; it does not use explicit intersection data to plan a route. Requesting a centered target also does not instantly straighten the wheels because filtering and the previous PD state remain active.

**Outcome and lesson.** The algorithmic changes are implemented. There is no committed crossing-success count, comparative dataset or repeatable before/after track result. Evaluate perception independently of steering gains, and include both left/right single-border cases and ambiguous crossings in physical tests. Camera height, pitch and lighting also require recording; the report identifies sensitivity to these factors (`ppp.pdf`, PDF pp. 13 and 20).

## 5. Holding the last steering angle does not stop a lost car

**Evidence.** [`code_cam_servo.ino`](../code_cam_servo.ino) recenters on missing vectors. Later camera/servo revisions keep the previous command. The [latest tracked motor source](../hell1/better_intersection), in both its no-vector and no-valid-line branches, holds `lastSteerAngle` and commands `MOTOR_SPEED_MIN=180`.

**What this achieves and leaves unresolved.** Retaining the last command avoids an immediate steering reset during a brief visual interruption. It also means the car keeps moving indefinitely if a line never returns or a camera error persists. The code has no time-based lost-line stop, explicit camera-fault state or manual arming state; propulsion begins after setup. The benefit during short interruptions and the recovery duration have not been measured in the available evidence.

**Lesson and next step.** Document this behavior accurately and test with wheels raised before any free-driving test. A timed loss-of-line policy and explicit arming/fault handling are future behavior changes requiring a separate design and hardware validation. This documentation cleanup does not silently introduce them.

## 6. Obstacle-stop claims differ between revisions

### TRY2: no-vector handling bypasses the stop check

[`TRY2.ino`](../TRY2.ino) reads distance, then returns early on missing camera vectors after commanding lost-line PWM 110 or finish-approach PWM 105. Its obstacle-stop checks occur later, so they are skipped on that branch. In frames with vectors, it stops at a configured threshold of 12 cm, but the stop is not latched: a later larger or missing-echo reading can allow motion again. A missing echo is represented as 999 cm, and `pulseIn(..., 30000)` can block the loop.

These are source-level findings. There is no corresponding physical failure log. They are reasons not to promote this archive sketch as a verified final obstacle controller.

### Day 2: “non-blocking” and distance comments overstate the implementation

[`day 2/main_day2.cpp`](../day%202/main_day2.cpp) labels ultrasonic acquisition non-blocking, but calls `pulseIn(..., 6000)`. It triggers on one loop call and reads on a later call, with other work between them; the available code does not prove that every echo is captured. A timeout retains the previous range. Finish-line detection runs only every ten seconds and examines features from the preceding acquisition. Once its finish-state obstacle threshold is reached, it does latch the stop in an infinite loop. Ordinary obstacle stopping is not latched.

The comment implying a 12 cm threshold is inside a 10 cm target is incorrect. A software threshold, sensor mounting offset and measured final bumper-to-obstacle distance are different quantities.

### Later reported approach

The report describes a 10 cm latched stop in the missing `bensaid2.0.ino` (`ppp.pdf`, PDF pp. 19–20, 24–26). This is **reported final behavior**, absent from the latest tracked source. Its pseudocode and unfilled checklist are insufficient to verify every branch or stopping distance. Recover the actual source before claiming that this repository reproduces that final behavior.

The supplied *NXP Cup 2025/2026 Rules*, version 1.0, July 2025 (`NXP-Cup-2026-Rules.pdf`, PDF pp. 9 and 12), specify slowdown after the final start/stop line and a stop within 10 cm without contact. The supplied teammate “Day 1 Fixed Version” example uses a 50 cm threshold and calls a five-second delay a rule requirement; no such five-second requirement was found in the supplied rules. That earlier example is not a compliance reference or final firmware.

**Lesson.** A stop decision needs an explicit state and deliberate behavior on every sensor path. Validate physical stopping distance and sensor-failure behavior separately. Neither compilation nor a threshold constant demonstrates competition compliance.

## 7. File layout obscured which program to build

**Evidence.** The original root contains several unrelated `.ino` entry points. The `day 2` directory mixes a main car program, a camera/servo program, a calibration program and a helper. The newest source has no extension. `main_day2.cpp` includes `Serial_Plotting.h`, which was absent, while the matching implementation already existed. Several files use different motor interfaces and calibration values; they are not interchangeable modules.

**Response in this cleanup.** The build workflow stages a selected program in its own Arduino sketch directory, keeps the original source paths and avoids combining entry points. A declaration header supplies the existing plotting interface without changing its integer formatting. Historical duplicates and incomplete experiments remain visible in the [inventory](FIRMWARE_HISTORY.md). Build results and any exclusions are recorded in [validation](VALIDATION.md).

**Lesson.** Name the exact source revision and dependencies in every hardware test. Preserve experiments for traceability, but make the intended build target explicit. A filename or successful compilation does not identify the firmware used in a previous demonstration.

## 8. An unused AVR example blocked the Teensy build

**Observed during this review, not claimed as a historical team fault.** On 27 November 2026, compiling the selected car source with the complete official [Pixy2 Arduino 1.0.3 ZIP](https://github.com/charmedlabs/pixy2/raw/master/releases/arduino/arduino_pixy2-1.0.3.zip) failed in the bundled `ZumoBuzzer.cpp`: the compiler rejected `ISR(TIMER2_OVF_vect)` and could not resolve AVR timer registers including `TIMSK2` and `TCCR2A`. The car does not use that example, but Arduino's library compilation still included its implementation.

**Established response and outcome.** [`scripts/prepare_pixy2.py`](../scripts/prepare_pixy2.py) verifies the archive's SHA-256 and stages its five unchanged camera-driver headers (`Pixy2.h`, `TPixy2.h`, `Pixy2Line.h`, `Pixy2CCC.h`, `Pixy2Video.h`) with their existing attribution. It leaves the downloaded ZIP and installed libraries untouched. Explicitly selecting this staged library allowed the latest tracked `intersection` target to compile with Arduino CLI 1.5.1 and `teensy:avr@1.62.0`. The complete toolchain, command and target results are recorded in [validation](VALIDATION.md).

**Lesson.** Dependency packaging can introduce code for hardware that the application never uses. Pin the tested archive and choose the needed upstream driver without changing its contents. These are newly verified dependency versions; the original project's installed versions were not recorded. A successful build verifies compilation and linking, not operation on the car.

## 9. Mechanical fragility and component availability constrained iteration

The project report lists sourcing the Teensy and Pixy2.1 and the fragility of some PLA parts among the team's difficulties (`ppp.pdf`, PDF p. 20). It records local chassis fabrication at the INSAT FabLab and progressive subsystem testing before full integration (PDF pp. 9 and 19–20). The [supplied prototype photo](../assets/media/gorilla-turbo-prototype.jpg) now documents the assembled mast, mounts and chassis, but no broken-part dimensions, material tests, replacement history or measured improvement are available. Credit these as collective prototyping constraints and reported working methods, without inventing a specific mechanical redesign or assigning it to an individual. CAD files and dated revision photographs would make this part of the engineering process reproducible.

## Evidence references

- Linked source files and immutable commits are the evidence for implementation and account contributions.
- `ppp.pdf`: *Rapport de Projet Personnel et Professionnel — Conception et réalisation d’une voiture autonome intelligente*, INSAT, academic year 2025–2026. Page references above use physical PDF pages; the document is supporting material reviewed for this cleanup and is not distributed here.
- Dated user observations, recovered during the 27 November 2026 review: battery-related failure on 24 March; subsequent Teensy 4.1 discussion on 25 March; reported working demonstration on 11 May. These are summarized observations, not reproduced test records or transcripts.
- The prototype photograph and track video supplied on 27 November 2026 are retained in [`assets/media/`](../assets/media/). They provide visual evidence of the assembled car and a demonstration; [validation](VALIDATION.md#available-demonstrations-and-result-evidence) records what can and cannot be established from them.
- `NXP-Cup-2026-Rules.pdf`: supplied historical rules, season 2025/2026, version 1.0, July 2025. Requirements are distinct from implemented behavior and do not establish vehicle compliance.
- `example-of-code-my-teammate-wrote.txt`: supplied “Day 1 Fixed Version” example. Its comments describe attempted corrections; they do not independently validate them.
