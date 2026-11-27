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
const int   SERVO_CENTER_ANGLE = 97;   // ⚠️ Vérifier physiquement
const int   SERVO_MIN_ANGLE    = 40;
const int   SERVO_MAX_ANGLE    = 150;
const int   STEER_SIGN         = 1;   // Corrigé : -1 pour sens correct

// ============================================================
//  VITESSE MOTEURS
// ============================================================
const int MOTOR_SPEED_BASE = 180;  // Vitesse nominale (0-255)
const int MOTOR_SPEED_MIN  = 160;  // Vitesse minimale en courbe serrée (0-255)

// Angle de déviation servo (en degrés par rapport au centre)
// au-delà duquel la réduction de vitesse est maximale
const int STEER_DEADZONE_DEG  = 5;    // en-dessous : pas de réduction
const int STEER_MAXCURVE_DEG  = 40;   // au-delà : vitesse minimale

// ============================================================
//  PD + FILTRE
// ============================================================
const float KP = 0.95f;
const float KD = 0.15f;

const int   DEADBAND_PX          = 2;
const float FILTER_ALPHA_SLOW    = 0.35f;
const float FILTER_ALPHA_FAST    = 0.65f;
const int   FAST_ERROR_THRESHOLD = 20;

// ============================================================
//  BOUCLE
// ============================================================
const unsigned long LOOP_INTERVAL_MS = 20;   // 50 Hz
unsigned long lastLoopTime = 0;

// ============================================================
//  PIXY FILTERING
// ============================================================
const int   LINE_VECTOR_SIZE     = 20;
const int   HORIZONTAL_LINE_SIZE = 5;
const int   ANGLE_THRESHOLD_DEG  = 15;
const float HORIZONTAL_MAX_DEG   = 20.0f;
const float CURVE_LANE_FACTOR    = 0.65f;  // Tuner vers 0.70 si sous-braquage

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

// Avancer les deux moteurs à la vitesse donnée
void setMotors(int speed)
{
  speed = constrain(speed, 0, 255);

  // Moteur gauche (OUT1/OUT2 = IN1/IN2)
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, speed);

  // Moteur droit (OUT3/OUT4 = IN3/IN4)
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

