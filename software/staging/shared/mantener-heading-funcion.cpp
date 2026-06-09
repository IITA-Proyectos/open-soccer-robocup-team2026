// =============================================================================
// MANTENER HEADING CON PID — versión función reutilizable
// Archivo: staging/shared/test-mantener-heading-1.cpp
//
// USO EN EL LOOP:
//
//   // Para mantener 90°:
//   mantenerHeading(90.0);
//
//   // Para dejar de mantener (en cualquier branch del if/else):
//   pararMantenerHeading();
//
//   // Ejemplo típico:
//   if (quieroMantenerHeading) {
//     mantenerHeading(anguloObjetivo);
//   } else {
//     pararMantenerHeading();
//     // ... acá hacés otra cosa con los motores
//   }
//
//   // Si querés el ángulo actual del gyro (0–360°):
//   float h = leerHeadingAbsoluto();
//
// CAMBIOS RESPECTO A test-mantener-heading-0:
//   - mantenerHeading(float anguloObjetivo) reemplaza a aplicarCorreccion().
//     Ahora recibe el ángulo deseado como parámetro (grados absolutos, 0–360°).
//   - leerHeadingAbsoluto() devuelve el ángulo real del BNO055 sin offset.
//     El error se calcula internamente en mantenerHeading().
//   - pararMantenerHeading() frena los motores y resetea el PID para la
//     próxima activación.
//   - El PID se resetea solo la primera vez que llamás mantenerHeading()
//     después de haberla desactivado (sin que tengas que acordarte).
//
// SIN CAMBIOS:
//   - Pines, constantes PID, funciones de motor, inicializarGyro(), botón.
//   - La máquina de estados del test sigue igual, solo el MODO_MANTENER
//     ahora llama a mantenerHeading(anguloObjetivo) en vez de aplicarCorreccion().
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// SELECCIÓN DE ROBOT
// =============================================================================
//#define ROBOT1   // Arquero
#define ROBOT2   // Delantero

// ---- PINES ----
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

#define PIN_BOTON  9

// =============================================================================
// CONFIGURACIÓN GENERAL
// =============================================================================
const long  BAUD_RATE           = 19200;
const float UMBRAL_ERROR_GRADOS = 3.0;
const int   VEL_MINIMA          = 60;
const int   VEL_MAXIMA          = 200;

// =============================================================================
// *** ACÁ PONÉS EL ÁNGULO QUE QUERÉS MANTENER ***
// Es el ángulo absoluto que devuelve el BNO055: 0° a 360°.
// 0° = norte magnético. Aumenta en sentido horario.
// =============================================================================
const float ANGULO_OBJETIVO = 45.0;

// =============================================================================
// PARÁMETROS PID
// =============================================================================
float Kp = 0.30;
float Ki = 0.05;
float Kd = 1.8;

const float MAX_CORRECCION = 150.0;
const float INTEGRAL_MAX   = 40.0;
const float FACTOR_M3      = 0.4;
const int   DIR_M3_POSITIVO = 1;

// =============================================================================
// GIRÓSCOPO BNO055
// =============================================================================
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
bool bnoOK = false;

// =============================================================================
// VARIABLES INTERNAS DEL PID
// (no las toques desde el loop — usá mantenerHeading/pararMantenerHeading)
// =============================================================================
static float         _errorAnterior      = 0.0;
static float         _integral           = 0.0;
static unsigned long _tiempoAnteriorPID  = 0;
static bool          _pidActivo          = false;  // false = próxima llamada resetea el PID

// =============================================================================
// VARIABLES DE CONTROL (test)
// =============================================================================
bool motorsPaused  = false;
bool botonAnterior = false;
unsigned long ultimoPrint    = 0;
unsigned long contadorCiclos = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest { ESPERANDO_INICIO, TEST_HEADING, MODO_MANTENER };
EstadoTest estadoTest = ESPERANDO_INICIO;

