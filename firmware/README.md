# Firmware entry points

Use one standalone sketch at a time. The main development target is [track following](track_following/track_following.ino); [servo calibration](calibration/servo_calibration/servo_calibration.ino) is a separate bench utility.

| Target | Source | What exists | What still needs verification |
| --- | --- | --- | --- |
| Track following | [`track_following/track_following.ino`](track_following/track_following.ino) | Pixy line acquisition, boundary selection, lane-width memory, filtered PD steering, steering-dependent motor PWM and intersection heuristics | Which source is flashed on the car, physical calibration, timed laps and repeated track runs |
| Servo calibration | [`calibration/servo_calibration/servo_calibration.ino`](calibration/servo_calibration/servo_calibration.ino) | Interactive serial steering adjustment | Installed servo center, limits and direction with propulsion disconnected |

Both files were moved without changing their contents. Track following comes from the latest tracked intersection experiment. It is a reference for future lap development, not a verified final or fastest version. The calibration utility starts at **84°**, while the track-following source uses **105°**; they do not share calibration settings.

## Build the main target

After preparing the dependencies in [Build and run](../docs/BUILD_AND_RUN.md):

```bash
python3 scripts/prepare_sketch.py intersection
arduino-cli compile --fqbn teensy:avr:teensy41 \
  --library build/libraries/Pixy2 build/intersection
```

Run these commands from the repository root. The `intersection` alias remains available for existing commands and selects the track-following source above. `servo_calibration` selects the bench utility. The helper stages one isolated sketch and preserves its source bytes.

## Current control settings

The track-following source retains center **105°**, steering limits **40–150°**, `KP=0.65`, `KD=0.15`, motor PWM **200–180** and a **20 ms** scheduling gate. These are existing source settings, not newly validated tuning recommendations. Constants remain inside the sketch.

The source starts propulsion automatically after initialization. On lost camera vectors, it keeps the previous steering angle and commands PWM 180. It has no ultrasonic or finish-marker stop. Follow the existing [hardware verification procedure](../docs/VALIDATION.md#physical-car-verification) before a powered run.

Historical programs are in [the archive](../archive/README.md). The current function responsibilities and proposed later module split are explained in [Architecture](../docs/ARCHITECTURE.md); priorities and missing inputs are in [Roadmap](../docs/ROADMAP.md).
