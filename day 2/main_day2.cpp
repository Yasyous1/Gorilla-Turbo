// ============================================================
//  NXP Cup 2026 — Main Racing Code
//  Day 2 Version: PD controller + curve speed + non-blocking ultrasonic
// ============================================================

#include <Arduino.h>
#include <Servo.h>
#include <Pixy2.h>
#include <Serial_Plotting.h>

// ============================================================
//  PIN DEFINITIONS
// ============================================================
#define IN1             4
#define IN2             5
#define IN3             6
#define IN4             7
#define ENA             2
#define ENB             3
#define STEERING_SERVO_PIN  19
#define TRIGGER_PIN     23
#define ECHO_PIN        22
#define BUTTON_PIN      20

// ============================================================
//  PD TUNING — adjust these while watching Serial Plotter
//  Tuning order:
//    1. Set KD=0, increase KP until robot steers but oscillates
//    2. Increase KD until oscillation dies
//    3. Never touch both at the same time
// ============================================================
#define KP   1.0f    // proportional gain — start here, increase slowly
#define KD   8.0f    // derivative gain   — add after KP is set

// ============================================================
//  SERVO & STEERING
// ============================================================
#define DEFAULT_SERVO_ANGLE     97    // CALIBRATED — wheels straight
#define STEERING_SERVO_MAX_LEFT  40
#define STEERING_SERVO_MAX_RIGHT 128
#define STEERING_SETPOINT        90.0f  // 90° = straight ahead in Pixy frame

// ============================================================
//  SPEED SETTINGS
// ============================================================
#define SPEED_STRAIGHT      200   // PWM on straights
#define SPEED_CURVE         150   // PWM on sharp curves
#define CURVE_THRESHOLD     15.0f // error (deg) above which we slow down

// ============================================================
//  OBSTACLE DETECTION
// ============================================================
#define OBSTACLE_STOP_DIST  12.0f   // cm — stop here (target: <10cm from box)
#define OBSTACLE_SLOW_DIST  50.0f   // cm — start slowing here
#define ULTRASONIC_INTERVAL 50      // ms between ultrasonic pings (non-blocking)

// ============================================================
//  LINE DETECTION
// ============================================================
#define LINE_VECTOR_SIZE     20
#define HORIZONTAL_LINE_SIZE  5
#define ANGLE_THRESHOLD      15     // min degrees from horizontal to count as a lane line
#define ANGLE_ARRAY_SIZE     15

// ============================================================
//  DATA STRUCTURES
// ============================================================
typedef struct {
  double longueur;
  int    m_x0, m_x1;
  int    m_y0, m_y1;
} vectorPixy;

// ============================================================
//  GLOBALS
// ============================================================
Servo  steeringServo;
Pixy2  pixy;

vectorPixy leftVectors[LINE_VECTOR_SIZE];
vectorPixy rightVectors[LINE_VECTOR_SIZE];
vectorPixy horizontalLines[HORIZONTAL_LINE_SIZE];

int leftVectorsIndex     = 0;
int rightVectorsIndex    = 0;
int horizontalLinesIndex = 0;

// PD state
float lastError = 0.0f;

// Finish line state
int   finishline      = 0;
int   firstAngleArray = 0;
int   finalAngleArray[ANGLE_ARRAY_SIZE];
int   angleArrayIndex = 0;

// Speed
int robotspeed = SPEED_STRAIGHT;

// Timing
unsigned long raceStartTime                      = 0;
unsigned long lastHorizontalCheckTime            = 0;
unsigned long lastUltrasonicTime                 = 0;

// Ultrasonic (non-blocking state)
float         lastDistanceCm                     = 999.0f;
bool          ultrasonicTriggered                = false;
unsigned long ultrasonicTriggerTime              = 0;

// ============================================================
//  MOTOR CONTROL
// ============================================================
void moveCar(int pwmLeft, int pwmRight)
{
  if (pwmLeft == 0 && pwmRight == 0)
  {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); analogWrite(ENB, 0);
    return;
  }
  // Left motor
  if (pwmLeft > 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, pwmLeft); }
  else             { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, -pwmLeft); }
  // Right motor
  if (pwmRight > 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, pwmRight); }
  else              { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, -pwmRight); }
}

// ============================================================
//  SERVO CONTROL
// ============================================================
void setSteeringServo(int angle)
{
  angle = constrain(angle, STEERING_SERVO_MAX_LEFT, STEERING_SERVO_MAX_RIGHT);
  steeringServo.write(angle);
}

// ============================================================
//  PD CONTROLLER
//  Input:  raw angle from camera (0-180°, 90 = straight)
//  Output: servo angle to write
// ============================================================
int computePD(float cameraAngle)
{
  // Error: how far is the track direction from straight ahead
  // Positive error = track curves right  → steer right
  // Negative error = track curves left   → steer left
  float error      = cameraAngle - STEERING_SETPOINT;
  float dError     = error - lastError;
  lastError        = error;

  float correction = (KP * error) + (KD * dError);

  // Servo command: center + correction
  // Correction sign: if error is positive (track right), servo should go right (higher angle)
  int servoAngle   = DEFAULT_SERVO_ANGLE + (int)correction;

  sendData("error",        error);
  sendData("dError",       dError);
  sendData("correction",   correction);
  sendData("servo_output", servoAngle);

  return servoAngle;
}

