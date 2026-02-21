// =============================================================================
// LEGACY - Temporada 2025 - Librería Zircon (implementación)
// Fuente: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
// Archivo original: /Librerias/zirconLib/zirconLib.cpp
// Nota: Soporta dos versiones de PCB: "Mark1" y "Naveen1"
// =============================================================================

#include "zirconLib.h"
#include <Arduino.h>

Adafruit_BNO055 bno;

String ZirconVersion;
boolean compassCalibrated = false;

#define motorLimit 100

int motor1dir1, motor1dir2, motor1pwm;
int motor2dir1, motor2dir2, motor2pwm;
int motor3dir1, motor3dir2, motor3pwm;

int ballpin1, ballpin2, ballpin3, ballpin4;
int ballpin5, ballpin6, ballpin7, ballpin8;
int ballpins[8];

int buttonpin1, buttonpin2;
int linepin1, linepin2, linepin3;

void InitializeZircon() {
  setZirconVersion();
  initializePins();
}

void setZirconVersion() {
  pinMode(32, INPUT_PULLDOWN);
  if (digitalRead(32) == LOW) {
    ZirconVersion = "Mark1";
  } else {
    ZirconVersion = "Naveen1";
  }
}

double readCompass() {
  if (compassCalibrated) {
    sensors_event_t orientationData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    return orientationData.orientation.x;
  } else {
    Serial.println("Compass not calibrated!");
    return 0;
  }
}

int readBall(int ballSensorNumber) {
  int ballpin;
  switch (ballSensorNumber) {
    case 1: ballpin = ballpin1; break;
    case 2: ballpin = ballpin2; break;
    case 3: ballpin = ballpin3; break;
    case 4: ballpin = ballpin4; break;
    case 5: ballpin = ballpin5; break;
    case 6: ballpin = ballpin6; break;
    case 7: ballpin = ballpin7; break;
    case 8: ballpin = ballpin8; break;
    default: return -1;
  }
  return 1024 - analogRead(ballpin);
}

int readButton(int buttonNumber) {
  if (buttonNumber == 1) return digitalRead(buttonpin1);
  else if (buttonNumber == 2) return digitalRead(buttonpin2);
  else return 0;
}

int readLine(int lineNumber) {
  if (lineNumber == 1) return analogRead(linepin1);
  else if (lineNumber == 2) return analogRead(linepin2);
  else if (lineNumber == 3) return analogRead(linepin3);
  else return 0;
}

void motor1(int power, bool direction) {
  power = min(power, motorLimit);
  if (ZirconVersion == "Naveen1") {
    if (direction == 0) { analogWrite(motor1dir1, 0); analogWrite(motor1dir2, power); }
    else { analogWrite(motor1dir1, power); analogWrite(motor1dir2, 0); }
  } else if (ZirconVersion == "Mark1") {
    digitalWrite(motor1dir1, direction);
    digitalWrite(motor1dir2, !direction);
    analogWrite(motor1pwm, power);
  }
}

void motor2(int power, bool direction) {
  power = min(power, motorLimit);
  if (ZirconVersion == "Naveen1") {
    if (direction == 0) { analogWrite(motor2dir1, 0); analogWrite(motor2dir2, power); }
    else { analogWrite(motor2dir1, power); analogWrite(motor2dir2, 0); }
  } else if (ZirconVersion == "Mark1") {
    digitalWrite(motor2dir1, direction);
    digitalWrite(motor2dir2, !direction);
    analogWrite(motor2pwm, power);
  }
}

void motor3(int power, bool direction) {
  power = min(power, motorLimit);
  if (ZirconVersion == "Naveen1") {
    if (direction == 0) { analogWrite(motor3dir1, 0); analogWrite(motor3dir2, power); }
    else { analogWrite(motor3dir1, power); analogWrite(motor3dir2, 0); }
  } else if (ZirconVersion == "Mark1") {
    digitalWrite(motor3dir1, direction);
    digitalWrite(motor3dir2, !direction);
    analogWrite(motor3pwm, power);
  }
}

void initializePins() {
  if (ZirconVersion == "Mark1") {
    motor1dir1=2; motor1dir2=5; motor1pwm=3;
    motor2dir1=8; motor2dir2=7; motor2pwm=6;
    motor3dir1=11; motor3dir2=12; motor3pwm=4;
    ballpin1=14; ballpin2=15; ballpin3=16; ballpin4=17;
    ballpin5=20; ballpin6=21; ballpin7=22; ballpin8=23;
    buttonpin1=9; buttonpin2=10;
    linepin1=A11; linepin2=A13; linepin3=A12;
  } else if (ZirconVersion == "Naveen1") {
    motor1dir1=3; motor1dir2=4;
    motor2dir1=6; motor2dir2=7;
    motor3dir1=5; motor3dir2=2;
    ballpin1=20; ballpin2=21; ballpin3=14; ballpin4=15;
    ballpin5=16; ballpin6=17; ballpin7=18; ballpin8=19;
    buttonpin1=8; buttonpin2=10;
    linepin1=A8; linepin2=A9; linepin3=A12;
  }

  pinMode(motor1dir1, OUTPUT); pinMode(motor1dir2, OUTPUT); pinMode(motor1pwm, OUTPUT);
  pinMode(motor2dir1, OUTPUT); pinMode(motor2dir2, OUTPUT); pinMode(motor2pwm, OUTPUT);
  pinMode(motor3dir1, OUTPUT); pinMode(motor3dir2, OUTPUT); pinMode(motor3pwm, OUTPUT);
  pinMode(ballpin1, INPUT); pinMode(ballpin2, INPUT); pinMode(ballpin3, INPUT); pinMode(ballpin4, INPUT);
  pinMode(ballpin5, INPUT); pinMode(ballpin6, INPUT); pinMode(ballpin7, INPUT); pinMode(ballpin8, INPUT);
  pinMode(buttonpin1, INPUT); pinMode(buttonpin2, INPUT);
}

String getZirconVersion() { return ZirconVersion; }
bool isCompassCalibrated() { return compassCalibrated; }
