// =============================================================================
// LEGACY - Temporada 2025 - Librería Zircon (header)
// Fuente: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
// Archivo original: /Librerias/zirconLib/zirconLib.h
// =============================================================================

#pragma once
#include <Arduino.h>
#include <Adafruit_BNO055.h>

void setZirconVersion();

void InitializeZircon();

//void CalibrateCompass();

double readCompass();

int readBall(int ballSensorNumber);

int readLine(int lineNumber);

int readButton(int buttonNumber);

void motor1(int power, bool direction);

void motor2(int power, bool direction);

void motor3(int power, bool direction);

void initializePins();

String getZirconVersion();