// ============================================================
//  SPEED ADAPTATION
//  Slow down proportionally when the error is large (sharp curve)
// ============================================================
int computeSpeed(float error)
{
  float absError = abs(error);
  if (absError < CURVE_THRESHOLD)
  {
    return SPEED_STRAIGHT;
  }
  // Linear ramp from SPEED_STRAIGHT down to SPEED_CURVE
  float t = constrain((absError - CURVE_THRESHOLD) / 20.0f, 0.0f, 1.0f);
  return (int)(SPEED_STRAIGHT - t * (SPEED_STRAIGHT - SPEED_CURVE));
}

// ============================================================
//  NON-BLOCKING ULTRASONIC
//  Call this every loop. It triggers a ping, then on the next
//  call reads the echo — no delay() involved.
// ============================================================
void updateUltrasonic()
{
  unsigned long now = millis();

  if (!ultrasonicTriggered)
  {
    // Time to send a new ping?
    if (now - lastUltrasonicTime >= ULTRASONIC_INTERVAL)
    {
      digitalWrite(TRIGGER_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIGGER_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIGGER_PIN, LOW);

      ultrasonicTriggered    = true;
      ultrasonicTriggerTime  = now;
      lastUltrasonicTime     = now;
    }
  }
  else
  {
    // Read the echo (pulseIn with timeout = blocking, so we use a short one here)
    // 6ms timeout ≈ ~1m max range, fast enough to not stall the loop
    long duration = pulseIn(ECHO_PIN, HIGH, 6000);
    if (duration > 0)
    {
      lastDistanceCm = duration * 0.0343f / 2.0f;
    }
    ultrasonicTriggered = false;
  }

  sendData("distance_cm", lastDistanceCm);
}

// ============================================================
//  OBSTACLE BRAKING
//  Returns a speed multiplier (0.0 = full stop, 1.0 = full speed)
//  Call this after updateUltrasonic() so lastDistanceCm is fresh.
// ============================================================
float obstacleSpeedMultiplier()
{
  if (lastDistanceCm <= OBSTACLE_STOP_DIST)
  {
    return 0.0f;  // hard stop
  }
  if (lastDistanceCm <= OBSTACLE_SLOW_DIST)
  {
    // Linear ramp: full speed at SLOW_DIST, stop at STOP_DIST
    return (lastDistanceCm - OBSTACLE_STOP_DIST) / (OBSTACLE_SLOW_DIST - OBSTACLE_STOP_DIST);
  }
  return 1.0f;  // no obstacle nearby, full speed
}

// ============================================================
//  PIXY LINE PROCESSING
// ============================================================
void sortVectorsByLength(vectorPixy vectors[], int size)
{
  for (int i = 0; i < size - 1; i++)
    for (int j = 0; j < size - i - 1; j++)
      if (vectors[j].longueur < vectors[j + 1].longueur)
      {
        vectorPixy tmp = vectors[j]; vectors[j] = vectors[j+1]; vectors[j+1] = tmp;
      }
}

void filterLines()
{
  pixy.line.getAllFeatures();
  leftVectorsIndex = rightVectorsIndex = horizontalLinesIndex = 0;

  for (int i = 0; i < pixy.line.numVectors; i++)
  {
    float dx  = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    float dy  = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
    float len = sqrt(dx*dx + dy*dy);

    float angleDeg      = fmod(atan2(dy, dx) * (180.0f / M_PI) + 180.0f, 180.0f);
    float horizAngle    = atan2(abs(dy), abs(dx)) * 180.0f / PI;
    int   yStart        = max(pixy.line.vectors[i].m_y0, pixy.line.vectors[i].m_y1);

    // Horizontal line (finish line detection)
    if (horizAngle < 20.0f && horizontalLinesIndex < HORIZONTAL_LINE_SIZE)
    {
      horizontalLines[horizontalLinesIndex++] = { len,
        pixy.line.vectors[i].m_x0, pixy.line.vectors[i].m_x1,
        pixy.line.vectors[i].m_y0, pixy.line.vectors[i].m_y1 };
    }

    // Lane line (must be sufficiently vertical and not at the very top of frame)
    if (angleDeg > ANGLE_THRESHOLD && angleDeg < 180 - ANGLE_THRESHOLD
        && yStart > pixy.frameHeight * 0.1f)
    {
      // Normalise: m_x0/y0 = bottom (closest to car)
      int ax0, ax1, ay0, ay1;
      if (pixy.line.vectors[i].m_y0 > pixy.line.vectors[i].m_y1)
      { ax0 = pixy.line.vectors[i].m_x0; ax1 = pixy.line.vectors[i].m_x1;
        ay0 = pixy.line.vectors[i].m_y0; ay1 = pixy.line.vectors[i].m_y1; }
      else
      { ax0 = pixy.line.vectors[i].m_x1; ax1 = pixy.line.vectors[i].m_x0;
        ay0 = pixy.line.vectors[i].m_y1; ay1 = pixy.line.vectors[i].m_y0; }

      double vlen = sqrt(pow(ay1-ay0,2) + pow(ax1-ax0,2));

      if (ax0 < pixy.frameWidth / 2)
            leftVectors[leftVectorsIndex++]  = { vlen, ax0, ax1, ay0, ay1 };
      else  rightVectors[rightVectorsIndex++] = { vlen, ax0, ax1, ay0, ay1 };
    }
  }

  sortVectorsByLength(leftVectors,  leftVectorsIndex);
  sortVectorsByLength(rightVectors, rightVectorsIndex);
}

