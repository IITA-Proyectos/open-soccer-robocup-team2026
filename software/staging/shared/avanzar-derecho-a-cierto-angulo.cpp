// =============================================================================
// TEST: mantenerHeading + avanzarDerecho
// =============================================================================
//
// Al encender:
//   1. Calibra el giróscopo automáticamente (espera 5 s o hasta gyro >= 3).
//   2. Entra directo al switch de estados — sin botón.
//
// Estados del test:
//
//   TEST_HEADING    → imprime el heading actual cada 200 ms.
//                     Botón → pasa a MODO_MANTENER.
//
//   MODO_MANTENER   → mantiene el robot en ANGULO_OBJETIVO usando el PID.
//                     Botón → pasa a MODO_AVANZAR.
//
//   MODO_AVANZAR    → avanza recto a POTENCIA_AVANCE mirando ANGULO_AVANCE
//                     durante TIEMPO_AVANCE_MS ms, luego para y vuelve a
//                     TEST_HEADING para que puedas repetir.
//                     Botón → cancela antes de que termine el tiempo.
//
// Comandos Serial (en cualquier estado):
//   'r' → resetea PID
//   'p' → pausa / reanuda motores
//   'v' → muestra valores actuales
//   '+' / '-' → ajusta Kp ±0.5
//
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// SELECCIÓN DE ROBOT
// =============================================================================
//#define ROBOT1   // Arquero
#define ROBOT2     // Delantero

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
// *** PARÁMETROS DEL TEST — modificá estos para probar ***
// =============================================================================

// mantenerHeading: ángulo que mantiene el robot quieto
const float ANGULO_OBJETIVO   = 45.0;

// avanzarDerecho: ángulo al que mira mientras avanza
const float ANGULO_AVANCE     = 45.0;

// avanzarDerecho: potencia de avance (0–255)
const int   POTENCIA_AVANCE   = 70;

// avanzarDerecho: cuántos ms avanza antes de frenar solo
const unsigned long TIEMPO_AVANCE_MS = 6000;

// =============================================================================
// PARÁMETROS PID
// =============================================================================
float Kp = 0.30;
float Ki = 0.05;
float Kd = 1.8;

const float MAX_CORRECCION  = 150.0;
const float INTEGRAL_MAX    = 40.0;
const float FACTOR_M3       = 0.4;
const int   DIR_M3_POSITIVO = 1;

// =============================================================================
// GIRÓSCOPO BNO055
// =============================================================================
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
bool bnoOK = false;

// =============================================================================
// VARIABLES INTERNAS DEL PID
// =============================================================================
static float         _errorAnterior     = 0.0;
static float         _integral          = 0.0;
static unsigned long _tiempoAnteriorPID = 0;
static bool          _pidActivo         = false;

// =============================================================================
// VARIABLES DE CONTROL
// =============================================================================
bool motorsPaused  = false;
bool botonAnterior = false;
unsigned long ultimoPrint    = 0;
unsigned long contadorCiclos = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest { TEST_HEADING, MODO_MANTENER, MODO_AVANZAR };
EstadoTest estadoTest = TEST_HEADING;

unsigned long tiempoInicioAvance = 0;  // para medir TIEMPO_AVANCE_MS

// =============================================================================
// FUNCIONES DE MOTOR
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
// =============================================================================
float leerHeadingAbsoluto() {
  if (!bnoOK) return 0.0;
  sensors_event_t event;
  bno.getEvent(&event);
  return event.orientation.x;
}

// =============================================================================
// normalizarError()
// =============================================================================
float normalizarError(float error) {
  while (error >  180.0) error -= 360.0;
  while (error < -180.0) error += 360.0;
  return error;
}

