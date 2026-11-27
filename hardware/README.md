# Hardware inventory and missing records

This is the hardware information currently available from source code, the team report and supplied media. It is a starting inventory, not a verified purchasing list. Detailed source-defined pins and power notes are in [Hardware and wiring](../docs/HARDWARE.md).

| Item | Quantity currently supported | Available information | Missing information |
| --- | --- | --- | --- |
| Teensy controller | 1 reported | Teensy 4.1 in the team report and build target | Confirm the installed board revision and power modifications |
| Pixy camera | 1 reported/visible | Pixy2.1 reported; firmware uses Pixy2 line tracking over SPI | Installed camera settings, connector orientation and annotated cable wiring |
| Steering servo | 1 | Signal pin 19; report names MG945, earlier discussion names MG995 | Read the installed label; measure center, usable limits and steering direction |
| L298N driver | 1 reported | Two PWM enables and four direction signals in the reference firmware | Actual board/jumper arrangement and physical channel-to-wheel mapping |
| Propulsion DC motors | 2 reported | Report describes 12 V motors; equal open-loop PWM commands | Models, gearing, current draw and wheel dimensions |
| Ultrasonic sensor | 1 reported/visible | HC-SR04 reported; used in archived experiments | Actual wiring and signal-level conversion; integration is deferred |
| Regulator | Quantity not verified | LM2596 module and a 5 V electronics/servo rail described in the report | Installed module rating, set voltage, loaded measurements and wiring |
| Battery and holders | Quantity/specification not verified | Battery holders visible; battery-powered operation reported | Chemistry, cell configuration, voltage, capacity, protection and connectors |
| Chassis, camera mast and linkage | One assembled prototype visible | Clear chassis plates, printed magenta mounts and INSAT FabLab fabrication reported | CAD/STL files, dimensions, fasteners, mounting details and alignment measurements |

## Existing visual evidence

- [Prototype photograph](../assets/media/gorilla-turbo-prototype.jpg): assembled car, camera mast, sensor and electronics layout.
- [Track demonstration](../assets/media/gorilla-turbo-track-demo.mp4): physical track driving; the flashed source and recording date are unidentified.

## Records to add when available

1. An annotated wiring diagram or clear connector photographs tied to the actual car.
2. Confirmed component labels, quantities and electrical ratings to complete the inventory.
3. Measured power-rail behavior while steering and driving, plus the actual power/isolation arrangement.
4. A calibration record linked to a source commit: steering center/limits/sign, motor sides/directions, camera position and Pixy settings.
5. Original CAD/STL and dimensioned mechanical drawings.

No schematic, CAD model or measured calibration record has been created from guesses. See the [prioritized roadmap](../docs/ROADMAP.md) for how these gaps affect fastest-lap development.
