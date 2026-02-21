// =============================================================================
// LEGACY - Temporada 2025
// Fuente: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
// Archivo original: /para que persiga la pelota
// Autores originales: Equipo 2025
// Migrado por: Claude (Anthropic) el 2026-02-21
// =============================================================================

#include <Arduino.h>
#include <zirconLib.h>
#include <Wire.h> 
#include <Adafruit_Sensor.h> 
#include <Adafruit_BNO055.h> 

const long BAUD_RATE = 19200;

// Angulos 
  float anguloRadPelota;
  float anguloPelota;
  float anguloRadArco;
  float anguloArco;

// Velocidades 
  float k; 
  float g = 0.4;
   
// Variables decodificadas
  float decodedXp = 0.0;
  float decodedYp = 0.0;
  float decodedXa = 0.0;
  float decodedYa = 0.0;

// Verificacion de objetos 
  bool haypelota = 0;
  bool hayarco = 0;

// Tolerancias
  const float tolerancia_centrado = 10.0;
  const float tolerancia_cercania = 19.0;

// Estados del seguimiento (Máquina de estados)
enum TrackingState {GIRANDO, CENTRANDO, AVANZANDO, PATEANDO_adelante};
TrackingState trackingState = GIRANDO;

unsigned long millis_inicio_estado = 0; 

void girar() 
{
    motor1(100 * g, 0);
    motor2(100 * g, 0);
    motor3(100 * g, 0);
}

void detenerMotores() 
{
  motor1(0, 0); 
  motor2(0, 0); 
  motor3(0, 0); 
}

void avanzarAlFrente() 
{
  motor2(50, 0);
  motor1(50, 1);
  motor3(0, 0);
}

void setup() 
{ 
  InitializeZircon();
  Serial.begin(BAUD_RATE);
  Serial.println("estamos en setup");
  while (!Serial && millis() < 5000);
  Serial1.begin(BAUD_RATE);
  Serial.println("Decodificador Teensy iniciado.");
}

void loop() 
{ 
  // Decodificar datos UART de la cámara OpenMV
  // Protocolo: [201][Xp][Yp][202][Xa][Ya]
  if (Serial1.available() >= 6)
  {
    int header1 = Serial1.read();
    int xp = Serial1.read();
    int yp = Serial1.read();
    int header2 = Serial1.read(); 
    int xa = Serial1.read();
    int ya = Serial1.read();

    if (header1 == 201 && header2 == 202)
    {
      decodedXp = xp / 2.0;
      decodedXa = xa / 2.0;

      if(yp == 0) { decodedYp = 0; }
      else { decodedYp = (yp / 2.0) - 50.0; }

      if(ya == 0) { decodedYa = 0; }
      else { decodedYa = (ya / 2.0) - 50.0; }

      anguloRadPelota = atan2(decodedYp, decodedXp);
      anguloPelota = anguloRadPelota * 180.0 / PI;
      anguloRadArco = atan2(decodedYp, decodedXp);
      anguloArco = anguloRadArco * 180.0 / PI;

      haypelota = (xp != 0);
      hayarco = (xa != 0);
    }
  }

  // MÁQUINA DE ESTADOS
  switch(trackingState) 
  {
    case GIRANDO:
      if (haypelota) { trackingState = AVANZANDO; }
      else { girar(); }
      break;

    case AVANZANDO: 
      avanzarAlFrente(); 
      if (haypelota && decodedXp <= tolerancia_cercania) 
        { trackingState = CENTRANDO; }
      if (!haypelota) { trackingState = GIRANDO; } 
      break; 
    
    case CENTRANDO:
      k = 0.6;
      if (decodedYa >= 0) {
        motor1(60*k, 0); motor2(60*k, 0); motor3(105*k, 1); 
      } else {
        motor1(60*k, 1); motor2(60*k, 1); motor3(105*k, 0);
      }
      if(hayarco && haypelota && abs(decodedYa) - abs(decodedYp) <= tolerancia_centrado) {  
        trackingState = PATEANDO_adelante; 
        millis_inicio_estado = millis();
      }
      break; 

    case PATEANDO_adelante: 
      motor2(100, 0); motor1(100, 1); motor3(0, 0); 
      if(millis() - millis_inicio_estado <= 2000) {
        motor2(0, 0); motor1(0, 1); motor3(0, 0); 
        trackingState = GIRANDO;   
      }
      break;
  }
}
