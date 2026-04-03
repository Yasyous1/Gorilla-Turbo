#include <Pixy2.h>
#include <PWMServo.h>

Pixy2 pixy;
PWMServo steering;

const int SERVO_PIN = 19;

const int SERVO_CENTER_ANGLE = 95;
const int SERVO_MIN_ANGLE = 40;
const int SERVO_MAX_ANGLE = 150;

const int STEER_SIGN = 1;
const float KP = 0.45f;
const float KD = 0.25f;
const float FILTER_ALPHA = 0.35f;
const int DEADBAND_PX = 2;

float lastError = 0.0f;
float filteredTargetX = 0.0f;
bool filterInitialized = false;

int lastLaneWidthPx = 30;

struct SideLine {
  bool valid;
  int bottomX;
  int topX;
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  steering.attach(SERVO_PIN);
  steering.write(SERVO_CENTER_ANGLE);

  pixy.init();
  pixy.setLamp(1, 1);
  pixy.changeProg("line");

  Serial.println("Start");
}

static void getBottomTop(const Vector &v, int &bottomX, int &bottomY, int &topX, int &topY) {
  if (v.m_y0 >= v.m_y1) {
    bottomX = v.m_x0;
    bottomY = v.m_y0;
    topX = v.m_x1;
    topY = v.m_y1;
  } else {
    bottomX = v.m_x1;
    bottomY = v.m_y1;
    topX = v.m_x0;
    topY = v.m_y0;
  }
}

void loop() {
  int8_t res = pixy.line.getAllFeatures(LINE_VECTOR);

  int frameCenterX = pixy.frameWidth / 2;

  if (res <= 0 || pixy.line.numVectors == 0) {
    steering.write(SERVO_CENTER_ANGLE);
    Serial.println("No vector");
    delay(20);
    return;
  }

  SideLine left = {false, 0, 0};
  SideLine right = {false, 0, 0};

  int bestLeftDist = 10000;
  int bestRightDist = 10000;

  for (uint8_t i = 0; i < pixy.line.numVectors; i++) {
    const Vector &v = pixy.line.vectors[i];

    int bottomX, bottomY, topX, topY;
    getBottomTop(v, bottomX, bottomY, topX, topY);

    if (bottomX <= frameCenterX) {
      int dist = frameCenterX - bottomX;
      if (dist < bestLeftDist) {
        bestLeftDist = dist;
        left.valid = true;
        left.bottomX = bottomX;
        left.topX = topX;
      }
    } else {
      int dist = bottomX - frameCenterX;
      if (dist < bestRightDist) {
        bestRightDist = dist;
        right.valid = true;
        right.bottomX = bottomX;
        right.topX = topX;
      }
    }
  }

  float targetX;

  if (left.valid && right.valid) {
    float centerNear = (left.bottomX + right.bottomX) * 0.5f;
    float centerFar  = (left.topX + right.topX) * 0.5f;
    targetX = 0.35f * centerNear + 0.65f * centerFar;
    lastLaneWidthPx = right.bottomX - left.bottomX;
  } else if (left.valid) {
    targetX = left.bottomX + lastLaneWidthPx * 0.5f;
  } else if (right.valid) {
    targetX = right.bottomX - lastLaneWidthPx * 0.5f;
  } else {
    targetX = frameCenterX;
  }

  if (!filterInitialized) {
    filteredTargetX = targetX;
    filterInitialized = true;
  } else {
    filteredTargetX = FILTER_ALPHA * targetX + (1.0f - FILTER_ALPHA) * filteredTargetX;
  }

  float error = filteredTargetX - frameCenterX;

  if (abs((int)error) <= DEADBAND_PX) {
    error = 0.0f;
  }

  float dError = error - lastError;
  lastError = error;

  int steerAngle = SERVO_CENTER_ANGLE + STEER_SIGN * (int)(KP * error + KD * dError);
  steerAngle = constrain(steerAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  steering.write(steerAngle);

  Serial.print("Vectors=");
  Serial.print(pixy.line.numVectors);
  Serial.print("  L=");
  if (left.valid) Serial.print(left.bottomX); else Serial.print("X");
  Serial.print("  R=");
  if (right.valid) Serial.print(right.bottomX); else Serial.print("X");
  Serial.print("  Target=");
  Serial.print(filteredTargetX);
  Serial.print("  Err=");
  Serial.print(error);
  Serial.print("  ServoAngle=");
  Serial.println(steerAngle);

  delay(20);
}