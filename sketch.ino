#include <ESP32Servo.h>
#include <Wire.h>

const int mpuAddress = 0x68;

Servo flHip,  flKnee;
Servo frHip,  frKnee;
Servo blHip,  blKnee;
Servo brHip,  brKnee;

const int hipMid   = 90;
const int hipFront = 120;
const int hipBack  = 60;
const int kneeDown = 90;
const int kneeUp   = 60;
const int kneeCrouch = 40;
const float rollGain  = 1.0;
const float pitchGain = 1.0;
const float maxAdjust = 30;

unsigned long lastPhaseChange = 0;
const unsigned long stepTime = 300;
int phase = 0;
char mode = 't';
float ax, ay, az;
float gx, gy, gz;
float accPitch = 0;
float accRoll  = 0;
float pitch = 0;
float roll  = 0;
const float alpha = 0.98;
unsigned long lastPrint = 0;
unsigned long lastIMU = 0;

void swingLeg(Servo &hip, Servo &knee) {
  hip.write(hipFront);
  knee.write(kneeUp);
}

void pushLeg(Servo &hip, Servo &knee) {
  hip.write(hipBack);
  knee.write(kneeDown);
}

void trotPhase(int p) {
  if (p == 0) {
    swingLeg(flHip, flKnee);  swingLeg(brHip, brKnee);
    pushLeg(frHip, frKnee);   pushLeg(blHip, blKnee);
  } else {
    pushLeg(flHip, flKnee);   pushLeg(brHip, brKnee);
    swingLeg(frHip, frKnee);  swingLeg(blHip, blKnee);
  }
}

void standPose() {
  flHip.write(hipMid);  frHip.write(hipMid);  blHip.write(hipMid);  brHip.write(hipMid);
  flKnee.write(kneeDown); frKnee.write(kneeDown); blKnee.write(kneeDown); brKnee.write(kneeDown);
}
void crouchPose() {
  flHip.write(hipMid);  frHip.write(hipMid);  blHip.write(hipMid);  brHip.write(hipMid);
  flKnee.write(kneeCrouch); frKnee.write(kneeCrouch); blKnee.write(kneeCrouch); brKnee.write(kneeCrouch);
}

void applyMode() {
  if (mode == 't') {
    standPose();
  } else if (mode == 'c') {
    crouchPose();
  }
  Serial.print("Mode: ");
  Serial.println(mode);
}
int16_t read16() {
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}
void readIMU() {
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(mpuAddress, 14, true);
    int16_t rawAx = read16();
  int16_t rawAy = read16();
  int16_t rawAz = read16();
  read16();
  int16_t rawGx = read16();
  int16_t rawGy = read16();
  int16_t rawGz = read16();

    ax = rawAx / 16384.0;
  ay = rawAy / 16384.0;
  az = rawAz / 16384.0;
  gx = rawGx / 131.0;
  gy = rawGy / 131.0;
  gz = rawGz / 131.0;
}
void computeAccelAngles() {
  accRoll  = atan2(ay, az) * 180.0 / PI;
  accPitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}
void updateFilter(float dt) {
  roll  = alpha * (roll  + gx * dt) + (1 - alpha) * accRoll;
  pitch = alpha * (pitch + gy * dt) + (1 - alpha) * accPitch;
}
void levelLegs() {
  float r = constrain(roll  * rollGain,  -maxAdjust, maxAdjust);
  float p = constrain(pitch * pitchGain, -maxAdjust, maxAdjust);
  flKnee.write(kneeDown + r + p);
  frKnee.write(kneeDown - r + p);
  blKnee.write(kneeDown + r - p);
  brKnee.write(kneeDown - r - p);
}
void setup() {
  Serial.begin(115200);
    Wire.begin(21, 22);
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  readIMU();
  computeAccelAngles();
  roll  = accRoll;
  pitch = accPitch;
  flHip.attach(13);  flKnee.attach(14);
  blHip.attach(18);  blKnee.attach(19);
  frHip.attach(23);  frKnee.attach(25);
  brHip.attach(16);  brKnee.attach(17);
  standPose();
  Serial.println("Commands: w walk, s stop, t stand tall, c crouch");
}


void loop() {
    if (Serial.available() > 0) {
    char cmd = Serial.read();
        if (cmd == 'w' || cmd == 's' || cmd == 't' || cmd == 'c') {
      mode = cmd;
      applyMode();
    }
  }
  unsigned long now = millis();
      if (now - lastIMU >= 20) {
    float dt = (now - lastIMU) / 1000.0;
    lastIMU = now;
    readIMU();
    computeAccelAngles();
    updateFilter(dt);
      if (mode == 't') {
      levelLegs();
    }
  }
    if (now - lastPrint >= 300) {
    lastPrint = now;
    Serial.print("Roll acc: "); Serial.print(accRoll);
    Serial.print("  filtered: "); Serial.print(roll);
    Serial.print("   Pitch acc: "); Serial.print(accPitch);
    Serial.print("  filtered: "); Serial.println(pitch);
  }
   if (mode == 'w' && now - lastPhaseChange >= stepTime) {
    lastPhaseChange = now;
    phase = 1 - phase;
    trotPhase(phase);
  }
}
