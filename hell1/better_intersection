#include <Arduino.h>
#include <Pixy2.h>
#include <PWMServo.h>

// ============================================================
//  PIXY + SERVO + MOTORS
//  - Commande servo PD + filtre
//  - Moteurs L298N
//  - Vitesse réduite en courbe selon angle servo
//  - SANS SERVO CAMERA
//  - UNE SEULE LECTURE PIXY PAR BOUCLE
// ============================================================

Pixy2    pixy;
PWMServo steering;

// ============================================================
//  PINS MOTEURS (L298N)
// ============================================================
const int ENA = 2;   // PWM moteur gauche
const int ENB = 3;   // PWM moteur droit
const int IN1 = 4;   // Direction moteur gauche
const int IN2 = 5;
const int IN3 = 6;   // Direction moteur droit
const int IN4 = 7;

// ============================================================
//  PIN SERVO DIRECTION
// ============================================================
const int STEERING_SERVO_PIN = 19;

// ============================================================
//  SERVO DIRECTION
// ============================================================
const int   SERVO_CENTER_ANGLE = 105;
const int   SERVO_MIN_ANGLE    = 40;
const int   SERVO_MAX_ANGLE    = 150;
const int   STEER_SIGN         = 1;

// ============================================================
//  VITESSE MOTEURS
// ============================================================
const int MOTOR_SPEED_BASE = 200;
const int MOTOR_SPEED_MIN  = 180;

const int STEER_DEADZONE_DEG  = 5;
const int STEER_MAXCURVE_DEG  = 40;

// ============================================================
//  PD + FILTRE
// ============================================================
const float KP = 0.65f;
const float KD = 0.15f;

const int   DEADBAND_PX          = 2;
const float FILTER_ALPHA_SLOW    = 0.35f;
const float FILTER_ALPHA_FAST    = 0.65f;
const int   FAST_ERROR_THRESHOLD = 20;

// ============================================================
//  BOUCLE
// ============================================================
const unsigned long LOOP_INTERVAL_MS = 20;
unsigned long lastLoopTime = 0;

// ============================================================
//  PIXY FILTERING
// ============================================================
const int   LINE_VECTOR_SIZE     = 20;
const int   HORIZONTAL_LINE_SIZE = 5;
const int   ANGLE_THRESHOLD_DEG  = 15;
const float HORIZONTAL_MAX_DEG   = 20.0f;
const float CURVE_LANE_FACTOR    = 0.7f;

// ---------- NEW: intersection robustness ----------
const float PRIMARY_VECTOR_MIN_BOTTOM_Y_RATIO = 0.45f;
const int   LANE_WIDTH_MIN_PX                 = 20;
const int   LANE_WIDTH_JUMP_PX                = 22;
const float PAIR_MIN_LENGTH_RATIO             = 0.35f;

typedef struct vectorPixy
{
  float longueur;
  int   m_x1, m_x0;
  int   m_y1, m_y0;
} vectorPixy;

vectorPixy leftVectors[LINE_VECTOR_SIZE];
vectorPixy rightVectors[LINE_VECTOR_SIZE];
vectorPixy horizontalLines[HORIZONTAL_LINE_SIZE];

int leftVectorsIndex     = 0;
int rightVectorsIndex    = 0;
int horizontalLinesIndex = 0;

// ============================================================
//  ÉTAT
// ============================================================
float lastError          = 0.0f;
float filteredTargetX    = 0.0f;
bool  filterInitialized  = false;

int  lastLaneWidthPx     = 0;
bool laneWidthCalibrated = false;

int lastSteerAngle = SERVO_CENTER_ANGLE;

// ============================================================
//  MOTEURS
// ============================================================
void setMotors(int speed)
{
  speed = constrain(speed, 0, 255);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, speed);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENB, speed);
}

void stopMotors()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 0);
}

int computeSpeed(int steerAngle)
{
  int deviation = abs(steerAngle - SERVO_CENTER_ANGLE);

  if (deviation <= STEER_DEADZONE_DEG)
  {
    return MOTOR_SPEED_BASE;
  }
  else if (deviation >= STEER_MAXCURVE_DEG)
  {
    return MOTOR_SPEED_MIN;
  }
  else
  {
    return map(deviation,
               STEER_DEADZONE_DEG, STEER_MAXCURVE_DEG,
               MOTOR_SPEED_BASE,   MOTOR_SPEED_MIN);
  }
}

