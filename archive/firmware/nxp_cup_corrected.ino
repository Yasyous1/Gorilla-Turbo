#include <Pixy2.h>
#include <PWMServo.h>

Pixy2 pixy;
PWMServo steering;

// ─────────────────────────────────────────────
//  BROCHES
// ─────────────────────────────────────────────
const int SERVO_PIN    = 19;
const int MOTOR_PIN_A  = 3;   // PWM moteur gauche  (adapter selon ton shield)
const int MOTOR_PIN_B  = 4;   // PWM moteur droite  (adapter selon ton shield)
const int MOTOR_DIR_A  = 5;   // Direction gauche
const int MOTOR_DIR_B  = 6;   // Direction droite

// ─────────────────────────────────────────────
//  SERVO
// ─────────────────────────────────────────────
const int SERVO_CENTER_ANGLE = 95;
const int SERVO_MIN_ANGLE    = 40;
const int SERVO_MAX_ANGLE    = 150;
const int STEER_SIGN         = 1;

// ─────────────────────────────────────────────
//  RÉGULATEUR PD
// ─────────────────────────────────────────────
const float KP           = 0.45f;
const float KD           = 0.25f;
const int   DEADBAND_PX  = 2;

// ─────────────────────────────────────────────
//  FILTRE EXPONENTIEL
//  Alpha augmente dynamiquement si erreur > FAST_ERROR_THRESHOLD
// ─────────────────────────────────────────────
const float FILTER_ALPHA_SLOW  = 0.35f;   // virage doux / ligne droite
const float FILTER_ALPHA_FAST  = 0.65f;   // virage serré
const int   FAST_ERROR_THRESHOLD = 20;    // px

// ─────────────────────────────────────────────
//  VITESSE MOTEUR
// ─────────────────────────────────────────────
const int MOTOR_MAX_SPEED  = 200;   // PWM 0-255
const int MOTOR_MIN_SPEED  = 100;   // ralentissement en virage
const int MOTOR_STOP_ERROR = 60;    // au-delà de cette erreur → vitesse min

// ─────────────────────────────────────────────
//  BOUCLE NON BLOQUANTE
// ─────────────────────────────────────────────
const unsigned long LOOP_INTERVAL_MS = 20;  // 50 Hz
unsigned long lastLoopTime = 0;

// ─────────────────────────────────────────────
//  ÉTAT INTERNE
// ─────────────────────────────────────────────
float lastError          = 0.0f;
float filteredTargetX    = 0.0f;
bool  filterInitialized  = false;

// Largeur de voie : initialisée à 0, mesurée sur les premières frames
int   lastLaneWidthPx    = 0;
bool  laneWidthCalibrated = false;

int lastSteerAngle = SERVO_CENTER_ANGLE;

// ─────────────────────────────────────────────
//  STRUCTURE LIGNE LATÉRALE
// ─────────────────────────────────────────────
struct SideLine {
  bool valid;
  int  bottomX;
  int  topX;
};

// ─────────────────────────────────────────────
//  PROTOTYPES
// ─────────────────────────────────────────────
void setMotorSpeed(int speed);
void setMotorStop();
static void getBottomTop(const Vector &v, int &bX, int &bY, int &tX, int &tY);

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Servo
  steering.attach(SERVO_PIN);
  steering.write(SERVO_CENTER_ANGLE);
  lastSteerAngle = SERVO_CENTER_ANGLE;

  // Moteur
  pinMode(MOTOR_PIN_A,  OUTPUT);
  pinMode(MOTOR_PIN_B,  OUTPUT);
  pinMode(MOTOR_DIR_A,  OUTPUT);
  pinMode(MOTOR_DIR_B,  OUTPUT);
  setMotorStop();

  // Pixy
  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  Serial.println("=== NXP Cup – démarrage ===");
}

