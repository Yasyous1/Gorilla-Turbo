#include <Pixy2.h>
#include <PWMServo.h>

// ============================================================
//  PIXY + SERVO FUSION
//  - Traitement Pixy robuste du code complet
//  - Commande servo PD + filtre du code 1
// ============================================================

Pixy2 pixy;
PWMServo steering;

// ============================================================
//  PINS
// ============================================================
const int STEERING_SERVO_PIN = 19;
const int CAMERA_SERVO_PIN   = 22;   // optionnel

// mettre false si tu n'utilises pas le servo caméra
const bool USE_CAMERA_SERVO = false;
const int  CAMERA_FIXED_ANGLE = 45;

// ============================================================
//  SERVO DIRECTION
// ============================================================
const int SERVO_CENTER_ANGLE = 95;
const int SERVO_MIN_ANGLE    = 40;
const int SERVO_MAX_ANGLE    = 150;
const int STEER_SIGN         = 1;   // mettre -1 si le sens est inversé

// ============================================================
//  PD + FILTRE
// ============================================================
const float KP = 0.45f;
const float KD = 0.25f;

const int   DEADBAND_PX = 2;

const float FILTER_ALPHA_SLOW = 0.35f;
const float FILTER_ALPHA_FAST = 0.65f;
const int   FAST_ERROR_THRESHOLD = 20;

// ============================================================
//  BOUCLE
// ============================================================
const unsigned long LOOP_INTERVAL_MS = 20;   // 50 Hz
unsigned long lastLoopTime = 0;

// ============================================================
//  PIXY FILTERING
// ============================================================
const int LINE_VECTOR_SIZE      = 20;
const int HORIZONTAL_LINE_SIZE  = 5;
const int ANGLE_THRESHOLD_DEG   = 15;   // pour considérer une ligne "verticale"
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
float filteredTargetX = 0.0f;
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
  pixy.line.getAllFeatures();

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

    // 1) stocker lignes quasi horizontales (utile si plus tard tu veux détecter croisement / finish)
    if (horizontalAngle < HORIZONTAL_MAX_DEG && horizontalLinesIndex < HORIZONTAL_LINE_SIZE)
    {
      horizontalLines[horizontalLinesIndex].m_x0 = pixy.line.vectors[i].m_x0;
      horizontalLines[horizontalLinesIndex].m_x1 = pixy.line.vectors[i].m_x1;
      horizontalLines[horizontalLinesIndex].m_y0 = pixy.line.vectors[i].m_y0;
      horizontalLines[horizontalLinesIndex].m_y1 = pixy.line.vectors[i].m_y1;
      horizontalLines[horizontalLinesIndex].longueur = len;
      horizontalLinesIndex++;
    }

    // 2) garder seulement les lignes assez "verticales"
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

  // prendre d'abord les plus longues = plus fiables
  sortVectorsByLength(leftVectors, leftVectorsIndex);
  sortVectorsByLength(rightVectors, rightVectorsIndex);
}

// ============================================================
//  CALCUL DE LA CIBLE X
// ============================================================
bool computeTargetX(float &targetX)
{
  filterLines();

  int frameCenterX = pixy.frameWidth / 2;

  if (leftVectorsIndex >= 1 && rightVectorsIndex >= 1)
  {
    // 2 lignes visibles -> ligne médiane
    float centerNear = (leftVectors[0].m_x0 + rightVectors[0].m_x0) * 0.5f;
    float centerFar  = (leftVectors[0].m_x1 + rightVectors[0].m_x1) * 0.5f;

    targetX = 0.35f * centerNear + 0.65f * centerFar;

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
    // seulement ligne gauche
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    targetX = leftVectors[0].m_x0 + halfLane;
    return true;
  }
  else if (leftVectorsIndex == 0 && rightVectorsIndex >= 1)
  {
    // seulement ligne droite
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    targetX = rightVectors[0].m_x0 - halfLane;
    return true;
  }

  // aucune ligne exploitable
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

  steering.attach(STEERING_SERVO_PIN);
  setSteeringServo(SERVO_CENTER_ANGLE);
  lastSteerAngle = SERVO_CENTER_ANGLE;

  if (USE_CAMERA_SERVO)
  {
    cameraServo.attach(CAMERA_SERVO_PIN);
    cameraServo.write(CAMERA_FIXED_ANGLE);   // caméra fixe pour l’instant
  }

  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  Serial.println("=== Pixy + Steering Servo Fusion ===");
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
    // perte totale -> garder dernier angle valide
    setSteeringServo(lastSteerAngle);
    Serial.print("No vector -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  float targetX = 0.0f;
  int frameCenterX = pixy.frameWidth / 2;

  bool ok = computeTargetX(targetX);

  if (!ok)
  {
    setSteeringServo(lastSteerAngle);
    Serial.print("No valid line -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  // filtre exponentiel adaptatif
  float rawError = targetX - frameCenterX;
  float alpha = (abs((int)rawError) > FAST_ERROR_THRESHOLD)
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

  // PD
  float error = filteredTargetX - frameCenterX;

  if (abs((int)error) <= DEADBAND_PX)
    error = 0.0f;

  float dError = error - lastError;
  lastError = error;

  float command = SERVO_CENTER_ANGLE + STEER_SIGN * (KP * error + KD * dError);
  int steerAngle = constrain((int)command, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  setSteeringServo(steerAngle);
  lastSteerAngle = steerAngle;

  // debug
  Serial.print("Vec=");
  Serial.print(pixy.line.numVectors);

  Serial.print("  Lcnt=");
  Serial.print(leftVectorsIndex);

  Serial.print("  Rcnt=");
  Serial.print(rightVectorsIndex);

  Serial.print("  LaneW=");
  Serial.print(lastLaneWidthPx);

  Serial.print("  Target=");
  Serial.print(filteredTargetX);

  Serial.print("  Err=");
  Serial.print(error);

  Serial.print("  Servo=");
  Serial.println(steerAngle);
}