// Calcule la vitesse cible en fonction de la déviation du servo
// Plus le servo est loin du centre → plus on ralentit
int computeSpeed(int steerAngle)
{
  int deviation = abs(steerAngle - SERVO_CENTER_ANGLE);

  if (deviation <= STEER_DEADZONE_DEG)
  {
    // Quasi-droit : vitesse max
    return MOTOR_SPEED_BASE;
  }
  else if (deviation >= STEER_MAXCURVE_DEG)
  {
    // Courbe serrée : vitesse min
    return MOTOR_SPEED_MIN;
  }
  else
  {
    // Interpolation linéaire entre les deux
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
    for (int j = 0; j < size - i - 1; j++)
      if (vectors[j].longueur < vectors[j + 1].longueur)
      {
        vectorPixy tmp  = vectors[j];
        vectors[j]      = vectors[j + 1];
        vectors[j + 1]  = tmp;
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
    bottomX = pixy.line.vectors[i].m_x0;  bottomY = pixy.line.vectors[i].m_y0;
    topX    = pixy.line.vectors[i].m_x1;  topY    = pixy.line.vectors[i].m_y1;
  }
  else
  {
    bottomX = pixy.line.vectors[i].m_x1;  bottomY = pixy.line.vectors[i].m_y1;
    topX    = pixy.line.vectors[i].m_x0;  topY    = pixy.line.vectors[i].m_y0;
  }
}

// ============================================================
//  FILTRAGE DES LIGNES PIXY
// ============================================================
void filterLines()
{
  leftVectorsIndex = 0; rightVectorsIndex = 0; horizontalLinesIndex = 0;

  for (uint8_t i = 0; i < pixy.line.numVectors; i++)
  {
    float dx  = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    float dy  = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
    float len = sqrtf(dx * dx + dy * dy);

    float angleDeg        = fmodf(atan2f(dy, dx) * (180.0f / PI) + 180.0f, 180.0f);
    float horizontalAngle = atan2f(fabsf(dy), fabsf(dx)) * 180.0f / PI;

    if (horizontalAngle < HORIZONTAL_MAX_DEG && horizontalLinesIndex < HORIZONTAL_LINE_SIZE)
    {
      horizontalLines[horizontalLinesIndex] = { len,
        pixy.line.vectors[i].m_x1, pixy.line.vectors[i].m_x0,
        pixy.line.vectors[i].m_y1, pixy.line.vectors[i].m_y0 };
      horizontalLinesIndex++;
    }

    int yStart = max(pixy.line.vectors[i].m_y0, pixy.line.vectors[i].m_y1);

    if (angleDeg > ANGLE_THRESHOLD_DEG &&
        angleDeg < (180 - ANGLE_THRESHOLD_DEG) &&
        yStart > pixy.frameHeight * 0.1f)
    {
      int bX, bY, tX, tY;
      getBottomTopFromPixyIndex(i, bX, bY, tX, tY);
      float vlen = sqrtf((float)(tY-bY)*(tY-bY) + (float)(tX-bX)*(tX-bX));

      if (bX < pixy.frameWidth / 2)
      {
        if (leftVectorsIndex < LINE_VECTOR_SIZE)
          leftVectors[leftVectorsIndex++]  = { vlen, tX, bX, tY, bY };
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
//  CALCUL DE LA CIBLE X
// ============================================================
bool computeTargetX(float &targetX, int &xBasCentre, int &xHautCentre)
{
  filterLines();

  int frameCenterX = pixy.frameWidth / 2;
  xBasCentre = xHautCentre = frameCenterX;

  // CAS 1 : deux lignes visibles
  if (leftVectorsIndex >= 1 && rightVectorsIndex >= 1)
  {
    xBasCentre  = (leftVectors[0].m_x0 + rightVectors[0].m_x0) / 2;
    xHautCentre = (leftVectors[0].m_x1 + rightVectors[0].m_x1) / 2;
    targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;

    int measuredWidth = rightVectors[0].m_x0 - leftVectors[0].m_x0;
    if (measuredWidth > 10 && measuredWidth < pixy.frameWidth)
    {
      if (!laneWidthCalibrated) { lastLaneWidthPx = measuredWidth; laneWidthCalibrated = true; }
      else lastLaneWidthPx = (int)(0.9f * lastLaneWidthPx + 0.1f * measuredWidth);
    }
    return true;
  }

  // CAS 2 : ligne gauche seulement (virage droit serré)
  else if (leftVectorsIndex >= 1 && rightVectorsIndex == 0)
  {
    int offset  = (lastLaneWidthPx > 0) ? (int)(lastLaneWidthPx * CURVE_LANE_FACTOR) : 40;
    xBasCentre  = leftVectors[0].m_x0 + offset;
    xHautCentre = leftVectors[0].m_x1 + offset;
    targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;
    return true;
  }

  // CAS 3 : ligne droite seulement (virage gauche serré)
  else if (leftVectorsIndex == 0 && rightVectorsIndex >= 1)
  {
    int offset  = (lastLaneWidthPx > 0) ? (int)(lastLaneWidthPx * CURVE_LANE_FACTOR) : 40;
    xBasCentre  = rightVectors[0].m_x0 - offset;
    xHautCentre = rightVectors[0].m_x1 - offset;
    targetX     = 0.35f * xBasCentre + 0.65f * xHautCentre;
    return true;
  }

  // CAS 4 : aucune ligne
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

  // Moteurs
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  stopMotors();

  // Servo
  steering.attach(STEERING_SERVO_PIN);
  setSteeringServo(SERVO_CENTER_ANGLE);
  lastSteerAngle = SERVO_CENTER_ANGLE;

  // Pixy
  pixy.init();
  pixy.setLamp(1, 1);   // ⚠️ Mettre (0,0) si éclairage fort en compétition
  pixy.changeProg("line");

  Serial.println("=== Pixy + Servo + Motors ===");
  delay(500);   // Laisser le servo se centrer avant de démarrer
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  // Lecture Pixy unique par cycle
  int8_t res = pixy.line.getAllFeatures(LINE_VECTOR);

  if (res <= 0 || pixy.line.numVectors == 0)
  {
    // Pas de ligne : garder dernier angle et ralentir par sécurité
    setSteeringServo(lastSteerAngle);
    setMotors(MOTOR_SPEED_MIN);
    Serial.print("No vector -> keep angle: ");
    Serial.print(lastSteerAngle);
    Serial.print("  speed: ");
    Serial.println(MOTOR_SPEED_MIN);
    return;
  }

  float targetX     = 0.0f;
  int frameCenterX  = pixy.frameWidth / 2;
  int xBasCentre    = frameCenterX;
  int xHautCentre   = frameCenterX;

  bool ok = computeTargetX(targetX, xBasCentre, xHautCentre);

  if (!ok)
  {
    setSteeringServo(lastSteerAngle);
    setMotors(MOTOR_SPEED_MIN);
    Serial.print("No valid line -> keep angle: ");
    Serial.println(lastSteerAngle);
    return;
  }

  // ---- Filtre adaptatif sur targetX ----
  float rawError = targetX - frameCenterX;
  float alpha    = (abs((int)rawError) > FAST_ERROR_THRESHOLD)
                   ? FILTER_ALPHA_FAST : FILTER_ALPHA_SLOW;

  if (!filterInitialized) { filteredTargetX = targetX; filterInitialized = true; }
  else filteredTargetX = alpha * targetX + (1.0f - alpha) * filteredTargetX;

  // ---- Erreur + deadband ----
  float error = filteredTargetX - frameCenterX;
  if (abs((int)error) <= DEADBAND_PX) error = 0.0f;

  // ---- Commande PD ----
  float dError   = error - lastError;
  lastError      = error;

  float command  = SERVO_CENTER_ANGLE + STEER_SIGN * (KP * error + KD * dError);
  int steerAngle = constrain((int)command, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  setSteeringServo(steerAngle);
  lastSteerAngle = steerAngle;

  // ---- Vitesse adaptée à la courbe ----
  int speed = computeSpeed(steerAngle);
  setMotors(speed);

  // ---- Debug Serial Plotter ----
  Serial.print("Lcnt=");    Serial.print(leftVectorsIndex);
  Serial.print("  Rcnt=");  Serial.print(rightVectorsIndex);
  Serial.print("  LaneW="); Serial.print(lastLaneWidthPx);
  Serial.print("  Err=");   Serial.print(error);
  Serial.print("  Servo="); Serial.print(steerAngle);
  Serial.print("  Speed="); Serial.println(speed);
}