// ============================================================
//  HELPERS PIXY
// ============================================================
void sortVectorsByLength(vectorPixy vectors[], int size)
{
  for (int i = 0; i < size - 1; i++)
  {
    for (int j = 0; j < size - i - 1; j++)
    {
      if (vectors[j].longueur < vectors[j + 1].longueur)
      {
        vectorPixy tmp  = vectors[j];
        vectors[j]      = vectors[j + 1];
        vectors[j + 1]  = tmp;
      }
    }
  }
}

void setSteeringServo(int angle)
{
  angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  steering.write(angle);
}

void getBottomTopFromPixyIndex(uint8_t i,
                               int &bottomX, int &bottomY,
                               int &topX,    int &topY)
{
  if (pixy.line.vectors[i].m_y0 >= pixy.line.vectors[i].m_y1)
  {
    bottomX = pixy.line.vectors[i].m_x0;
    bottomY = pixy.line.vectors[i].m_y0;
    topX    = pixy.line.vectors[i].m_x1;
    topY    = pixy.line.vectors[i].m_y1;
  }
  else
  {
    bottomX = pixy.line.vectors[i].m_x1;
    bottomY = pixy.line.vectors[i].m_y1;
    topX    = pixy.line.vectors[i].m_x0;
    topY    = pixy.line.vectors[i].m_y0;
  }
}

// ============================================================
//  FILTRAGE DES LIGNES PIXY
// ============================================================
void filterLines()
{
  leftVectorsIndex = 0;
  rightVectorsIndex = 0;
  horizontalLinesIndex = 0;

  for (uint8_t i = 0; i < pixy.line.numVectors; i++)
  {
    float dx  = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    float dy  = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
    float len = sqrtf(dx * dx + dy * dy);

    float angleDeg        = fmodf(atan2f(dy, dx) * (180.0f / PI) + 180.0f, 180.0f);
    float horizontalAngle = atan2f(fabsf(dy), fabsf(dx)) * 180.0f / PI;

    if (horizontalAngle < HORIZONTAL_MAX_DEG && horizontalLinesIndex < HORIZONTAL_LINE_SIZE)
    {
      horizontalLines[horizontalLinesIndex] = {
        len,
        pixy.line.vectors[i].m_x1, pixy.line.vectors[i].m_x0,
        pixy.line.vectors[i].m_y1, pixy.line.vectors[i].m_y0
      };
      horizontalLinesIndex++;
    }

    int yStart = max(pixy.line.vectors[i].m_y0, pixy.line.vectors[i].m_y1);

    if (angleDeg > ANGLE_THRESHOLD_DEG &&
        angleDeg < (180 - ANGLE_THRESHOLD_DEG) &&
        yStart > pixy.frameHeight * 0.1f)
    {
      int bX, bY, tX, tY;
      getBottomTopFromPixyIndex(i, bX, bY, tX, tY);
      float vlen = sqrtf((float)(tY - bY) * (tY - bY) + (float)(tX - bX) * (tX - bX));

      if (bX < pixy.frameWidth / 2)
      {
        if (leftVectorsIndex < LINE_VECTOR_SIZE)
          leftVectors[leftVectorsIndex++] = { vlen, tX, bX, tY, bY };
      }
      else
      {
        if (rightVectorsIndex < LINE_VECTOR_SIZE)
          rightVectors[rightVectorsIndex++] = { vlen, tX, bX, tY, bY };
      }
    }
  }

  sortVectorsByLength(leftVectors,  leftVectorsIndex);
  sortVectorsByLength(rightVectors, rightVectorsIndex);
}

// ============================================================
//  NEW HELPERS FOR ROBUST INTERSECTION HANDLING
// ============================================================
bool isPrimaryVector(const vectorPixy &v)
{
  return v.m_y0 >= (int)(pixy.frameHeight * PRIMARY_VECTOR_MIN_BOTTOM_Y_RATIO);
}