// =============================================================================
// FUNCIONES DE MOTOR (sin cambios)
// =============================================================================
void motor1(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM1, vel);
  if      (dir > 0) { digitalWrite(INA1, 1); digitalWrite(INB1, 0); }
  else if (dir < 0) { digitalWrite(INA1, 0); digitalWrite(INB1, 1); }
  else              { digitalWrite(INA1, 0); digitalWrite(INB1, 0); }
}

void motor2(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM2, vel);
  if      (dir > 0) { digitalWrite(INA2, 0); digitalWrite(INB2, 1); }
  else if (dir < 0) { digitalWrite(INA2, 1); digitalWrite(INB2, 0); }
  else              { digitalWrite(INA2, 0); digitalWrite(INB2, 0); }
}

void motor3(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM3, vel);
  if      (dir > 0) { digitalWrite(INA3, 1); digitalWrite(INB3, 0); }
  else if (dir < 0) { digitalWrite(INA3, 0); digitalWrite(INB3, 1); }
  else              { digitalWrite(INA3, 0); digitalWrite(INB3, 0); }
}

void parar() {
  motor1(0, 0);
  motor2(0, 0);
  motor3(0, 0);
}

// =============================================================================
// leerHeadingAbsoluto()
// Devuelve el ángulo real del BNO055 en grados (0–360°).
// Sin offset — el objetivo se pasa directamente a mantenerHeading().
// =============================================================================
float leerHeadingAbsoluto() {
  if (!bnoOK) return 0.0;
  sensors_event_t event;
  bno.getEvent(&event);
  return event.orientation.x;
}

// =============================================================================
// normalizarError()
// Convierte cualquier diferencia de ángulos al rango [-180, +180].
// Necesario para que el camino más corto sea siempre el que se toma.
// Ejemplos:  350° - 10° = 340°  →  -20°  (gira izquierda, no 340° a la derecha)
//            10° - 350° = -340° →  +20°  (gira derecha, no 340° a la izquierda)
// =============================================================================
float normalizarError(float error) {
  while (error >  180.0) error -= 360.0;
  while (error < -180.0) error += 360.0;
  return error;
}