// =============================================================================
// mantenerHeading(float anguloObjetivo)
//
// Llamá en el loop() cada ciclo para corregir orientación.
// Se resetea solo la primera vez tras pararMantenerHeading().
// Si el error < UMBRAL_ERROR_GRADOS, para los motores.
// =============================================================================
void mantenerHeading(float anguloObjetivo) {

  if (!_pidActivo) {
    _errorAnterior     = 0.0;
    _integral          = 0.0;
    _tiempoAnteriorPID = millis();
    _pidActivo         = true;
  }

  float headingActual = leerHeadingAbsoluto();
  float error         = normalizarError(anguloObjetivo - headingActual);

  unsigned long ahora = millis();
  float dt = (ahora - _tiempoAnteriorPID) / 1000.0;
  if (dt <= 0.001) dt = 0.01;
  _tiempoAnteriorPID = ahora;

  float P = Kp * error;

  _integral += error * dt;
  _integral  = constrain(_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I    = Ki * _integral;

  float derivada = (error - _errorAnterior) / dt;
  float D        = Kd * derivada;
  _errorAnterior = error;

  float correccion = constrain(P + I + D, -MAX_CORRECCION, MAX_CORRECCION);

  if (abs(error) < UMBRAL_ERROR_GRADOS) {
    parar();
    return;
  }

  int velM1 = (int)( correccion);
  int velM2 = (int)(-correccion);
  int velM3 = (int)( correccion * FACTOR_M3);

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

  if (millis() - ultimoPrint > 200) {
    Serial.print(F("  [PID] Obj:")); Serial.print(anguloObjetivo, 1);
    Serial.print(F("° Act:"));       Serial.print(headingActual, 1);
    Serial.print(F("° Err:"));
    if (error >= 0) Serial.print('+');
    Serial.print(error, 1);
    Serial.print(F("° | Corr:")); Serial.print(correccion, 1);
    Serial.print(F(" | M1:"));    Serial.print(velM1);
    Serial.print(F(" M2:"));      Serial.print(velM2);
    Serial.print(F(" M3:"));      Serial.println(velM3);
    ultimoPrint = millis();
  }
}

// =============================================================================
// pararMantenerHeading()
//
// Frena motores y marca PID inactivo.
// La próxima llamada a mantenerHeading() o avanzarDerecho() arranca limpio.
// =============================================================================
void pararMantenerHeading() {
  parar();
  _pidActivo = false;
}

// =============================================================================
// avanzarDerecho(int potencia, float heading)
//
// Avanza hacia adelante a la potencia indicada mientras corrige el heading
// con el mismo PID de mantenerHeading().
//
// Uso típico en el loop():
//
//   avanzarDerecho(100, 45.0);   // avanza a potencia 100 mirando a 45°
//
// Para frenar:
//   pararMantenerHeading();      // misma función de siempre
//
// Cómo funciona la mezcla avance + corrección:
//   M1 = potencia + correccion   → si hay error a la derecha, M1 acelera
//   M2 = potencia - correccion   → M2 frena para compensar y el robot se endereza
//   M3 = correccion * FACTOR_M3  → solo ayuda a la rotación, sin contribuir al avance
//
// IMPORTANTE: comparte las variables internas del PID con mantenerHeading().
// No llamar a ambas en el mismo ciclo.
// =============================================================================
void avanzarDerecho(int potencia, float heading) {

  if (!_pidActivo) {
    _errorAnterior     = 0.0;
    _integral          = 0.0;
    _tiempoAnteriorPID = millis();
    _pidActivo         = true;
  }

  float headingActual = leerHeadingAbsoluto();
  float error         = normalizarError(heading - headingActual);

  unsigned long ahora = millis();
  float dt = (ahora - _tiempoAnteriorPID) / 1000.0;
  if (dt <= 0.001) dt = 0.01;
  _tiempoAnteriorPID = ahora;

  float P = Kp * error;

  _integral += error * dt;
  _integral  = constrain(_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I    = Ki * _integral;

  float derivada = (error - _errorAnterior) / dt;
  float D        = Kd * derivada;
  _errorAnterior = error;

  float correccion = constrain(P + I + D, -MAX_CORRECCION, MAX_CORRECCION);

  // Combinar potencia de avance + corrección diferencial
  float total_M1 = potencia + correccion;
  float total_M2 = potencia - correccion;
  float total_M3 = correccion * FACTOR_M3;

  // Saturación proporcional: si algún motor supera VEL_MAXIMA, escalar todos
  float maxM = max(abs(total_M1), max(abs(total_M2), abs(total_M3)));
  if (maxM > VEL_MAXIMA) {
    float escala = (float)VEL_MAXIMA / maxM;
    total_M1 *= escala;
    total_M2 *= escala;
    total_M3 *= escala;
  }

  auto aplicarMinima = [](float v) -> int {
    int vi = (int)v;
    if (abs(vi) > 0 && abs(vi) < VEL_MINIMA)
      return (vi > 0) ? VEL_MINIMA : -VEL_MINIMA;
    return vi;
  };

  int velM1 = aplicarMinima(total_M1);
  int velM2 = aplicarMinima(total_M2);
  int velM3 = aplicarMinima(total_M3);

  int dirM1 = (velM1 > 0) ? 1 : ((velM1 < 0) ? -1 : 0);
  int dirM2 = (velM2 > 0) ? 1 : ((velM2 < 0) ? -1 : 0);
  int dirM3 = (velM3 > 0) ? DIR_M3_POSITIVO : ((velM3 < 0) ? -DIR_M3_POSITIVO : 0);

  motor1(abs(velM1), dirM1);
  motor2(abs(velM2), dirM2);
  motor3(abs(velM3), dirM3);

  static unsigned long _ultimoPrintAD = 0;
  if (millis() - _ultimoPrintAD > 200) {
    Serial.print(F("  [AVANZAR] Obj:")); Serial.print(heading, 1);
    Serial.print(F("° Act:"));           Serial.print(headingActual, 1);
    Serial.print(F("° Err:"));
    if (error >= 0) Serial.print('+');
    Serial.print(error, 1);
    Serial.print(F("° | Corr:"));  Serial.print(correccion, 1);
    Serial.print(F(" | M1:"));     Serial.print(velM1);
    Serial.print(F(" M2:"));       Serial.print(velM2);
    Serial.print(F(" M3:"));       Serial.println(velM3);
    _ultimoPrintAD = millis();
  }
}

// =============================================================================
// INICIALIZAR GIRÓSCOPO
// Espera hasta 5 s a que gyro >= 3, sin necesidad de botón.
// =============================================================================
bool inicializarGyro() {
  Serial.println(F("\n=== INICIALIZANDO GIROSCOPO BNO055 ==="));
  Serial.println(F(">>> NO MOVER EL ROBOT <<<\n"));

  unsigned long inicio = millis();
  while (!bno.begin()) {
    if (millis() - inicio > 3000) {
      Serial.println(F("ERROR: BNO055 no responde."));
      return false;
    }
    delay(100);
  }
  Serial.println(F("BNO055 detectado."));
  bno.setExtCrystalUse(true);
  delay(500);

  Serial.println(F("Calibrando giroscopo (max 5 s)..."));
  inicio = millis();
  while (millis() - inicio < 5000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print(F("  SYS=")); Serial.print(sys);
    Serial.print(F(" GYR="));  Serial.print(gyro);
    Serial.print(F(" ACC="));  Serial.print(accel);
    Serial.print(F(" MAG="));  Serial.println(mag);
    if (gyro >= 3) {
      Serial.println(F("  -> Giroscopo calibrado!"));
      break;
    }
    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
    delay(250);
  }

  Serial.println(F("=== GIROSCOPO LISTO ===\n"));
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}

// =============================================================================
// BOTÓN — detección de flanco ascendente con debounce
// =============================================================================
bool botonPresionado() {
  bool estadoActual = (digitalRead(PIN_BOTON) == HIGH);
  if (estadoActual && !botonAnterior) {
    botonAnterior = estadoActual;
    delay(50);
    return true;
  }
  botonAnterior = estadoActual;
  return false;
}

void esperarSoltarBoton() {
  while (digitalRead(PIN_BOTON) == HIGH) delay(10);
  delay(50);
  botonAnterior = false;
}

// =============================================================================
// COMANDOS SERIAL
// =============================================================================
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'r':
        pararMantenerHeading();
        Serial.println(F("  >> PID reseteado."));
        break;
      case 'p':
        motorsPaused = !motorsPaused;
        if (motorsPaused) { pararMantenerHeading(); Serial.println(F("  >> PAUSADO")); }
        else              { Serial.println(F("  >> REACTIVADO")); }
        break;
      case 'v':
        Serial.println(F("\n  === VALORES ACTUALES ==="));
        Serial.print(F("  Kp=")); Serial.print(Kp);
        Serial.print(F("  Ki=")); Serial.print(Ki);
        Serial.print(F("  Kd=")); Serial.println(Kd);
        Serial.print(F("  Heading actual: ")); Serial.print(leerHeadingAbsoluto(), 1); Serial.println(F("°"));
        Serial.print(F("  Ciclos PID: ")); Serial.println(contadorCiclos);
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
  Serial.println(F("*  TEST: mantenerHeading + avanzarDerecho          *"));
  Serial.println(F("****************************************************\n"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(PIN_BOTON, INPUT);

  pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);
  parar();

  // Calibración automática — sin botón
  bnoOK = inicializarGyro();
  if (!bnoOK) {
    Serial.println(F("*** ERROR FATAL: BNO055 no detectado ***"));
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW);  delay(100);
    }
  }

  // Arrancar directo en TEST_HEADING
  estadoTest = TEST_HEADING;
  Serial.println(F(">>> Mostrando heading. Girá el robot para verificar."));
  Serial.println(F("Botón -> MODO MANTENER\n"));
}