int pickBestVectorIndex(vectorPixy vectors[], int count)
{
  if (count <= 0) return -1;

  for (int i = 0; i < count; i++)
  {
    if (isPrimaryVector(vectors[i]))
      return i;
  }

  return 0;
}

void buildTargetFromLeftVector(const vectorPixy &v,
                               float &targetX,
                               int &xBasCentre,
                               int &xHautCentre)
{
  int offset = (lastLaneWidthPx > 0) ? (int)(lastLaneWidthPx * CURVE_LANE_FACTOR) : 40;
  xBasCentre  = v.m_x0 + offset;
  xHautCentre = v.m_x1 + offset;
  targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;
}

void buildTargetFromRightVector(const vectorPixy &v,
                                float &targetX,
                                int &xBasCentre,
                                int &xHautCentre)
{
  int offset = (lastLaneWidthPx > 0) ? (int)(lastLaneWidthPx * CURVE_LANE_FACTOR) : 40;
  xBasCentre  = v.m_x0 - offset;
  xHautCentre = v.m_x1 - offset;
  targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;
}

void updateLaneWidthMemory(int measuredWidth)
{
  if (measuredWidth > LANE_WIDTH_MIN_PX && measuredWidth < pixy.frameWidth)
  {
    if (!laneWidthCalibrated)
    {
      lastLaneWidthPx = measuredWidth;
      laneWidthCalibrated = true;
    }
    else
    {
      lastLaneWidthPx = (int)(0.9f * lastLaneWidthPx + 0.1f * measuredWidth);
    }
  }
}

