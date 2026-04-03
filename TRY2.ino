#include <Arduino.h>
#include <Pixy2.h>
#include <PWMServo.h>

// ============================================================
//  OBJETS
// ============================================================
Pixy2 pixy;
PWMServo steering;

// ============================================================
//  PINS
// ============================================================
#define ENA 2
#define ENB 3
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

#define SERVO_PIN    19
#define TRIGGER_PIN  23
#define ECHO_PIN     22
#define BUTTON_PIN   20

// ============================================================
//  SERVO / CAMERA CONTROL
// ============================================================
const int SERVO_CENTER_ANGLE = 98;
const int SERVO_MIN_ANGLE    = 40;
const int SERVO_MAX_ANGLE    = 140;

const int   STEER_SIGN    = 1;      // si le servo tourne à l'envers -> mettre -1
const float KP            = 0.35f;
const float KD            = 0.18f;
const float FILTER_ALPHA  = 0.25f;
const int   DEADBAND_PX   = 2;

float lastError = 0.0f;
float filteredTargetX = 0.0f;
bool  filterInitialized = false;

int lastLaneWidthPx = 30;

// ============================================================
//  SPEED
// ============================================================
const int SPEED_STRAIGHT        = 150;
const int SPEED_MEDIUM          = 140;
const int SPEED_CURVE           = 125;
const int SPEED_LOST            = 110;
const int SPEED_FINISH_APPROACH = 105;

// ============================================================
//  ULTRASON / OBSTACLE
// ============================================================
const float OBSTACLE_STOP_CM      = 12.0f;
const float OBSTACLE_SLOW_CM      = 35.0f;
const float OBSTACLE_IGNORE_CM    = 250.0f;

// ============================================================
//  FINISH LINE / INTERSECTION
// ============================================================
const int HORIZONTAL_MIN_LEN           = 8;
const int HORIZONTAL_MAX_LEN           = 80;
const int FINISHLINE_HORIZONTAL_COUNT  = 2;
const unsigned long INTERSECTION_HOLD_MS = 180;
const unsigned long START_IGNORE_MS      = 3000;

// ============================================================
//  STRUCTURES
// ============================================================
struct SideLine
{
  bool valid;
  int bottomX;
  int topX;
};

struct HorizontalInfo
{
  int count;
  int lowCount;   // horizontales dans la moitié basse
};

// ============================================================
//  ETAT GLOBAL
// ============================================================
bool finishlineDetected = false;
bool obstacleApproachMode = false;
unsigned long raceStartMs = 0;
unsigned long holdStraightUntil = 0;

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // moteurs arrêtés au début
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  // servo au centre
  steering.attach(SERVO_PIN);
  steering.write(SERVO_CENTER_ANGLE);
  delay(1000);

  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  raceStartMs = millis();

  Serial.println("NXP final code ready");
}

// ============================================================
//  MOTEURS
// ============================================================
void moveCar(int pwmLeft, int pwmRight)
{
  if (pwmLeft == 0 && pwmRight == 0)
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
    return;
  }

  // Left motor
  if (pwmLeft > 0)
  {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    analogWrite(ENB, pwmLeft);
  }
  else
  {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, -pwmLeft);
  }

  // Right motor
  if (pwmRight > 0)
  {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, pwmRight);
  }
  else
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, -pwmRight);
  }
}

// ============================================================
//  ULTRASON
// ============================================================
float readDistanceCm()
{
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
    return 999.0f;

  float d = duration * 0.0343f / 2.0f;

  if (d > OBSTACLE_IGNORE_CM)
    return 999.0f;

  return d;
}

// ============================================================
//  PIXY HELPERS
// ============================================================
static void getBottomTop(const Vector &v, int &bottomX, int &bottomY, int &topX, int &topY)
{
  if (v.m_y0 >= v.m_y1)
  {
    bottomX = v.m_x0;
    bottomY = v.m_y0;
    topX    = v.m_x1;
    topY    = v.m_y1;
  }
  else
  {
    bottomX = v.m_x1;
    bottomY = v.m_y1;
    topX    = v.m_x0;
    topY    = v.m_y0;
  }
}

bool isMostlyHorizontal(int x0, int y0, int x1, int y1)
{
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  return (dx > 2 * dy);
}

