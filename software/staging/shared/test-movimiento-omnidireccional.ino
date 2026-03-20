// =============================================================================
// TEST: Primitivas de movimiento omnidireccional con PD heading
// Archivo: staging/shared/test-movimiento-omnidireccional.ino
//
// PROPÓSITO: Probar la función moverRobot() que combina traslación en
//           cualquier dirección con control PD de heading por giróscopo.
//
// INSTRUCCIONES:
//   1. Subir a cualquiera de los robots
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER durante calibración (5 seg)
//   4. Presionar boton 1 para avanzar por los tests
//   5. Cada test dura 3 segundos. Observar movimiento.
//   6. Anotar si cada test hace lo esperado.
//
// REFERENCIA: docs/internal/cinematica-omnidireccional-movimientos.md
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <zirconLib.h>

// --- SELECCION DE ROBOT ---
// Descomentar UNO:
//#define ROBOT1
#define ROBOT2

#if defined(ROBOT2)
  #define INA1 8
  #define INB1 7
  #define PWM1 6
  #define INA2 11
  #define INB2 12
  #define PWM2 4
  #define INA3 2
  #define INB3 5
  #define PWM3 3
#endif

#if defined(ROBOT1)
  #define INA1 2
  #define INB1 5
  #define PWM1 3
  #define INA2 8
  #define INB2 7
  #define PWM2 6
  #define INA3 11
  #define INB3 12
  #define PWM3 4
#endif

// --- CONSTANTES CINEMATICA ---
// Estos son los valores a CALIBRAR en la cancha
const float Kp_heading = 2.0;     // Proporcional heading (subir si corrige lento)
const float Kd_heading = 0.05;    // Derivativo heading (subir si oscila)
const float OMEGA_MAX = 100.0;    // Velocidad maxima de rotacion
const float L_ROTACION = 0.6;     // Factor escala rotacion (bajar si gira mucho)
const int MOTOR_MAX = 200;        // PWM maximo

// Si algun motor gira al reves, cambiar el signo aca:
const int SIGNO_M1 = 1;   // 1 o -1
const int SIGNO_M2 = 1;   // 1 o -1
const int SIGNO_M3 = 1;   // 1 o -1

// --- GIROSCOPO ---
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

// --- PID ---
float headingErrorAnterior = 0;
unsigned long pidTiempoAnterior = 0;

// --- TEST ---
int testActual = 0;
const int TOTAL_TESTS = 9;
unsigned long testInicio = 0;
const unsigned long TEST_DURACION = 3000; // 3 segundos por test
bool testCorriendo = false;

// ======================================================
// FUNCIONES DE MOVIMIENTO
// ======================================================

void aplicarMotor(int pinPWM, int pinINA, int pinINB, int velocidad, int signo) {
  velocidad *= signo;
  if (velocidad >= 0) {
    digitalWrite(pinINA, 1);
    digitalWrite(pinINB, 0);
  } else {
    digitalWrite(pinINA, 0);
    digitalWrite(pinINB, 1);
  }
  analogWrite(pinPWM, constrain(abs(velocidad), 0, 255));
}

void parar() {
  analogWrite(PWM1, 0); digitalWrite(INA1, 0); digitalWrite(INB1, 0);
  analogWrite(PWM2, 0); digitalWrite(INA2, 0); digitalWrite(INB2, 0);
  analogWrite(PWM3, 0); digitalWrite(INA3, 0); digitalWrite(INB3, 0);
}

float leerHeading() {
  if (!bnoOK) return 0;
  sensors_event_t event;
  bno.getEvent(&event);
  float heading = event.orientation.x - headingOffset;
  if (heading > 180) heading -= 360;
  if (heading < -180) heading += 360;
  return heading;
}

// ======================================================
// FUNCION PRINCIPAL: moverRobot()
// ======================================================
//
// velocidad:    0-100, rapidez de traslacion
// direccion:    angulo en grados, marco del robot
//               0 = adelante, 90 = derecha, 180 = atras, -90 = izquierda
// headingObj:   heading deseado en grados, marco cancha
//               0 = al arco contrincante (referencia de encendido)
//               Usa PD con giroscopo para mantener este heading.
//
void moverRobot(float velocidad, float direccionGrados, float headingObj) {
  // 1. Leer heading actual
  float headingActual = leerHeading();

  // 2. PD sobre heading
  float headingError = headingObj - headingActual;
  if (headingError > 180) headingError -= 360;
  if (headingError < -180) headingError += 360;

  unsigned long ahora = millis();
  float dt = (ahora - pidTiempoAnterior) / 1000.0;
  if (dt < 0.001) dt = 0.001;
  float derivada = (headingError - headingErrorAnterior) / dt;
  headingErrorAnterior = headingError;
  pidTiempoAnterior = ahora;

  float omega = headingError * Kp_heading + derivada * Kd_heading;
  omega = constrain(omega, -OMEGA_MAX, OMEGA_MAX);

  // 3. Polar a cartesiano
  float direccionRad = direccionGrados * PI / 180.0;
  float vx = velocidad * sin(direccionRad);
  float vy = velocidad * cos(direccionRad);

  // 4. Cinematica inversa (3 ruedas omni a 30/150/270 grados)
  float m1 = -0.5 * vx + 0.866 * vy + L_ROTACION * omega;
  float m2 = -0.5 * vx - 0.866 * vy + L_ROTACION * omega;
  float m3 =  1.0 * vx + 0.0   * vy + L_ROTACION * omega;

  // 5. Saturacion proporcional
  float maxM = max(abs(m1), max(abs(m2), abs(m3)));
  if (maxM > MOTOR_MAX) {
    float escala = (float)MOTOR_MAX / maxM;
    m1 *= escala;
    m2 *= escala;
    m3 *= escala;
  }

  // 6. Aplicar a motores
  aplicarMotor(PWM1, INA1, INB1, (int)m1, SIGNO_M1);
  aplicarMotor(PWM2, INA2, INB2, (int)m2, SIGNO_M2);
  aplicarMotor(PWM3, INA3, INB3, (int)m3, SIGNO_M3);
}