// =============================================================================
// mantenerHeading(float anguloObjetivo)
//
// Llamá esta función en el loop() cada ciclo que quieras que el robot
// corrija su orientación hacia el ángulo indicado.
//
//   anguloObjetivo: ángulo absoluto en grados, igual a lo que lee el BNO055
//                   (0° = norte magnético, aumenta en sentido horario).
//
// La primera vez que la llamás después de pararMantenerHeading() (o al inicio),
// el PID se resetea automáticamente — no tenés que hacer nada extra.
//
// Si el error es menor que UMBRAL_ERROR_GRADOS, los motores se apagan solos.
// =============================================================================
void mantenerHeading(float anguloObjetivo) {

  // Reset automático si estaba inactivo
  if (!_pidActivo) {
    _errorAnterior     = 0.0;
    _integral          = 0.0;
    _tiempoAnteriorPID = millis();
    _pidActivo         = true;
  }

  float headingActual = leerHeadingAbsoluto();
  float error         = normalizarError(anguloObjetivo - headingActual);

  // --- PID ---
  unsigned long ahora = millis();
  float dt = (ahora - _tiempoAnteriorPID) / 1000.0;
  if (dt <= 0.001) dt = 0.01;
  _tiempoAnteriorPID = ahora;

  float P = Kp * error;

  _integral += error * dt;
  _integral  = constrain(_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I    = Ki * _integral;

  float derivada    = (error - _errorAnterior) / dt;
  float D           = Kd * derivada;
  _errorAnterior    = error;

  float correccion = constrain(P + I + D, -MAX_CORRECCION, MAX_CORRECCION);

  // Dentro del umbral: apagar motores y salir
  if (abs(error) < UMBRAL_ERROR_GRADOS) {
    parar();
    return;
  }

  // --- Velocidades ---
  int velM1 = (int)( correccion);
  int velM2 = (int)(-correccion);
  int velM3 = (int)( correccion * FACTOR_M3);

  // Velocidad mínima para que el motor realmente se mueva
  auto aplicarMinima = [](int v) -> int {
    if (abs(v) > 0 && abs(v) < VEL_MINIMA)
      return (v > 0) ? VEL_MINIMA : -VEL_MINIMA;
    return v;
  };
  velM1 = aplicarMinima(velM1);
  velM2 = aplicarMinima(velM2);
  velM3 = aplicarMinima(velM3);

  velM1 = constrain(velM1, -VEL_MAXIMA, VEL_MAXIMA);
  velM2 = constrain(velM2, -VEL_MAXIMA, VEL_MAXIMA);
  velM3 = constrain(velM3, -VEL_MAXIMA, VEL_MAXIMA);

  int dirM1 = (velM1 > 0) ? 1 : ((velM1 < 0) ? -1 : 0);
  int dirM2 = (velM2 > 0) ? 1 : ((velM2 < 0) ? -1 : 0);
  int dirM3 = (velM3 > 0) ? DIR_M3_POSITIVO : ((velM3 < 0) ? -DIR_M3_POSITIVO : 0);

  motor1(abs(velM1), dirM1);
  motor2(abs(velM2), dirM2);
  motor3(abs(velM3), dirM3);

  contadorCiclos++;

  // --- Debug serial cada 200ms ---
  if (millis() - ultimoPrint > 200) {
    Serial.print(F("  [PID] Obj:"));
    Serial.print(anguloObjetivo, 1);
    Serial.print(F("° Act:"));
    Serial.print(headingActual, 1);
    Serial.print(F("° Err:"));
    if (error >= 0) Serial.print('+');
    Serial.print(error, 1);
    Serial.print(F("° | Corr:"));
    Serial.print(correccion, 1);
    Serial.print(F(" | M1:"));   Serial.print(velM1);
    Serial.print(F(" M2:"));     Serial.print(velM2);
    Serial.print(F(" M3:"));     Serial.print(velM3);
    Serial.println();
    ultimoPrint = millis();
  }
}

// =============================================================================
// pararMantenerHeading()
//
// Llamá esta función cuando ya no querés corregir el heading.
// Frena los motores y marca el PID como inactivo, para que la próxima
// llamada a mantenerHeading() arranque limpio.
// =============================================================================
void pararMantenerHeading() {
  parar();
  _pidActivo = false;
}

// =============================================================================
// FUNCIONES DE APOYO (sin cambios relevantes)
// =============================================================================

bool inicializarGyro() {
  Serial.println(F("\n=== INICIALIZANDO GIROSCOPO BNO055 ==="));
  Serial.println(F(">>> NO LO MUEVAS <<<\n"));

  unsigned long inicio = millis();
  while (!bno.begin()) {
    if (millis() - inicio > 3000) { Serial.println(F("ERROR!")); return false; }
    delay(100);
  }
  Serial.println(F("BNO055 detectado."));
  bno.setExtCrystalUse(true);
  delay(1000);

  Serial.println(F("Calibrando giróscopo (5 s)..."));
  inicio = millis();
  while (millis() - inicio < 5000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print(F("  SYS=")); Serial.print(sys);
    Serial.print(F(" GYR=")); Serial.print(gyro);
    Serial.print(F(" ACC=")); Serial.print(accel);
    Serial.print(F(" MAG=")); Serial.println(mag);
    if (gyro >= 3) { Serial.println(F("  -> Calibrado!")); break; }
    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
    delay(250);
  }

  Serial.println(F("=== GIROSCOPO LISTO ===\n"));
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}

bool botonPresionado() {
  bool estadoActual = (digitalRead(PIN_BOTON) == HIGH);
  if (estadoActual && !botonAnterior) { botonAnterior = estadoActual; delay(50); return true; }
  botonAnterior = estadoActual;
  return false;
}

void esperarSoltarBoton() {
  while (digitalRead(PIN_BOTON) == HIGH) delay(10);
  delay(50);
  botonAnterior = false;
}

void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {

      case 'r': {  // Recalibrar: el ángulo actual pasa a ser el nuevo objetivo
        pararMantenerHeading();  // resetea PID
        Serial.print(F("  >> PID reseteado. Objetivo sigue siendo: "));
        Serial.print(ANGULO_OBJETIVO, 1);
        Serial.println(F("°"));
        break;
      }

      case 'p':  // Pausa/reanuda
        motorsPaused = !motorsPaused;
        if (motorsPaused) {
          pararMantenerHeading();
          Serial.println(F("  >> PAUSADO (enviar 'p' para reanudar)"));
        } else {
          Serial.println(F("  >> REACTIVADO"));
        }
        break;

      case 'v':
        Serial.println(F("\n  === VALORES ACTUALES ==="));
        Serial.print(F("  Kp=")); Serial.print(Kp);
        Serial.print(F("  Ki=")); Serial.print(Ki);
        Serial.print(F("  Kd=")); Serial.println(Kd);
        Serial.print(F("  Objetivo=")); Serial.print(ANGULO_OBJETIVO, 1); Serial.println(F("°"));
        Serial.print(F("  Actual  =")); Serial.print(leerHeadingAbsoluto(), 1); Serial.println(F("°"));
        Serial.print(F("  Ciclos  =")); Serial.println(contadorCiclos);
        Serial.println(F("  ========================\n"));
        break;

      case '+': Kp += 0.5; Serial.print(F("  >> Kp = ")); Serial.println(Kp); break;
      case '-': Kp  = max(0.0f, Kp - 0.5f); Serial.print(F("  >> Kp = ")); Serial.println(Kp); break;
    }
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);

  Serial.println(F("\n****************************************************"));
  Serial.println(F("*  TEST: MANTENER HEADING CON PID — v1            *"));
  Serial.println(F("****************************************************\n"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(PIN_BOTON, INPUT);

  pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);
  parar();

  bnoOK = inicializarGyro();
  if (!bnoOK) {
    Serial.println(F("*** ERROR FATAL: BNO055 no detectado ***"));
    while (true) { digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW); delay(100); }
  }

  Serial.println(F("Presioná BOTON para test de heading."));
}