// ─────────────────────────────────────────────
//  BOUCLE PRINCIPALE (non bloquante)
// ─────────────────────────────────────────────
void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  // ── Lecture Pixy ─────────────────────────
  int8_t res = pixy.line.getAllFeatures(LINE_VECTOR);
  int frameCenterX = pixy.frameWidth / 2;

  if (res <= 0 || pixy.line.numVectors == 0) {
    steering.write(lastSteerAngle);   // garde la dernière direction valide
    // Ralentir progressivement si on ne voit plus la ligne
    setMotorSpeed(MOTOR_MIN_SPEED);
    Serial.print("Pas de vecteur -> angle gardé: ");
    Serial.println(lastSteerAngle);
    return;
  }

  // ── Tri gauche / droite ───────────────────
  SideLine left  = {false, 0, 0};
  SideLine right = {false, 0, 0};
  int bestLeftDist  = 10000;
  int bestRightDist = 10000;

  for (uint8_t i = 0; i < pixy.line.numVectors; i++) {
    const Vector &v = pixy.line.vectors[i];
    int bottomX, bottomY, topX, topY;
    getBottomTop(v, bottomX, bottomY, topX, topY);

    if (bottomX <= frameCenterX) {
      int dist = frameCenterX - bottomX;
      if (dist < bestLeftDist) {
        bestLeftDist  = dist;
        left.valid    = true;
        left.bottomX  = bottomX;
        left.topX     = topX;
      }
    } else {
      int dist = bottomX - frameCenterX;
      if (dist < bestRightDist) {
        bestRightDist  = dist;
        right.valid    = true;
        right.bottomX  = bottomX;
        right.topX     = topX;
      }
    }
  }

  // ── Calcul cible X ───────────────────────
  float targetX;

  if (left.valid && right.valid) {
    float centerNear = (left.bottomX + right.bottomX) * 0.5f;
    float centerFar  = (left.topX    + right.topX)    * 0.5f;
    targetX = 0.35f * centerNear + 0.65f * centerFar;

    // Calibrage largeur de voie sur les premières frames valides
    int measuredWidth = right.bottomX - left.bottomX;
    if (!laneWidthCalibrated) {
      lastLaneWidthPx   = measuredWidth;
      laneWidthCalibrated = true;
    } else {
      // Mise à jour douce pour éviter les sauts
      lastLaneWidthPx = (int)(0.9f * lastLaneWidthPx + 0.1f * measuredWidth);
    }

  } else if (left.valid) {
    // Une seule ligne gauche visible
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    targetX = left.bottomX + halfLane;

  } else if (right.valid) {
    // Une seule ligne droite visible
    int halfLane = (lastLaneWidthPx > 0) ? lastLaneWidthPx / 2 : 30;
    targetX = right.bottomX - halfLane;

  } else {
    // Vecteurs présents mais aucun côté classifié → garde le cap
    steering.write(lastSteerAngle);
    setMotorSpeed(MOTOR_MIN_SPEED);
    Serial.print("Aucun côté valide -> angle gardé: ");
    Serial.println(lastSteerAngle);
    return;
  }

  // ── Filtre exponentiel adaptatif ─────────
  float rawError = targetX - frameCenterX;
  float alpha = (abs((int)rawError) > FAST_ERROR_THRESHOLD)
                ? FILTER_ALPHA_FAST
                : FILTER_ALPHA_SLOW;

  if (!filterInitialized) {
    filteredTargetX  = targetX;
    filterInitialized = true;
  } else {
    filteredTargetX = alpha * targetX + (1.0f - alpha) * filteredTargetX;
  }

  // ── Régulateur PD ────────────────────────
  float error = filteredTargetX - frameCenterX;

  if (abs((int)error) <= DEADBAND_PX) {
    error = 0.0f;
  }

  float dError = error - lastError;
  lastError    = error;

  int steerAngle = SERVO_CENTER_ANGLE + STEER_SIGN * (int)(KP * error + KD * dError);
  steerAngle = constrain(steerAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  steering.write(steerAngle);
  lastSteerAngle = steerAngle;

  // ── Vitesse adaptative ────────────────────
  // Réduit la vitesse proportionnellement à l'amplitude de l'erreur
  int absError   = abs((int)error);
  int motorSpeed = map(absError, 0, MOTOR_STOP_ERROR, MOTOR_MAX_SPEED, MOTOR_MIN_SPEED);
  motorSpeed     = constrain(motorSpeed, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
  setMotorSpeed(motorSpeed);

  // ── Debug série ───────────────────────────
  Serial.print("Vec=");   Serial.print(pixy.line.numVectors);
  Serial.print("  L=");   if (left.valid)  Serial.print(left.bottomX);  else Serial.print("X");
  Serial.print("  R=");   if (right.valid) Serial.print(right.bottomX); else Serial.print("X");
  Serial.print("  W=");   Serial.print(lastLaneWidthPx);
  Serial.print("  Tgt="); Serial.print(filteredTargetX);
  Serial.print("  Err="); Serial.print(error);
  Serial.print("  Srv="); Serial.print(steerAngle);
  Serial.print("  Mtr="); Serial.println(motorSpeed);
}

// ─────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────

// Retourne le point le plus bas (y max) et le plus haut d'un vecteur
static void getBottomTop(const Vector &v,
                         int &bottomX, int &bottomY,
                         int &topX,    int &topY) {
  if (v.m_y0 >= v.m_y1) {
    bottomX = v.m_x0; bottomY = v.m_y0;
    topX    = v.m_x1; topY    = v.m_y1;
  } else {
    bottomX = v.m_x1; bottomY = v.m_y1;
    topX    = v.m_x0; topY    = v.m_y0;
  }
}

// Commande les deux moteurs à la même vitesse (avant)
// ⚠️  Adapte les digitalWrite DIR selon le câblage de ton pont en H
void setMotorSpeed(int speed) {
  speed = constrain(speed, 0, 255);
  digitalWrite(MOTOR_DIR_A, HIGH);
  digitalWrite(MOTOR_DIR_B, HIGH);
  analogWrite(MOTOR_PIN_A, speed);
  analogWrite(MOTOR_PIN_B, speed);
}

// Arrêt moteur
void setMotorStop() {
  analogWrite(MOTOR_PIN_A, 0);
  analogWrite(MOTOR_PIN_B, 0);
}