int vectorLength(int x0, int y0, int x1, int y1)
{
  int dx = x1 - x0;
  int dy = y1 - y0;
  return (int)sqrt((float)(dx * dx + dy * dy));
}

int computeSpeedFromServo(int steerAngle)
{
  int delta = abs(steerAngle - SERVO_CENTER_ANGLE);

  if (delta < 6)  return SPEED_STRAIGHT;
  if (delta < 14) return SPEED_MEDIUM;
  return SPEED_CURVE;
}

// ============================================================
//  ANALYSE DES VECTEURS PIXY
// ============================================================
void extractLinesAndEvents(SideLine &left, SideLine &right, HorizontalInfo &hz)
{
  int frameCenterX = pixy.frameWidth / 2;

  left.valid = false;
  right.valid = false;
  left.bottomX = right.bottomX = 0;
  left.topX = right.topX = 0;

  hz.count = 0;
  hz.lowCount = 0;

  int bestLeftDist = 10000;
  int bestRightDist = 10000;

  for (uint8_t i = 0; i < pixy.line.numVectors; i++)
  {
    const Vector &v = pixy.line.vectors[i];

    int bottomX, bottomY, topX, topY;
    getBottomTop(v, bottomX, bottomY, topX, topY);

    int len = vectorLength(bottomX, bottomY, topX, topY);

    // ---------- horizontales : finish line / intersection ----------
    if (isMostlyHorizontal(bottomX, bottomY, topX, topY) &&
        len >= HORIZONTAL_MIN_LEN &&
        len <= HORIZONTAL_MAX_LEN)
    {
      hz.count++;

      if (bottomY > pixy.frameHeight / 2)
        hz.lowCount++;

      continue;
    }

    // ---------- lignes gauche / droite ----------
    if (bottomX <= frameCenterX)
    {
      int dist = frameCenterX - bottomX;
      if (dist < bestLeftDist)
      {
        bestLeftDist = dist;
        left.valid = true;
        left.bottomX = bottomX;
        left.topX = topX;
      }
    }
    else
    {
      int dist = bottomX - frameCenterX;
      if (dist < bestRightDist)
      {
        bestRightDist = dist;
        right.valid = true;
        right.bottomX = bottomX;
        right.topX = topX;
      }
    }
  }
}

