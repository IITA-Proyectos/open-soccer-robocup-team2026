// ============================================================================
// ⚠️ STAGING CONGELADO (2026-06-03) — NO subir más material a software/staging/.
// Antes de tocar o agregar algo acá, LEÉ:
//   software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md
// Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
//   software/teensy/Soccer 2026/src/  (ver el "mapa de reúso" del documento).
// ============================================================================

// =============================================================================
// TEST: BNO055 en modo IMUPLUS (sin magnetómetro)
// Archivo: staging/shared/test-bno055-imuplus.ino
// Propósito: Verificar que el BNO055 funciona correctamente en IMUPLUS
//           antes de integrarlo al programa completo del robot.
//
// INSTRUCCIONES:
//   1. Subir este programa al Teensy (cualquiera de los dos robots)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER el robot durante los primeros 5 segundos
//   4. Observar los mensajes de calibración
//   5. Cuando diga "LISTO", girar el robot y verificar que el heading cambia
//   6. Volver a la posición original y verificar que vuelve a ~0°
//
// QUÉ BUSCAR:
//   - El heading debería arrancar en 0° (no en 35° como antes)
//   - Al girar 90° a la derecha, debería mostrar ~-90°
//   - Al girar 90° a la izquierda, debería mostrar ~+90°
//   - Observar si hay drift: dejar 5 minutos quieto y ver cuánto se desvía
//
// CAMBIO CLAVE: bno.begin(OPERATION_MODE_IMUPLUS) en vez de bno.begin()
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

float headingOffset = 0;
bool bnoOK = false;

void setup() {
  Serial.begin(19200);
  pinMode(LED_BUILTIN, OUTPUT);

  // --- LED parpadeando = inicializando ---
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, HIGH); delay(100);
    digitalWrite(LED_BUILTIN, LOW);  delay(100);
  }

  Serial.println("=== TEST BNO055 IMUPLUS ===");
  Serial.println("NO MOVER EL ROBOT!");
  Serial.println();

  // --- PASO 1: Detectar sensor con timeout ---
  Serial.print("Detectando BNO055... ");
  unsigned long inicio = millis();
  while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {
    if (millis() - inicio > 3000) {
      Serial.println("ERROR: No detectado!");
      Serial.println("Verificar cables I2C (SDA/SCL) y alimentacion");
      bnoOK = false;
      // Parpadeo rápido = error
      while (true) {
        digitalWrite(LED_BUILTIN, HIGH); delay(50);
        digitalWrite(LED_BUILTIN, LOW);  delay(50);
      }
    }
    delay(100);
  }
  Serial.println("OK!");
  bnoOK = true;

  // --- PASO 2: Cristal externo ---
  bno.setExtCrystalUse(true);

  // --- PASO 3: Esperar estabilización (1 segundo) ---
  Serial.println("Esperando estabilizacion (1s)...");
  delay(1000);

  // --- PASO 4: Esperar calibración del giróscopo ---
  Serial.println("Calibrando giroscopo (no mover!)...");
  inicio = millis();
  while (millis() - inicio < 3000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print("  Calibracion -> SYS:"); Serial.print(sys);
    Serial.print(" GYR:"); Serial.print(gyro);
    Serial.print(" ACC:"); Serial.print(accel);
    Serial.print(" MAG:"); Serial.println(mag);

    if (gyro >= 3) {
      Serial.print("  Gyro calibrado en ");
      Serial.print(millis() - inicio);
      Serial.println("ms!");
      break;
    }
    delay(250);
  }

  // --- PASO 5: Capturar heading inicial (promedio de 10 lecturas) ---
  Serial.println("Capturando heading inicial (10 lecturas)...");
  float suma = 0;
  for (int i = 0; i < 10; i++) {
    sensors_event_t event;
    bno.getEvent(&event);
    suma += event.orientation.x;
    Serial.print("  Lectura "); Serial.print(i+1);
    Serial.print(": "); Serial.println(event.orientation.x);
    delay(50);
  }
  headingOffset = suma / 10.0;
  Serial.print("Heading offset = "); Serial.println(headingOffset);

  // --- LED fijo = LISTO ---
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println();
  Serial.println("=== LISTO! Girar el robot para probar ===");
  Serial.println("Formato: heading | error | raw | calibGYR");
  Serial.println();
}

void loop() {
  if (!bnoOK) return;

  // Leer heading
  sensors_event_t event;
  bno.getEvent(&event);
  float raw = event.orientation.x;
  float heading = raw - headingOffset;

  // Normalizar a -180..+180
  if (heading > 180) heading -= 360;
  if (heading < -180) heading += 360;

  // Leer calibración
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);

  // Mostrar
  Serial.print("Heading: ");
  if (heading >= 0) Serial.print("+");
  Serial.print(heading, 1);
  Serial.print("\t| raw: ");
  Serial.print(raw, 1);
  Serial.print("\t| GYR:");
  Serial.print(gyro);
  Serial.print(" SYS:");
  Serial.println(sys);

  // LED indica heading: fijo=frente, parpadeo=girado más de 30°
  if (abs(heading) > 30) {
    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  delay(100); // 10 Hz para el serial monitor
}
