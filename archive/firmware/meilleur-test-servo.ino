#include <Pixy2.h>
#include <PWMServo.h>

// ============================================================
//  PIXY + SERVO DIRECTION
//  VERSION PLUS REACTIVE
// ============================================================

Pixy2 pixy;
PWMServo steering;

// ============================================================
//  PIN SERVO
// ============================================================
const int STEERING_SERVO_PIN = 19;

// ============================================================
//  SERVO DIRECTION
// ============================================================
const int SERVO_CENTER_ANGLE = 95;
const int SERVO_MIN_ANGLE    = 40;
const int SERVO_MAX_ANGLE    = 150;
const int STEER_SIGN         = 1;   // garde ton signe actuel

// amplitude max autour du centre
const int MAX_STEER_DELTA = 35;

// ============================================================
//  CALIBRATION CENTRE CAMERA
// ============================================================
const int CAMERA_X_OFFSET = 2;

// ============================================================
//  PD + FILTRE
// ============================================================
// AVANT : KP=0.45 KD=0.25
// MAINTENANT : plus agressif
const float KP = 0.95f;
const float KD = 0.35f;

// poids de l'orientation de la route
const float HEADING_WEIGHT = 0.30f;

const int   DEADBAND_PX = 2;

// filtre un peu moins lent
const float FILTER_ALPHA_SLOW = 0.50f;
const float FILTER_ALPHA_FAST = 0.80f;
const int   FAST_ERROR_THRESHOLD = 15;

// ============================================================
//  BOUCLE
// ============================================================
const unsigned long LOOP_INTERVAL_MS = 20;
unsigned long lastLoopTime = 0;

// ============================================================
//  PIXY FILTERING
// ============================================================
const int LINE_VECTOR_SIZE      = 20;
const int HORIZONTAL_LINE_SIZE  = 5;
const int ANGLE_THRESHOLD_DEG   = 15;
const float HORIZONTAL_MAX_DEG  = 20.0f;

typedef struct vectorPixy
{
  float longueur;
  int   m_x1;
  int   m_x0;
  int   m_y1;
  int   m_y0;
} vectorPixy;

vectorPixy leftVectors[LINE_VECTOR_SIZE];
vectorPixy rightVectors[LINE_VECTOR_SIZE];
vectorPixy horizontalLines[HORIZONTAL_LINE_SIZE];

int leftVectorsIndex = 0;
int rightVectorsIndex = 0;
int horizontalLinesIndex = 0;

// ============================================================
//  ÉTAT
// ============================================================
float lastError = 0.0f;
float filteredError = 0.0f;
bool  filterInitialized = false;

int lastLaneWidthPx = 0;
bool laneWidthCalibrated = false;

int lastSteerAngle = SERVO_CENTER_ANGLE;

