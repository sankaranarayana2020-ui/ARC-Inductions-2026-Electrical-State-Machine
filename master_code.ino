#include <Wire.h>
#include <Servo.h>

const int ldrPin = A0;
const int gasPin = A1;
const int tempPin = A2;
const int servoPin = 9;
const int buzzerPin = 8;
const int SLAVE_ADDR = 8;

const int GAS_ALERT_THRESHOLD = 180;
const int GAS_CLEAR_THRESHOLD = 130;
const float TEMP_EMERGENCY = 45.0;
const int LIGHT_DROP_THRESHOLD = 200; // TUNE THIS after watching real LDR values

enum SystemState { STANDBY=0, ACTIVE=1, GAS_ALERT=2, BLACKOUT=3, TEMP_EMERGENCY_STATE=4, MULTI_FAULT=5 };
SystemState currentState = STANDBY;

Servo myServo;
bool displayMode = false; // false=light, true=gas
int lightBaseline = 500;
bool blackoutLatched = false;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  myServo.attach(servoPin);
  myServo.write(0);
  lightBaseline = analogRead(ldrPin);
}

void loop() {
  int lightVal = analogRead(ldrPin);
  int gasVal = analogRead(gasPin);
  float tempC = readTemperature();
  byte command = requestCommandFromSlave();

  // Blackout detection (baseline tracking)
  if (!blackoutLatched) {
    lightBaseline = (lightBaseline * 9 + lightVal) / 10;
    if ((lightBaseline - lightVal) > LIGHT_DROP_THRESHOLD) blackoutLatched = true;
  } else {
    if (lightVal > (lightBaseline - LIGHT_DROP_THRESHOLD / 2)) blackoutLatched = false;
  }

  bool gasHigh = (gasVal > GAS_ALERT_THRESHOLD);
  bool gasStillHigh = (currentState == GAS_ALERT && gasVal >= GAS_CLEAR_THRESHOLD);

  // --- STATE MACHINE ---
  if (tempC > TEMP_EMERGENCY) {
    currentState = TEMP_EMERGENCY_STATE;
  }

  if (currentState == TEMP_EMERGENCY_STATE) {
    myServo.write(180);
    noTone(buzzerPin);
    if (command == 3) { // reset
      currentState = ACTIVE;
      myServo.write(0);
    }
  } else {
    myServo.write(0);

    if (currentState == STANDBY) {
      if (command == 1) currentState = ACTIVE; // activate
    } else {
      bool inGasAlert = gasHigh || gasStillHigh;
      bool inBlackout = blackoutLatched;

      if (inGasAlert && inBlackout) currentState = MULTI_FAULT;
      else if (inGasAlert) currentState = GAS_ALERT;
      else if (inBlackout) currentState = BLACKOUT;
      else currentState = ACTIVE;

      if (currentState == ACTIVE && command == 2) displayMode = !displayMode;
    }
  }

  tone_or_stop();
  int valueToSend = displayMode ? map(gasVal, 0, 1023, 100, 0) : lightVal; // gas as rough "purity %"
  sendDataToSlave(currentState, displayMode, valueToSend);

  delay(200);
}

void tone_or_stop() {
  if (currentState == MULTI_FAULT) tone(buzzerPin, 1000);
  else noTone(buzzerPin);
}

float readTemperature() {
  int rawValue = analogRead(tempPin);
  float voltage = rawValue * (5.0 / 1023.0);
  return (voltage - 0.5) * 100.0;
}

byte requestCommandFromSlave() {
  Wire.requestFrom(SLAVE_ADDR, 1);
  byte cmd = 0;
  if (Wire.available()) cmd = Wire.read();
  return cmd;
}

void sendDataToSlave(byte state, bool mode, int value) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(state);
  Wire.write((byte)mode);
  Wire.write(highByte(value));
  Wire.write(lowByte(value));
  Wire.endTransmission();
}
