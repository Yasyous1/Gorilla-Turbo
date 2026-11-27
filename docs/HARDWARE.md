# Hardware and wiring

This document separates connections defined by source code, hardware described in the team report and features visible in the supplied media. The physical car was not available for the November 2026 repository review. Match the selected firmware to the wiring before applying motor power.

## Hardware used by the project

| Component | Role | Evidence and qualification |
| --- | --- | --- |
| Teensy 4.1 | Executes the C++ control loop and commands the actuators | Named in project history and the team report; Arduino/Teensy firmware uses its GPIO, SPI and PWM APIs. Earlier conversations also discussed Teensy 4.0. |
| Pixy2.1 | Detects track-line vectors onboard the camera | Reported camera model; the source uses `Pixy2.h` and the line-tracking program over SPI. |
| Steering servo | Turns the front wheels | Signal pin 19 in the recent tracked variants. The report names MG945; earlier discussion mentions MG995. Check the label on the installed servo before treating either model as verified. |
| L298N module | Drives two DC propulsion motors | Six-pin control interface in the latest motor variants: two PWM enables and four direction signals. |
| Two DC motors | Propulsion | The report describes 12 V motors; exact motor model, gearing and wheel dimensions are not recorded. PWM is open-loop; no encoder feedback is implemented. |
| HC-SR04 | Frontal distance measurement | Used by `TRY2.ino` and `day 2/main_day2.cpp`; described in the later car report. **Absent from the newest tracked firmware.** |
| LM2596 adjustable module | Regulated supply | The report records a 5 V supply for Teensy, Pixy and steering servo. Regulator board rating, wiring and loaded-voltage measurements are not available. |
| Chassis, printed mounts and steering linkage | Mechanical support and wheel steering | The report credits PLA fabrication at the INSAT FabLab. The supplied photo shows magenta printed structures and clear chassis plates; their dimensions and materials are not independently verified. CAD/STL files are not in this repository. |

Report source: *Conception et réalisation d’une voiture autonome intelligente*, `ppp.pdf`, academic year 2025–2026, printed pp. 6, 8–9 and 12 (PDF pages 13, 15–16 and 19). It is reviewed project material, not an included bill of materials or an independently measured electrical specification. See [source history](FIRMWARE_HISTORY.md).

### Visible prototype layout

The [photograph supplied by Youssef](../assets/media/gorilla-turbo-prototype.jpg) shows an elevated camera on a magenta mast and bracket, a front-facing twin-transducer ultrasonic module, battery holders, a rear controller board on prototyping board, wiring, clear chassis plates and wheels. The [track video](../assets/media/gorilla-turbo-track-demo.mp4) shows a car with the same distinctive mast arrangement; matching appearance does not establish identical electronics or firmware.

These views document the assembled layout. They do not resolve the installed servo label, battery specifications, motor-side mapping, regulator setting or individual pin connections. Preserve the source-defined pin tables below until the actual wiring has been traced.

## Latest tracked firmware: code-defined connections

Source: [`hell1/better_intersection`](../hell1/better_intersection), also shared by [`hell1/sketch_with_motors.cpp`](../hell1/sketch_with_motors.cpp).

| Teensy pin | Connection | Behavior in the latest firmware |
| ---: | --- | --- |
| 2 | L298N ENA | Channel A PWM, command range 0–255 |
| 3 | L298N ENB | Channel B PWM, command range 0–255 |
| 4 | L298N IN1 | HIGH while driving forward |
| 5 | L298N IN2 | LOW while driving forward |
| 6 | L298N IN3 | LOW while driving forward |
| 7 | L298N IN4 | HIGH while driving forward |
| 19 | Steering-servo signal | `PWMServo`; center 105°, constrained to 40–150° |

`stopMotors()` sets all four direction outputs LOW and both enable PWM commands to zero. The normal drive command applies the same PWM value to both channels, reducing it from 200 to 180 as steering deviation increases.

**Channel A/B is deliberate:** the latest source comments call A the left motor and B the right motor. The report's wiring table and some earlier code use the opposite side labels. The numeric pin mapping agrees. Verify which physical wheel is connected to OUT1/OUT2 and OUT3/OUT4 with the wheels lifted; do not swap pins or direction values based on a comment alone.

For PWM on ENA/ENB, the module must allow the microcontroller to drive those inputs. Check the enable-jumper arrangement on the actual L298N board. A pin table does not establish the module's supply-jumper configuration.