// ======================================================
// DESCRIPCION DE TESTS
// ======================================================

struct TestMovimiento {
  float velocidad;
  float direccion;
  float headingObj;
  const char* nombre;
};

TestMovimiento tests[] = {
  { 0,    0,   0,   "Quieto mirando al frente (heading 0)" },
  { 60,   0,   0,   "Avanzar recto mirando al frente" },
  { 60,  180,  0,   "Retroceder mirando al frente" },
  { 60,   90,  0,   "Lateral DERECHA mirando al frente" },
  { 60,  -90,  0,   "Lateral IZQUIERDA mirando al frente" },
  { 60,   45,  0,   "Diagonal adelante-derecha" },
  { 0,    0,   90,  "Solo girar a heading 90 (derecha)" },
  { 0,    0,  -90,  "Solo girar a heading -90 (izquierda)" },
  { 50,   90,  0,   "Orbitar: lateral derecha + mirar al frente" },
};

void imprimirTest(int i) {
  Serial.println("\n========================================");
  Serial.print("TEST "); Serial.print(i + 1);
  Serial.print("/"); Serial.print(TOTAL_TESTS);
  Serial.print(": "); Serial.println(tests[i].nombre);
  Serial.print("  vel="); Serial.print(tests[i].velocidad);
  Serial.print(" dir="); Serial.print(tests[i].direccion);
  Serial.print(" heading="); Serial.println(tests[i].headingObj);
  Serial.println("  Observar 3 segundos...");
  Serial.println("========================================");
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(19200);
  InitializeZircon();
  pinMode(LED_BUILTIN, OUTPUT);

  // Pines de motor como output
  pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);

  Serial.println("=== TEST MOVIMIENTO OMNIDIRECCIONAL ===");
  Serial.println("Inicializando BNO055 en IMUPLUS...");

  // BNO055 en IMUPLUS
  unsigned long inicio = millis();
  while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {
    if (millis() - inicio > 3000) {
      Serial.println("ERROR: BNO055 no detectado!");
      bnoOK = false;
      break;
    }
    delay(100);
  }
  if (bnoOK || millis() - inicio <= 3000) {
    bnoOK = true;
    bno.setExtCrystalUse(true);
    delay(1000);

    // Esperar calibracion gyro
    Serial.println("Calibrando gyro (NO MOVER!)...");
    inicio = millis();
    while (millis() - inicio < 2000) {
      uint8_t sys, gyro, accel, mag;
      bno.getCalibration(&sys, &gyro, &accel, &mag);
      if (gyro >= 3) break;
      delay(100);
    }

    // Capturar heading
    float suma = 0;
    for (int i = 0; i < 10; i++) {
      sensors_event_t event;
      bno.getEvent(&event);
      suma += event.orientation.x;
      delay(20);
    }
    headingOffset = suma / 10.0;
    Serial.print("Heading offset: "); Serial.println(headingOffset);
  }

  pidTiempoAnterior = millis();

  Serial.println();
  Serial.println("=== LISTO ===");
  Serial.println("Presionar BOTON 1 para comenzar los tests.");
  Serial.println("Cada test dura 3 segundos.");
  Serial.println("Presionar BOTON 1 de nuevo para el siguiente test.");
  Serial.println();

  // Esperar boton
  while (readButton(1) == LOW) { delay(50); }
  delay(300); // anti-rebote

  testActual = 0;
  imprimirTest(testActual);
  testInicio = millis();
  testCorriendo = true;
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  if (testCorriendo) {
    // Ejecutar movimiento del test actual
    moverRobot(
      tests[testActual].velocidad,
      tests[testActual].direccion,
      tests[testActual].headingObj
    );

    // Imprimir heading actual cada 200ms
    static unsigned long ultimoPrint = 0;
    if (millis() - ultimoPrint > 200) {
      float h = leerHeading();
      Serial.print("  heading: ");
      if (h >= 0) Serial.print("+");
      Serial.println(h, 1);
      ultimoPrint = millis();
    }

    // Si pasan 3 segundos, parar
    if (millis() - testInicio >= TEST_DURACION) {
      parar();
      testCorriendo = false;
      Serial.println("  >> TEST TERMINADO. Presionar BOTON 1 para siguiente.");
    }
  }
  else {
    // Esperando boton para siguiente test
    if (readButton(1) == HIGH) {
      delay(300); // anti-rebote
      testActual++;

      if (testActual >= TOTAL_TESTS) {
        Serial.println("\n=== TODOS LOS TESTS COMPLETADOS ===");
        Serial.println("Reiniciar para volver a correr.");
        while (true) { delay(1000); }
      }

      imprimirTest(testActual);
      headingErrorAnterior = 0; // resetear PD
      pidTiempoAnterior = millis();
      testInicio = millis();
      testCorriendo = true;
    }
  }
}
