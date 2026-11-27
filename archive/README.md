# Historical firmware and notes

This archive contains earlier experiments, duplicate sources, placeholders and the original review notes. They remain useful for contribution history and technical comparisons. Their tuning advice and pin mappings belong to their own revisions; they are not the current operating instructions.

Use [the current track-following entry point](../firmware/README.md) for new lap development. The archived programs are independent sketches, not interchangeable modules. Select one using an explicit alias in [Build and run](../docs/BUILD_AND_RUN.md); compiling this directory as a single sketch would mix multiple programs.

All 18 relocated source/note files, including the two promoted entry points, were compared byte-for-byte with the pre-organization commit. [PATHS.json](PATHS.json) records the complete mapping and SHA256 hashes. Existing code comments and credits were preserved. No historical source was deleted.

## Previous and current paths

Previous paths below are historical names. The current-path links point to files in this layout.

| Previous path | Current path |
| --- | --- |
| `TRY2.ino` | [`archive/firmware/TRY2.ino`](firmware/TRY2.ino) |
| `better_intersection` | [`archive/firmware/better_intersection`](firmware/better_intersection) |
| `code_cam_servo.ino` | [`archive/firmware/code_cam_servo.ino`](firmware/code_cam_servo.ino) |
| `code_cam_servov3.ino` | [`archive/firmware/code_cam_servov3.ino`](firmware/code_cam_servov3.ino) |
| `day 2/Serial_Plotting.cpp` | [`archive/firmware/day 2/Serial_Plotting.cpp`](firmware/day%202/Serial_Plotting.cpp) |
| `day 2/Serial_Plotting.h` | [`archive/firmware/day 2/Serial_Plotting.h`](firmware/day%202/Serial_Plotting.h) |
| `day 2/code_cam_servo.ino` | [`archive/firmware/day 2/code_cam_servo.ino`](firmware/day%202/code_cam_servo.ino) |
| `day 2/main_day2.cpp` | [`archive/firmware/day 2/main_day2.cpp`](firmware/day%202/main_day2.cpp) |
| `day 2/read.md` | [`archive/firmware/day 2/read.md`](firmware/day%202/read.md) |
| `day 2/servo_calibration.cpp` | [`firmware/calibration/servo_calibration/servo_calibration.ino`](../firmware/calibration/servo_calibration/servo_calibration.ino) |
| `helemwthakek.ino` | [`archive/firmware/helemwthakek.ino`](firmware/helemwthakek.ino) |
| `hell1/better_intersection` | [`firmware/track_following/track_following.ino`](../firmware/track_following/track_following.ino) |
| `hell1/readme.md` | [`archive/firmware/hell1/readme.md`](firmware/hell1/readme.md) |
| `hell1/sketch_with_motors.cpp` | [`archive/firmware/hell1/sketch_with_motors.cpp`](firmware/hell1/sketch_with_motors.cpp) |
| `meilleur-test-servo.ino` | [`archive/firmware/meilleur-test-servo.ino`](firmware/meilleur-test-servo.ino) |
| `nxp_cup_corrected.ino` | [`archive/firmware/nxp_cup_corrected.ino`](firmware/nxp_cup_corrected.ino) |
| `test erreur-servo/Read me.md` | [`archive/firmware/test erreur-servo/Read me.md`](firmware/test%20erreur-servo/Read%20me.md) |
| `test erreur-servo/test-erreur_servo.ino` | [`archive/firmware/test erreur-servo/test-erreur_servo.ino`](firmware/test%20erreur-servo/test-erreur_servo.ino) |

## Known archive limitations

- The camera/servo v3 sketch still references an undeclared `cameraServo`; its historical build failure remains documented.
- The duplicated camera source and one-newline placeholders are preserved as historical artifacts, not recommended build targets.
- Earlier obstacle and finish-zone experiments have documented limitations. Obstacle-stop recovery remains a deferred bonus.
- Externally recovered pasted snapshots are discussed in the history document but their source files are not part of this checkout.

See [Firmware history](../docs/FIRMWARE_HISTORY.md) for dates, constants, evidence and commit-linked attribution. Git history retains the previous paths; for a moved file, `git log --follow -- <current-path>` follows its earlier versions.