// ============================================================
//  ANGLE CALCULATION
// ============================================================
float calculateAngle()
{
  filterLines();
  static float calculatedAngle = 90.0f;

  if (leftVectorsIndex >= 1 && rightVectorsIndex >= 1)
  {
    int mx0 = (leftVectors[0].m_x0 + rightVectors[0].m_x0) / 2;
    int mx1 = (leftVectors[0].m_x1 + rightVectors[0].m_x1) / 2;
    int my0 = (leftVectors[0].m_y0 + rightVectors[0].m_y0) / 2;
    int my1 = (leftVectors[0].m_y1 + rightVectors[0].m_y1) / 2;

    int robotdistance = constrain(mx0 - pixy.frameWidth / 2, -15, 15);

    float dx = mx1 - mx0;
    float dy = my1 - my0;
    calculatedAngle = fmod(atan2(dy, dx) * (180.0f / M_PI) + 180.0f, 180.0f) + robotdistance;
  }
  else if (leftVectorsIndex >= 1)
  {
    // Only left line visible — hold last angle, minor distance nudge
    int mx0 = (leftVectors[0].m_x0 + pixy.frameWidth) / 2;
    int mx1 = (leftVectors[0].m_x1 + pixy.frameWidth) / 2;
    constrain(mx0 + mx1 - pixy.frameWidth, -5, 5); // nudge only, angle held
  }
  else if (rightVectorsIndex >= 1)
  {
    float dx = rightVectors[0].m_x1 - rightVectors[0].m_x0;
    float dy = rightVectors[0].m_y1 - rightVectors[0].m_y0;
    calculatedAngle = fmod(atan2(dy, dx) * (180.0f / M_PI) + 180.0f, 180.0f);
  }
  // No lines → hold last known angle

  sendData("camera_angle", calculatedAngle);
  return calculatedAngle;
}

// ============================================================
//  FINISH LINE DETECTION
// ============================================================
void checkForFinishLine()
{
  int k = 0;
  for (int i = 0; i < horizontalLinesIndex; i++)
    if (horizontalLines[i].longueur > 5 && horizontalLines[i].longueur < 10
        && horizontalLines[i].m_y0 > pixy.frameHeight * 0.5f)
      k++;

  if (k >= 2) finishline = 1;
}

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN,    INPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  delay(100);

  steeringServo.attach(STEERING_SERVO_PIN);
  steeringServo.write(DEFAULT_SERVO_ANGLE);

  pixy.init();
  pixy.changeProg("line");
  pixy.setLamp(0, 0);

  robotspeed   = (digitalRead(BUTTON_PIN) == LOW) ? 180 : SPEED_STRAIGHT;
  raceStartTime = millis();

  Serial.println("=== NXP Cup Ready ===");
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop()
{
  unsigned long now = millis();

  // 1. Update ultrasonic (non-blocking)
  updateUltrasonic();

  // 2. Finish line check every 10s
  if (now - lastHorizontalCheckTime >= 10000)
  {
    checkForFinishLine();
    lastHorizontalCheckTime = now;
  }

  // 3. Camera angle
  float cameraAngle = calculateAngle();
  float error       = cameraAngle - STEERING_SETPOINT;

  // 4. PD controller → servo
  int servoAngle = computePD(cameraAngle);
  setSteeringServo(servoAngle);

  // 5. Finish line approach logic
  if (finishline == 1 && (now - raceStartTime) > 10000)
  {
    // Slow approach toward obstacle
    float multiplier = obstacleSpeedMultiplier();
    int   approachSpeed = (int)(120 * multiplier);
    moveCar(approachSpeed, approachSpeed);

    if (multiplier == 0.0f)
    {
      // Stopped within OBSTACLE_STOP_DIST — hold position forever
      moveCar(0, 0);
      while (true) { delay(1000); }
    }
    return;
  }

  // 6. Normal driving — adapt speed to curve sharpness
  int speed = computeSpeed(error);
  float obstacleMult = obstacleSpeedMultiplier();
  speed = (int)(speed * obstacleMult);
  moveCar(speed, speed);
}