### Camera connection

The current firmware constructs `Pixy2 pixy` without a custom transport or alternate SPI pin configuration. With the documented default Teensy SPI bus, the signal connections are:

| Pixy2 I/O connector | Teensy 4.1 | Signal |
| --- | ---: | --- |
| Pin 1 | 12 | MISO: camera to controller |
| Pin 4 | 11 | MOSI: controller to camera |
| Pin 3 | 13 | SPI clock |
| Pin 6 | GND | Common signal reference |

Use PixyMon's **Arduino ICSP SPI** interface for the unchanged `Pixy2.h`/`pixy.init()` setup. Pixy's **SPI with SS** mode is a different configuration; its connector pin 7 is only used when slave-select operation is explicitly configured. Do not infer a confirmed chip-select wire from the fact that Teensy pin 10 is commonly used as CS. Pixy's USB connection to a PC is used for PixyMon; it is not this firmware's Teensy-to-camera link.

These signal assignments are supported by [PJRC's Teensy 4.1 pinout](https://www.pjrc.com/teensy/pinout.html) and the [Pixy2 porting guide](https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide). They are not a photograph-based verification of the car's cable. Verify the connector orientation against [Pixy's connector drawings](https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:port_pinouts).

### Ultrasonic and jumper connections belong to earlier variants

| Function | `TRY2.ino` / `day 2/main_day2.cpp` | Latest `hell1/better_intersection` |
| --- | --- | --- |
| HC-SR04 TRIG | Pin 23 | Not configured or read |
| HC-SR04 ECHO | Pin 22 | Not configured or read |
| Jumper/button input | Pin 20, `INPUT_PULLUP` | Not configured or read |

Pin 20 is not a dependable start or emergency-stop input in these versions: `TRY2` never reads it, and Day 2's initial speed selection is superseded by the normal speed calculation. The newest motor firmware starts driving automatically after initialization.

The alternative [`nxp_cup_corrected.ino`](../nxp_cup_corrected.ino) uses PWM pins 3/4 and direction pins 5/6. It is a different motor interface and must not be uploaded as a drop-in replacement for the six-signal L298N wiring above.

## Power arrangement and checks

The team report describes two branches from the battery: one to the L298N/motors, and one through an LM2596 adjusted to 5 V for the controller, camera and steering servo. All grounds share a reference. This is a reported arrangement, not a complete schematic; the battery model, fusing, regulator capacity and signal-level protection still need confirmation.

The project history includes a Teensy failure when battery power was introduced. Its electrical root cause was not established. Apply the manufacturer's requirements when checking the surviving car:

- Teensy 4.1 GPIO accepts 0–3.3 V and is **not 5 V tolerant**. Keep the HC-SR04 ECHO signal within that range using suitable level conversion, and verify the actual signal voltage. The installed conversion circuit is not documented.
- PJRC specifies a 5 V input at VIN when USB power is not being used. Never apply the motor battery directly to VIN or a GPIO.
- VIN and VUSB are connected on an unmodified Teensy. Disconnect the external controller supply for USB programming unless the board has the documented VIN/VUSB isolation or another verified power arrangement.
- Supply the servo through an appropriately rated regulated rail with a common ground. Record voltage at the controller and servo while steering and motor loads change; a regulator's unloaded display is not a loaded-supply test.

Manufacturer source: [Teensy 4.1, Digital Pins and Power](https://www.pjrc.com/store/teensy41.html). These requirements explain what to check; they do not prove why the earlier board failed.

## Calibration values are revision-specific

For the newest tracked source, keep the existing center **105°**, range **40–150°**, sign **+1**, PD gains **0.65/0.15**, filter factors **0.35/0.65**, motor commands **200/180** and loop gate **20 ms**. These values were preserved during the documentation work.

The later report lists center 98°, PD 0.95/0.25 and an 8 ms gate for the missing final firmware. Those values must not be transplanted into an April sketch to manufacture a supposed final version. See [firmware history](FIRMWARE_HISTORY.md) for the differences and [build and operating instructions](BUILD_AND_RUN.md) for the calibration procedure.

## Hardware information still needed

To turn this into a fully reproducible wiring reference, record the installed servo label, motor models, battery and regulator ratings, actual motor-side assignments, camera connector/power wiring, HC-SR04 level conversion, and an annotated photograph or schematic. Pair that record with the exact firmware file or commit used for the demonstration.