// ============================================================
//  HELPERS
// ============================================================
void sortVectorsByLength(vectorPixy vectors[], int size)
{
  for (int i = 0; i < size - 1; i++)
  {
    for (int j = 0; j < size - i - 1; j++)
    {
      if (vectors[j].longueur < vectors[j + 1].longueur)
      {
        vectorPixy tmp = vectors[j];
        vectors[j] = vectors[j + 1];
        vectors[j + 1] = tmp;
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
    float dx = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    float dy = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
    float len = sqrtf(dx * dx + dy * dy);

    float angleDeg = fmodf(atan2f(dy, dx) * (180.0f / PI) + 180.0f, 180.0f);
    float horizontalAngle = atan2f(abs(dy), abs(dx)) * 180.0f / PI;

    if (horizontalAngle < HORIZONTAL_MAX_DEG && horizontalLinesIndex < HORIZONTAL_LINE_SIZE)
    {
      horizontalLines[horizontalLinesIndex].m_x0 = pixy.line.vectors[i].m_x0;
      horizontalLines[horizontalLinesIndex].m_x1 = pixy.line.vectors[i].m_x1;
      horizontalLines[horizontalLinesIndex].m_y0 = pixy.line.vectors[i].m_y0;
      horizontalLines[horizontalLinesIndex].m_y1 = pixy.line.vectors[i].m_y1;
      horizontalLines[horizontalLinesIndex].longueur = len;
      horizontalLinesIndex++;
    }

    int yStart = max(pixy.line.vectors[i].m_y0, pixy.line.vectors[i].m_y1);

    if (angleDeg > ANGLE_THRESHOLD_DEG &&
        angleDeg < (180 - ANGLE_THRESHOLD_DEG) &&
        yStart > pixy.frameHeight * 0.1f)
    {
      int bottomX, bottomY, topX, topY;
      getBottomTopFromPixyIndex(i, bottomX, bottomY, topX, topY);

      float vlen = sqrtf((topY - bottomY) * (topY - bottomY) +
                         (topX - bottomX) * (topX - bottomX));

      if (bottomX < pixy.frameWidth / 2)
      {
        if (leftVectorsIndex < LINE_VECTOR_SIZE)
        {
          leftVectors[leftVectorsIndex].longueur = vlen;
          leftVectors[leftVectorsIndex].m_x0 = bottomX;
          leftVectors[leftVectorsIndex].m_x1 = topX;
          leftVectors[leftVectorsIndex].m_y0 = bottomY;
          leftVectors[leftVectorsIndex].m_y1 = topY;
          leftVectorsIndex++;
        }
      }
      else
      {
        if (rightVectorsIndex < LINE_VECTOR_SIZE)
        {
          rightVectors[rightVectorsIndex].longueur = vlen;
          rightVectors[rightVectorsIndex].m_x0 = bottomX;
          rightVectors[rightVectorsIndex].m_x1 = topX;
          rightVectors[rightVectorsIndex].m_y0 = bottomY;
          rightVectors[rightVectorsIndex].m_y1 = topY;
          rightVectorsIndex++;
        }
      }
    }
  }

  sortVectorsByLength(leftVectors, leftVectorsIndex);
  sortVectorsByLength(rightVectors, rightVectorsIndex);
}

// ============================================================
//  CALCUL DES CENTRES
// ============================================================
bool computeCenters(int &xBasCentre, int &xHautCentre)
{
  filterLines();

  int frameCenterX = pixy.frameWidth / 2;
  xBasCentre = frameCenterX;
  xHautCentre = frameCenterX;

  if (leftVectorsIndex >= 1 && rightVectorsIndex >= 1)
  {
    xBasCentre  = (leftVectors[0].m_x0 + rightVectors[0].m_x0) / 2;
    xHautCentre = (leftVectors[0].m_x1 + rightVectors[0].m_x1) / 2;

    int measuredWidth = rightVectors[0].m_x0 - leftVectors[0].m_x0;
    if (measuredWidth > 10 && measuredWidth < pixy.frameWidth)
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
    return true;
  }
  else if (leftVectorsIndex >= 1 && rightVectorsIndex == 0)
  {
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    xBasCentre  = leftVectors[0].m_x0 + halfLane;
    xHautCentre = leftVectors[0].m_x1 + halfLane;
    return true;
  }
  else if (leftVectorsIndex == 0 && rightVectorsIndex >= 1)
  {
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    xBasCentre  = rightVectors[0].m_x0 - halfLane;
    xHautCentre = rightVectors[0].m_x1 - halfLane;
    return true;
  }

  return false;
}

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  steering.attach(STEERING_SERVO_PIN);
  setSteeringServo(SERVO_CENTER_ANGLE);
  lastSteerAngle = SERVO_CENTER_ANGLE;

  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  Serial.println("=== Pixy + Steering Servo More Aggressive ===");
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
    Serial.print("No vector -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  int frameCenterX = pixy.frameWidth / 2;
  int xBasCentre = frameCenterX;
  int xHautCentre = frameCenterX;

  bool ok = computeCenters(xBasCentre, xHautCentre);

  if (!ok)
  {
    setSteeringServo(lastSteerAngle);
    Serial.print("No valid line -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  int centerRef = frameCenterX + CAMERA_X_OFFSET;

  float lateralError = xBasCentre - centerRef;
  float headingError = xHautCentre - xBasCentre;

  // plus agressif qu'avant
  float rawError = lateralError + HEADING_WEIGHT * headingError;

  float alpha = (abs((int)rawError) > FAST_ERROR_THRESHOLD)
                ? FILTER_ALPHA_FAST
                : FILTER_ALPHA_SLOW;

  if (!filterInitialized)
  {
    filteredError = rawError;
    filterInitialized = true;
  }
  else
  {
    filteredError = alpha * rawError + (1.0f - alpha) * filteredError;
  }

  float error = filteredError;

  if (abs((int)error) <= DEADBAND_PX)
    error = 0.0f;

  float dError = error - lastError;
  lastError = error;

  // delta servo autour du centre
  float steerDelta = KP * error + KD * dError;
  steerDelta = constrain(steerDelta, -MAX_STEER_DELTA, MAX_STEER_DELTA);

  int steerAngle = SERVO_CENTER_ANGLE + STEER_SIGN * (int)steerDelta;
  steerAngle = constrain(steerAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  setSteeringServo(steerAngle);
  lastSteerAngle = steerAngle;

  Serial.print("LatErr=");
  Serial.print(lateralError);
  Serial.print("  HeadErr=");
  Serial.print(headingError);
  Serial.print("  Err=");
  Serial.print(error);
  Serial.print("  Delta=");
  Serial.print(steerDelta);
  Serial.print("  Servo=");
  Serial.println(steerAngle);
}