// ============================================================
//  CALCUL DE LA CIBLE X
// ============================================================
bool computeTargetX(float &targetX, int &xBasCentre, int &xHautCentre)
{
  filterLines();

  int frameCenterX = pixy.frameWidth / 2;
  xBasCentre = xHautCentre = frameCenterX;

  int leftIdx  = pickBestVectorIndex(leftVectors, leftVectorsIndex);
  int rightIdx = pickBestVectorIndex(rightVectors, rightVectorsIndex);

  bool hasLeft  = (leftIdx >= 0);
  bool hasRight = (rightIdx >= 0);

  // ==========================================================
  // CAS 1 : deux lignes candidates
  // ==========================================================
  if (hasLeft && hasRight)
  {
    const vectorPixy &L = leftVectors[leftIdx];
    const vectorPixy &R = rightVectors[rightIdx];

    int measuredWidth = R.m_x0 - L.m_x0;

    float longerLen  = (L.longueur > R.longueur) ? L.longueur : R.longueur;
    float shorterLen = (L.longueur < R.longueur) ? L.longueur : R.longueur;

    bool widthPlausible = (measuredWidth > LANE_WIDTH_MIN_PX &&
                           measuredWidth < (pixy.frameWidth - 6));

    int widthTolerance = laneWidthCalibrated
                         ? max(LANE_WIDTH_JUMP_PX, lastLaneWidthPx / 3)
                         : LANE_WIDTH_JUMP_PX;

    bool widthStable = (!laneWidthCalibrated ||
                        abs(measuredWidth - lastLaneWidthPx) <= widthTolerance);

    bool lengthBalanced = (longerLen > 0.0f) &&
                          ((shorterLen / longerLen) >= PAIR_MIN_LENGTH_RATIO);

    bool bottomsCoherent = abs(L.m_y0 - R.m_y0) <= (int)(pixy.frameHeight * 0.35f);

    bool pairReliable = widthPlausible && widthStable && lengthBalanced && bottomsCoherent;

    // ------- normal case: both borders are coherent -------
    if (pairReliable)
    {
      xBasCentre  = (L.m_x0 + R.m_x0) / 2;
      xHautCentre = (L.m_x1 + R.m_x1) / 2;
      targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;

      updateLaneWidthMemory(measuredWidth);
      return true;
    }

    // ======================================================
    // AMBIGUOUS / INTERSECTION CASE
    // ======================================================
    bool leftPrimary  = isPrimaryVector(L);
    bool rightPrimary = isPrimaryVector(R);

    if (leftPrimary && !rightPrimary)
    {
      buildTargetFromLeftVector(L, targetX, xBasCentre, xHautCentre);
      return true;
    }

    if (!leftPrimary && rightPrimary)
    {
      buildTargetFromRightVector(R, targetX, xBasCentre, xHautCentre);
      return true;
    }

    if (L.longueur > R.longueur * 1.35f)
    {
      buildTargetFromLeftVector(L, targetX, xBasCentre, xHautCentre);
      return true;
    }

    if (R.longueur > L.longueur * 1.35f)
    {
      buildTargetFromRightVector(R, targetX, xBasCentre, xHautCentre);
      return true;
    }

    // Final fallback: stay straight instead of following a fake branch
    targetX     = frameCenterX;
    xBasCentre  = frameCenterX;
    xHautCentre = frameCenterX;
    return true;
  }

  // ==========================================================
  // CAS 2 : ligne gauche seulement
  // ==========================================================
  else if (hasLeft && !hasRight)
  {
    buildTargetFromLeftVector(leftVectors[leftIdx], targetX, xBasCentre, xHautCentre);
    return true;
  }

  // ==========================================================
  // CAS 3 : ligne droite seulement
  // ==========================================================
  else if (!hasLeft && hasRight)
  {
    buildTargetFromRightVector(rightVectors[rightIdx], targetX, xBasCentre, xHautCentre);
    return true;
  }

  // ==========================================================
  // CAS 4 : aucune ligne
  // ==========================================================
  targetX = frameCenterX;
  return false;
}

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
  stopMotors();

  steering.attach(STEERING_SERVO_PIN);
  setSteeringServo(SERVO_CENTER_ANGLE);
  lastSteerAngle = SERVO_CENTER_ANGLE;

  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  Serial.println("=== Pixy + Servo + Motors ===");
  delay(500);
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  int8_t res = pixy.line.getAllFeatures(LINE_VECTOR);

  if (res <= 0 || pixy.line.numVectors == 0)
  {
    setSteeringServo(lastSteerAngle);
    setMotors(MOTOR_SPEED_MIN);

    Serial.print("No vector -> keep angle: ");
    Serial.print(lastSteerAngle);
    Serial.print("  speed: ");
    Serial.println(MOTOR_SPEED_MIN);
    return;
  }

  float targetX    = 0.0f;
  int frameCenterX = pixy.frameWidth / 2;
  int xBasCentre   = frameCenterX;
  int xHautCentre  = frameCenterX;

  bool ok = computeTargetX(targetX, xBasCentre, xHautCentre);

  if (!ok)
  {
    setSteeringServo(lastSteerAngle);
    setMotors(MOTOR_SPEED_MIN);

    Serial.print("No valid line -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  float rawError = targetX - frameCenterX;
  float alpha    = (abs((int)rawError) > FAST_ERROR_THRESHOLD)
                   ? FILTER_ALPHA_FAST
                   : FILTER_ALPHA_SLOW;

  if (!filterInitialized)
  {
    filteredTargetX = targetX;
    filterInitialized = true;
  }
  else
  {
    filteredTargetX = alpha * targetX + (1.0f - alpha) * filteredTargetX;
  }

  float error = filteredTargetX - frameCenterX;
  if (abs((int)error) <= DEADBAND_PX) error = 0.0f;

  float dError   = error - lastError;
  lastError      = error;

  float command  = SERVO_CENTER_ANGLE + STEER_SIGN * (KP * error + KD * dError);
  int steerAngle = constrain((int)command, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  setSteeringServo(steerAngle);
  lastSteerAngle = steerAngle;

  int speed = computeSpeed(steerAngle);
  setMotors(speed);

  Serial.print("Lcnt=");    Serial.print(leftVectorsIndex);
  Serial.print("  Rcnt=");  Serial.print(rightVectorsIndex);
  Serial.print("  LaneW="); Serial.print(lastLaneWidthPx);
  Serial.print("  Err=");   Serial.print(error);
  Serial.print("  Servo="); Serial.print(steerAngle);
  Serial.print("  Speed="); Serial.println(speed);
}