// ============================================================
//  CALCUL SERVO A PARTIR DE TON ALGO
// ============================================================
int computeSteerFromLines(const SideLine &left, const SideLine &right, int frameCenterX)
{
  float targetX;

  if (left.valid && right.valid)
  {
    float centerNear = (left.bottomX + right.bottomX) * 0.5f;
    float centerFar  = (left.topX + right.topX) * 0.5f;

    // plus de poids au haut de l'image pour anticiper le virage
    targetX = 0.35f * centerNear + 0.65f * centerFar;
    lastLaneWidthPx = right.bottomX - left.bottomX;
  }
  else if (left.valid)
  {
    targetX = left.bottomX + lastLaneWidthPx * 0.5f;
  }
  else if (right.valid)
  {
    targetX = right.bottomX - lastLaneWidthPx * 0.5f;
  }
  else
  {
    targetX = frameCenterX;
  }

  if (!filterInitialized)
  {
    filteredTargetX = targetX;
    filterInitialized = true;
  }
  else
  {
    filteredTargetX = FILTER_ALPHA * targetX + (1.0f - FILTER_ALPHA) * filteredTargetX;
  }

  float error = filteredTargetX - frameCenterX;

  if (abs((int)error) <= DEADBAND_PX)
    error = 0.0f;

  float dError = error - lastError;
  lastError = error;

  int steerAngle = SERVO_CENTER_ANGLE + STEER_SIGN * (int)(KP * error + KD * dError);
  steerAngle = constrain(steerAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  return steerAngle;
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  float distanceCm = readDistanceCm();

  int8_t res = pixy.line.getAllFeatures(LINE_VECTOR);
  int frameCenterX = pixy.frameWidth / 2;

  // ----------------------------------------------------------
  // 1) SI PAS DE VECTEURS
  // ----------------------------------------------------------
  if (res <= 0 || pixy.line.numVectors == 0)
  {
    steering.write(SERVO_CENTER_ANGLE);

    if (obstacleApproachMode)
      moveCar(SPEED_FINISH_APPROACH, SPEED_FINISH_APPROACH);
    else
      moveCar(SPEED_LOST, SPEED_LOST);

    Serial.print("No vector | dist=");
    Serial.print(distanceCm);
    Serial.print(" | finish=");
    Serial.println(obstacleApproachMode);
    delay(20);
    return;
  }

  // ----------------------------------------------------------
  // 2) EXTRACTION DES LIGNES
  // ----------------------------------------------------------
  SideLine left, right;
  HorizontalInfo hz;
  extractLinesAndEvents(left, right, hz);

  // ----------------------------------------------------------
  // 3) DETECTION INTERSECTION
  // ----------------------------------------------------------
  if (hz.count >= 2 && !obstacleApproachMode)
  {
    holdStraightUntil = millis() + INTERSECTION_HOLD_MS;
  }

  // ----------------------------------------------------------
  // 4) DETECTION FINISH LINE
  // ----------------------------------------------------------
  if (!finishlineDetected &&
      (millis() - raceStartMs) > START_IGNORE_MS &&
      hz.lowCount >= FINISHLINE_HORIZONTAL_COUNT)
  {
    finishlineDetected = true;
    obstacleApproachMode = true;
  }

  // ----------------------------------------------------------
  // 5) MODE FINISH LINE + OBSTACLE
  // ----------------------------------------------------------
  if (obstacleApproachMode)
  {
    // obstacle proche -> arrêt complet
    if (distanceCm <= OBSTACLE_STOP_CM)
    {
      moveCar(0, 0);
      steering.write(SERVO_CENTER_ANGLE);

      Serial.print("STOP FINAL obstacle=");
      Serial.println(distanceCm);
      delay(50);
      return;
    }

    // ralentissement progressif
    int speedCmd = SPEED_FINISH_APPROACH;
    if (distanceCm < OBSTACLE_SLOW_CM && distanceCm > OBSTACLE_STOP_CM)
    {
      float t = (distanceCm - OBSTACLE_STOP_CM) / (OBSTACLE_SLOW_CM - OBSTACLE_STOP_CM);
      t = constrain(t, 0.0f, 1.0f);
      speedCmd = (int)(95 + t * (SPEED_FINISH_APPROACH - 95));
    }

    int steerAngle = computeSteerFromLines(left, right, frameCenterX);

    // dans une petite fenêtre après détection d'intersection, garde presque droit
    if (millis() < holdStraightUntil)
      steerAngle = SERVO_CENTER_ANGLE;

    steering.write(steerAngle);
    moveCar(speedCmd, speedCmd);

    Serial.print("FINAL | vec=");
    Serial.print(pixy.line.numVectors);
    Serial.print(" | hz=");
    Serial.print(hz.lowCount);
    Serial.print(" | dist=");
    Serial.print(distanceCm);
    Serial.print(" | servo=");
    Serial.print(steerAngle);
    Serial.print(" | speed=");
    Serial.println(speedCmd);

    delay(20);
    return;
  }

  // ----------------------------------------------------------
  // 6) MODE NORMAL
  // ----------------------------------------------------------
  int steerAngle = computeSteerFromLines(left, right, frameCenterX);

  if (millis() < holdStraightUntil)
    steerAngle = SERVO_CENTER_ANGLE;

  steering.write(steerAngle);

  int speedCmd = computeSpeedFromServo(steerAngle);

  // sécurité obstacle en mode normal aussi
  if (distanceCm <= OBSTACLE_STOP_CM)
  {
    moveCar(0, 0);
    Serial.print("STOP obstacle=");
    Serial.println(distanceCm);
    delay(30);
    return;
  }

  moveCar(speedCmd, speedCmd);

  Serial.print("vec=");
  Serial.print(pixy.line.numVectors);
  Serial.print(" L=");
  if (left.valid) Serial.print(left.bottomX); else Serial.print("X");
  Serial.print(" R=");
  if (right.valid) Serial.print(right.bottomX); else Serial.print("X");
  Serial.print(" hz=");
  Serial.print(hz.count);
  Serial.print(" target=");
  Serial.print(filteredTargetX);
  Serial.print(" err=");
  Serial.print(lastError);
  Serial.print(" servo=");
  Serial.print(steerAngle);
  Serial.print(" dist=");
  Serial.print(distanceCm);
  Serial.print(" speed=");
  Serial.print(speedCmd);
  Serial.print(" finish=");
  Serial.println(obstacleApproachMode ? 1 : 0);

  delay(20);
}