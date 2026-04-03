// ============================================================
//  SERVO CALIBRATION SKETCH  —  Day 2 Step 1
//  Flash this standalone, motors stay OFF the whole time.
//
//  How to use:
//  1. Flash this to the Teensy
//  2. Open Serial Monitor at 115200 baud
//  3. Type an angle (e.g. 80) and hit Enter
//  4. Watch the front wheels — are they straight?
//  5. Keep adjusting until perfectly straight
//  6. Write down that number → that is your defaultServoAngle
// ============================================================

#include <Arduino.h>
#include <Servo.h>

#define STEERING_SERVO_PIN  19    // matches your wiring
#define SERVO_MIN           40    // hard left limit
#define SERVO_MAX           128   // hard right limit

Servo steeringServo;
int currentAngle = 84;            // starting guess — adjust from here

void setup()
{
  Serial.begin(115200);
  steeringServo.attach(STEERING_SERVO_PIN);
  steeringServo.write(currentAngle);

  Serial.println("=== Servo Calibration ===");
  Serial.println("Type an angle (40-128) and press Enter.");
  Serial.println("Find the angle where front wheels are PERFECTLY straight.");
  Serial.print("Current angle: ");
  Serial.println(currentAngle);
}

void loop()
{
  if (Serial.available() > 0)
  {
    int input = Serial.parseInt();

    // Flush leftover newline characters
    while (Serial.available()) Serial.read();

    if (input >= SERVO_MIN && input <= SERVO_MAX)
    {
      currentAngle = input;
      steeringServo.write(currentAngle);
      Serial.print(">> Moved to angle: ");
      Serial.println(currentAngle);
      Serial.println("   Are wheels straight? If not, try a different value.");
    }
    else if (input != 0)  // ignore accidental 0 from empty reads
    {
      Serial.print("!! Out of range. Enter a value between ");
      Serial.print(SERVO_MIN);
      Serial.print(" and ");
      Serial.print(SERVO_MAX);
      Serial.println(".");
    }
  }
}
