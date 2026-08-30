#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

LiquidCrystal_I2C lcd(0x20, 16, 2);
const int irPin = 11;
const byte MY_ADDRESS = 8;

#define IR_POWER 0xFF00BF00
#define IR_PLAY  0xFA05BF00
#define IR_EQ    0xF20DBF00

volatile byte lastCommand = 0; // 0=none,1=activate,2=toggle,3=reset
volatile byte currentState = 0;
volatile byte displayMode = 0;
volatile int currentValue = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin(MY_ADDRESS);
  Wire.onRequest(sendCommand);
  Wire.onReceive(receiveData);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AWAITING RITUAL");

  IrReceiver.begin(irPin);
}

void loop() {
  checkIR();
  updateDisplay();
  delay(50);
}

void checkIR() {
  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    if (code == IR_POWER) lastCommand = 1;
    else if (code == IR_PLAY) lastCommand = 2;
    else if (code == IR_EQ) lastCommand = 3;
    IrReceiver.resume();
  }
}

void sendCommand() {
  Wire.write(lastCommand);
  lastCommand = 0;
}

void receiveData(int byteCount) {
  if (byteCount >= 4) {
    currentState = Wire.read();
    displayMode = Wire.read();
    byte highB = Wire.read();
    byte lowB = Wire.read();
    currentValue = (highB << 8) | lowB;
  }
}

void updateDisplay() {
  static byte lastState = 255, lastMode = 255;
  static int lastValue = -1;
  if (currentState == lastState && displayMode == lastMode && currentValue == lastValue) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  switch (currentState) {
    case 0: lcd.print("AWAITING RITUAL"); break;
    case 1:
      if (displayMode == 0) { lcd.print("LIGHT LEVEL:"); lcd.setCursor(0,1); lcd.print(currentValue); }
      else { lcd.print("AIR PURITY:"); lcd.setCursor(0,1); lcd.print(currentValue); lcd.print("%"); }
      break;
    case 2:
    lcd.print("TOXIC PURGE");
    
    break;
    case 3: lcd.print("NOCTIS PROTOCOL"); break;
    case 4: lcd.print("COOKED"); break;
    case 5: lcd.print("MULTIPLE"); lcd.setCursor(0,1); lcd.print("PROBLEMS DETECTED"); break;
  }
  lastState = currentState; lastMode = displayMode; lastValue = currentValue;
}