// =============================================================================
// LOOP PRINCIPAL
//
// Acá podés ver exactamente cómo se usa mantenerHeading() / pararMantenerHeading().
// En tu código real de juego, simplemente copiás ese patrón.
// =============================================================================
void loop() {

  procesarComandoSerial();

  switch (estadoTest) {

    // -------------------------------------------------------------------------
    case ESPERANDO_INICIO:
      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = TEST_HEADING;
        Serial.println(F("\n>>> TEST HEADING — girá el robot para verificar el BNO055."));
        Serial.println(F("Presioná BOTON para activar MODO MANTENER.\n"));
      }
      break;

    // -------------------------------------------------------------------------
    case TEST_HEADING:
      if (millis() - ultimoPrint > 200) {
        float h = leerHeadingAbsoluto();
        Serial.print(F("  [HEADING] ")); Serial.print(h, 1); Serial.println(F("°"));
        digitalWrite(LED_BUILTIN, (abs(h) > 20.0) ? ((millis() / 200) % 2) : 1);
        ultimoPrint = millis();
      }
      if (botonPresionado()) {
        esperarSoltarBoton();
        contadorCiclos = 0;
        estadoTest = MODO_MANTENER;
        Serial.println(F("\n**** MODO MANTENER ACTIVADO ****"));
        Serial.print(F("Ángulo objetivo: ")); Serial.print(ANGULO_OBJETIVO, 1); Serial.println(F("°"));
        Serial.println(F("Comandos: 'r'=resetear PID  'p'=pausa  'v'=valores  '+'/'-'=ajustar Kp\n"));
      }
      break;

    // -------------------------------------------------------------------------
    // MODO MANTENER — toda la lógica está en mantenerHeading().
    // Para desactivar desde el loop: llamá pararMantenerHeading() y salí del estado.
    // -------------------------------------------------------------------------
    case MODO_MANTENER:
      if (!motorsPaused) {
        mantenerHeading(ANGULO_OBJETIVO);   // ← así de simple
      } else {
        if (millis() - ultimoPrint > 500) {
          Serial.print(F("  [PAUSADO] Actual: ")); Serial.print(leerHeadingAbsoluto(), 1); Serial.println(F("°"));
          ultimoPrint = millis();
        }
      }
      break;
  }
}