// =============================================================================
// LOOP PRINCIPAL
// =============================================================================
void loop() {

  procesarComandoSerial();

  switch (estadoTest) {

    // -------------------------------------------------------------------------
    // TEST_HEADING: solo imprime el heading — sin mover motores.
    // Botón → pasa a MODO_MANTENER.
    // -------------------------------------------------------------------------
    case TEST_HEADING:
      if (millis() - ultimoPrint > 200) {
        float h = leerHeadingAbsoluto();
        Serial.print(F("  [HEADING] ")); Serial.print(h, 1); Serial.println(F("°"));
        digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
        ultimoPrint = millis();
      }
      if (botonPresionado()) {
        esperarSoltarBoton();
        contadorCiclos = 0;
        estadoTest = MODO_MANTENER;
        Serial.println(F("\n**** MODO MANTENER ACTIVADO ****"));
        Serial.print(F("Objetivo: ")); Serial.print(ANGULO_OBJETIVO, 1); Serial.println(F("°"));
        Serial.println(F("Botón -> iniciar AVANZAR DERECHO"));
        Serial.println(F("Comandos: 'r'=reset PID  'p'=pausa  'v'=valores  '+'/'-'=Kp\n"));
      }
      break;

    // -------------------------------------------------------------------------
    // MODO_MANTENER: robot quieto, corrigiendo heading.
    // Botón → pasa a MODO_AVANZAR.
    // -------------------------------------------------------------------------
    case MODO_MANTENER:
      if (!motorsPaused) {
        mantenerHeading(ANGULO_OBJETIVO);
      } else {
        if (millis() - ultimoPrint > 500) {
          Serial.print(F("  [PAUSADO] Actual: "));
          Serial.print(leerHeadingAbsoluto(), 1); Serial.println(F("°"));
          ultimoPrint = millis();
        }
      }
      if (botonPresionado()) {
        esperarSoltarBoton();
        pararMantenerHeading();
        tiempoInicioAvance = millis();
        estadoTest = MODO_AVANZAR;
        Serial.println(F("\n**** MODO AVANZAR DERECHO ACTIVADO ****"));
        Serial.print(F("Potencia: ")); Serial.print(POTENCIA_AVANCE);
        Serial.print(F("  Heading: ")); Serial.print(ANGULO_AVANCE, 1); Serial.println(F("°"));
        Serial.print(F("Duracion: ")); Serial.print(TIEMPO_AVANCE_MS); Serial.println(F(" ms"));
        Serial.println(F("Botón -> cancelar\n"));
      }
      break;

    // -------------------------------------------------------------------------
    // MODO_AVANZAR: avanza recto durante TIEMPO_AVANCE_MS ms.
    // Al terminar (o si apretás botón), frena y vuelve a TEST_HEADING.
    //
    // Acá está el ejemplo de uso de avanzarDerecho():
    //   - Se llama cada ciclo del loop, igual que mantenerHeading().
    //   - Al terminar, se llama pararMantenerHeading() para frenar y resetear el PID.
    // -------------------------------------------------------------------------
    case MODO_AVANZAR: {
      bool tiempoAgotado = (millis() - tiempoInicioAvance >= TIEMPO_AVANCE_MS);
      bool botonCancelado = botonPresionado();

      if (tiempoAgotado || botonCancelado) {
        pararMantenerHeading();
        estadoTest = TEST_HEADING;
        if (tiempoAgotado)
          Serial.println(F("\n**** AVANCE COMPLETADO — volviendo a TEST_HEADING ****\n"));
        else
          Serial.println(F("\n**** AVANCE CANCELADO por boton ****\n"));
        Serial.println(F(">>> Mostrando heading. Botón -> MODO MANTENER\n"));
        ultimoPrint = 0;
      } else {
        if (!motorsPaused) {
          avanzarDerecho(POTENCIA_AVANCE, ANGULO_AVANCE);   // ← así de simple
        } else {
          parar();
          if (millis() - ultimoPrint > 500) {
            Serial.print(F("  [PAUSADO] Actual: "));
            Serial.print(leerHeadingAbsoluto(), 1); Serial.println(F("°"));
            ultimoPrint = millis();
          }
        }
      }
      break;
    }
  }
}