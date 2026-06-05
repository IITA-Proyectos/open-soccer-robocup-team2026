#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURACIÓN DE PINES - ROBOT 2 (delantero) — Zircon Rev v15
// Mismo pinout que el sketch canónico diag_central_motors.cpp:
//   Motor 1 -> driver U5      Motor 2 -> driver U17      Motor 3 -> driver U7
//   (Motor 2 usa pines 7/8 — libres: el link a DOWN se movio a Serial1 (pin 0/1))
// =============================================================================
#define INA1  2     // Motor 1 (U5)
#define INB1  5
#define PWM1  3

#define INA2  8     // Motor 2 (U17) — pines 7/8 libres (link a DOWN = Serial1 pin 0/1)
#define INB2  7
#define PWM2  6

#define INA3 11     // Motor 3 (U7)
#define INB3 12
#define PWM3  4

unsigned long tiempoInicioTest = 0;
int estado = 1;

// =============================================================================
// FUNCIÓN: moverAdelante()
// =============================================================================
void moverAdelante() 
{
  analogWrite(PWM1, 50);
  digitalWrite(INA1, 1);
  digitalWrite(INB1, 0);
  
  analogWrite(PWM2, 50);
  digitalWrite(INA2, 0);
  digitalWrite(INB2, 1);
  
  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
}

// =============================================================================
// FUNCIÓN: moverAtras()
// =============================================================================
void moverAtras() {
  analogWrite(PWM1, 50);
  digitalWrite(INA1, 0);
  digitalWrite(INB1, 1);
  
  analogWrite(PWM2, 50);
  digitalWrite(INA2, 1);
  digitalWrite(INB2, 0);
  
  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
}

// =============================================================================
// FUNCIÓN: parar()
// =============================================================================
void parar() {
  analogWrite(PWM1, 0);
  digitalWrite(INA1, 0);
  digitalWrite(INB1, 0);
  
  analogWrite(PWM2, 0);
  digitalWrite(INA2, 0);
  digitalWrite(INB2, 0);
  
  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
}

void setup()
{
  Serial.begin(19200);
  delay(500);
  
  pinMode(INA1, OUTPUT);
  pinMode(INB1, OUTPUT);
  pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT);
  pinMode(INB2, OUTPUT);
  pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT);
  pinMode(INB3, OUTPUT);
  pinMode(PWM3, OUTPUT);
  
  parar();
  tiempoInicioTest = millis();
}
  
// =============================================================================
// LOOP
// =============================================================================
void loop() 
{
  switch (estado)
  {
    case 1:
      moverAtras();
      if (millis() - tiempoInicioTest >= 2000) 
      {
        // CORRECCIÓN 2: reiniciar el temporizador al cambiar de estado
        // para que el case 2 también dure 2 segundos desde su inicio
        tiempoInicioTest = millis();
        estado = 2;
      }
      break;
    
    case 2:
      Serial.print("se mueve hacia adelante");
      moverAdelante();
      if (millis() - tiempoInicioTest >= 2000) 
      {
        parar();
      }
      break;
  }
}
