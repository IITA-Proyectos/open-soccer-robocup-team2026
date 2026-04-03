// =============================================================================
// TEST: MANTENER ÁNGULO INICIAL CON PID (corrección omnidireccional)
// Archivo: staging/shared/test-mantener-angulo.ino
//
// PROPÓSITO:
//   Al encender, el robot captura su ángulo actual como "ángulo objetivo".
//   Cuando lo empujás o girás manualmente, los 3 motores se activan para
//   volver y mantener ese ángulo usando PID con el giróscopo BNO055.
//
//   Si el error es de orientación (rotación), los motores giran en sentido
//   opuesto para corregir el heading.
//   Si el error es de posición (te lo empujaron), los motores intentan
//   volver al heading original moviéndose.
//
// INSTRUCCIONES:
//   1. Subir al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. Poner el robot en la posición/orientación deseada
//   4. NO MOVER durante calibración (~5 segundos)
//   5. BOTÓN (pin 9) para avanzar:
//      - Presión 1: Test heading (verificar giróscopo)
//      - Presión 2: Activa modo MANTENER ÁNGULO
//   6. Comandos Serial (durante el modo activo):
//      'r' recalibra el ángulo objetivo al ángulo actual
//      'p' toggle: para los motores (pausa manual)
//      'v' muestra los valores actuales de PID y heading
//      '+' / '-' sube/baja Kp en 0.5
//
// BASADO EN:
//   - test-4-movimientos.ino (estructura de estados, PID, giróscopo)
//   - Mismos pines de motor, mismo BNO055, mismo estilo de código
//
// ESTRATEGIA DE CORRECCIÓN:
//   El error de heading se convierte en:
//     - Rotación diferencial de M1/M2 (para corregir giro)
//     - Activación de M3 proporcional al error (para resistir empuje lateral)
//   Esto permite que el robot se oponga tanto a giros como a desplazamientos.
//
// ROBOT 2 (delantero) - NO USA zirconLib
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 2 (delantero) — igual que test-4-movimientos
// =============================================================================
// ---- SELECCIÓN DE ROBOT ----
#define ROBOT1   // Arquero
//#define ROBOT2     // Delantero

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
const long BAUD_RATE = 19200;

// Umbral mínimo de error para activar los motores.
// Debajo de este valor el robot se considera "en posición" y no gasta batería.
const float UMBRAL_ERROR_GRADOS = 5;

// Velocidad mínima que se aplica a los motores cuando hay corrección pequeña.
// Evita que el robot "vibre" con pulsos muy chicos que no mueven nada.
const int VEL_MINIMA = 60;

// Velocidad máxima que puede alcanzar cualquier motor durante la corrección.
const int VEL_MAXIMA = 200;

// =============================================================================
// PARÁMETROS PID
// Mismos valores que el PID básico de test-4-movimientos como punto de partida.
// Ajustar con comandos Serial '+'/'-' durante el test.
// =============================================================================
float Kp = 0.20;    // Proporcional: cuánto reacciona al error actual
float Ki = 0.05;   // Integral: corrige errores sostenidos en el tiempo
float Kd = 1.8;    // Derivativo: frena la corrección cuando el error se achica rápido

const float MAX_CORRECCION  = 150.0;  // Límite de la salida PID total
const float INTEGRAL_MAX    = 40.0;   // Anti-windup: límite de la integral acumulada

// Factor de cuánto del error de heading se usa para activar M3 (lateral).
// 0.0 = M3 no participa; 1.0 = M3 tiene la misma corrección que M1/M2.
const float FACTOR_M3 = 0.4;

// Signos de dirección de M3 calibrados para este robot.
// Cambiar el signo si M3 empuja en la dirección equivocada.
const int DIR_M3_POSITIVO = 1;  // cuando el error es positivo (giró a la derecha)

// =============================================================================
// GIRÓSCOPO BNO055
// =============================================================================
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0.0;   // Ángulo capturado al inicio = nuestro "cero"
bool  bnoOK        = false;

// =============================================================================
// VARIABLES DEL PID
// =============================================================================
float errorAnterior      = 0.0;
float integral           = 0.0;
unsigned long tiempoAnteriorPID = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest {
  ESPERANDO_INICIO,     // Robot encendido, esperando primer botón
  TEST_HEADING,         // Usuario puede girar el robot para verificar el BNO055
  MODO_MANTENER         // Activo: el robot se corrige continuamente
};
EstadoTest estadoTest = ESPERANDO_INICIO;

// =============================================================================
// VARIABLES DE CONTROL
// =============================================================================
bool motorsPaused = false;             // Toggle de pausa manual por serial
bool botonAnterior = false;
unsigned long ultimoPrint = 0;         // Para no spamear el serial
unsigned long contadorCiclos = 0;      // Cuántos ciclos de corrección lleva

// =============================================================================
//  FUNCIONES DE MOTOR
//  Iguales a test-4-movimientos: vel en [0,255], dir en {-1, 0, +1}
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
// FUNCIÓN: leerHeading()
// Devuelve el ángulo actual relativo al offset capturado al inicio.
// Positivo = giró a la derecha respecto al ángulo inicial.
// Negativo = giró a la izquierda.
// Siempre en rango [-180, +180].
// =============================================================================
float leerHeading() {
  if (!bnoOK) return 0.0;

  sensors_event_t event;
  bno.getEvent(&event);

  float heading = event.orientation.x - headingOffset;

  // Normalizar al rango [-180, +180]
  if (heading >  180.0) heading -= 360.0;
  if (heading < -180.0) heading += 360.0;

  return heading;
}

// =============================================================================
// FUNCIÓN: resetPID()
// Llama esta función cada vez que el robot pasa de parado a activo,
// para que el PID arranque limpio sin memoria de errores anteriores.
// =============================================================================
void resetPID() {
  errorAnterior = 0.0;
  integral      = 0.0;
  tiempoAnteriorPID = millis();
}

// =============================================================================
// FUNCIÓN: calcularCorreccionPID()
// Toma el heading actual, calcula cuánto corregir y devuelve un valor en
// [-MAX_CORRECCION, +MAX_CORRECCION].
// Positivo = el robot giró a la derecha → motores deben girar a la izquierda.
// =============================================================================
float calcularCorreccionPID(float headingActual) {
  float error = 0.0 - headingActual;  // El objetivo siempre es 0° (heading inicial)

  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnteriorPID) / 1000.0;
  if (dt <= 0.001) dt = 0.01;  // Seguridad: evitar división por cero
  tiempoAnteriorPID = ahora;

  // --- Término proporcional ---
  float P = Kp * error;

  // --- Término integral con anti-windup ---
  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I = Ki * integral;

  // --- Término derivativo ---
  float derivada = (error - errorAnterior) / dt;
  float D = Kd * derivada;
  errorAnterior = error;

  // Suma y saturación
  float correccion = P + I + D;
  correccion = constrain(correccion, -MAX_CORRECCION, MAX_CORRECCION);

  return correccion;
}

// =============================================================================
// FUNCIÓN: aplicarCorrecion()
//
// Esta es la función principal de este programa.
//
// Estrategia:
//   Si el robot fue girado (error de heading):
//     → M1 y M2 giran en diferencial para volver a la orientación original.
//     → M3 se activa en proporción al error para resistir el giro.
//
//   Si el robot fue empujado lateralmente (el BNO055 detecta que perdió
//   el norte magnético aunque giro.x no cambie mucho):
//     → La corrección en M3 es la que mayor efecto tiene.
//
//   En la práctica: cualquier perturbación genera un error de heading,
//   y la corrección combinada M1+M2+M3 opone resistencia a la perturbación.
//
// El robot NO se mueve si el error está dentro del UMBRAL_ERROR_GRADOS.
// =============================================================================
void aplicarCorreccion() {
  float heading    = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  // Si el error es pequeño, apagar motores y no gastar batería
  if (abs(heading) < UMBRAL_ERROR_GRADOS) {
    parar();
    return;
  }

  // --- Corrección diferencial M1 / M2 (igual a moverAdelante/Atras en modo rotación) ---
  // Un error positivo (giró a la derecha) → correccion positiva
  //   → M1 gira menos / M2 gira más → el robot vuelve a la izquierda
  int velM1 = (int)( correccion);
  int velM2 = (int)(-correccion);

  // Aplicar velocidad mínima en la dirección correcta si hay corrección pequeña
  // (así el motor realmente se mueve y no se queda vibrando)
  if (abs(velM1) > 0 && abs(velM1) < VEL_MINIMA) velM1 = (velM1 > 0) ? VEL_MINIMA : -VEL_MINIMA;
  if (abs(velM2) > 0 && abs(velM2) < VEL_MINIMA) velM2 = (velM2 > 0) ? VEL_MINIMA : -VEL_MINIMA;

  velM1 = constrain(velM1, -VEL_MAXIMA, VEL_MAXIMA);
  velM2 = constrain(velM2, -VEL_MAXIMA, VEL_MAXIMA);

  // --- Corrección M3 (motor lateral) ---
  // M3 se opone al desplazamiento lateral. Su dirección depende del signo del error.
  // FACTOR_M3 controla cuánto participa respecto a M1/M2.
  int velM3 = (int)(correccion * FACTOR_M3);
  if (abs(velM3) > 0 && abs(velM3) < VEL_MINIMA) velM3 = (velM3 > 0) ? VEL_MINIMA : -VEL_MINIMA;
  velM3 = constrain(velM3, -VEL_MAXIMA, VEL_MAXIMA);

  // --- Aplicar a los motores ---
  int dirM1 = (velM1 > 0) ? 1 : ((velM1 < 0) ? -1 : 0);
  int dirM2 = (velM2 > 0) ? 1 : ((velM2 < 0) ? -1 : 0);
  int dirM3 = (velM3 > 0) ? DIR_M3_POSITIVO : ((velM3 < 0) ? -DIR_M3_POSITIVO : 0);

  motor1(abs(velM1), dirM1);
  motor2(abs(velM2), dirM2);
  motor3(abs(velM3), dirM3);

  contadorCiclos++;

  // --- Debug serial cada 200ms ---
  if (millis() - ultimoPrint > 200) {
    Serial.print(F("  [PID] H:"));
    if (heading >= 0) Serial.print('+');
    Serial.print(heading, 1);
    Serial.print(F("° | Corr:"));
    Serial.print(correccion, 1);
    Serial.print(F(" | M1:"));
    Serial.print(velM1);
    Serial.print(F(" M2:"));
    Serial.print(velM2);
    Serial.print(F(" M3:"));
    Serial.print(velM3);
    if (motorsPaused) Serial.print(F(" [PAUSADO]"));
    Serial.println();
    ultimoPrint = millis();
  }
}

// =============================================================================
// FUNCIÓN: recalibrarOffset()
// Captura el heading actual y lo establece como nuevo "cero".
// Útil si querés cambiar la orientación objetivo en caliente.
// =============================================================================
void recalibrarOffset() {
  sensors_event_t event;
  bno.getEvent(&event);
  headingOffset = event.orientation.x;
  resetPID();

  Serial.print(F("  >> RECALIBRADO. Nuevo offset = "));
  Serial.print(headingOffset, 1);
  Serial.println(F("°. Heading actual = 0.0°"));
}

// =============================================================================
// FUNCIÓN: procesarComandoSerial()
// =============================================================================
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {

      case 'r': // Recalibrar: el ángulo actual se convierte en el nuevo objetivo
        recalibrarOffset();
        break;

      case 'p': // Pausa/reanuda los motores (el PID sigue calculando)
        motorsPaused = !motorsPaused;
        if (motorsPaused) {
          parar();
          Serial.println(F("  >> MOTORES PAUSADOS (enviar 'p' para reanudar)"));
        } else {
          resetPID();
          Serial.println(F("  >> MOTORES REACTIVADOS"));
        }
        break;

      case 'v': // Ver valores actuales
        Serial.println(F("\n  === VALORES ACTUALES ==="));
        Serial.print(F("  Kp=")); Serial.print(Kp);
        Serial.print(F("  Ki=")); Serial.print(Ki);
        Serial.print(F("  Kd=")); Serial.println(Kd);
        Serial.print(F("  MAX_CORRECCION=")); Serial.println(MAX_CORRECCION);
        Serial.print(F("  UMBRAL_ERROR_GRADOS=")); Serial.println(UMBRAL_ERROR_GRADOS);
        Serial.print(F("  FACTOR_M3=")); Serial.println(FACTOR_M3);
        Serial.print(F("  headingOffset=")); Serial.print(headingOffset, 1); Serial.println(F("°"));
        Serial.print(F("  headingActual=")); Serial.print(leerHeading(), 1); Serial.println(F("°"));
        Serial.print(F("  ciclosCorrecion=")); Serial.println(contadorCiclos);
        Serial.println(F("  ========================\n"));
        break;

      case '+': // Sube Kp
        Kp += 0.5;
        Serial.print(F("  >> Kp = ")); Serial.println(Kp);
        break;

      case '-': // Baja Kp
        Kp -= 0.5;
        if (Kp < 0) Kp = 0;
        Serial.print(F("  >> Kp = ")); Serial.println(Kp);
        break;
    }
  }
}

// =============================================================================
// FUNCIÓN: inicializarGyro()
// Igual a test-4-movimientos: captura el heading inicial como offset = "cero".
// =============================================================================
bool inicializarGyro() {
  Serial.println(F("\n=== INICIALIZANDO GIROSCOPO BNO055 ==="));
  Serial.println(F(">>> PONELO EN LA POSICION FINAL Y NO LO MUEVAS <<<"));
  Serial.println();

  Serial.print(F("1. Detectando BNO055... "));
  unsigned long inicio = millis();
  while (!bno.begin()) {
    if (millis() - inicio > 3000) {
      Serial.println(F("ERROR!"));
      return false;
    }
    delay(100);
  }
  Serial.println(F("OK!"));

  Serial.print(F("2. Configurando cristal externo... "));
  bno.setExtCrystalUse(true);
  Serial.println(F("OK!"));

  Serial.print(F("3. Esperando estabilización (1 segundo)... "));
  delay(1000);
  Serial.println(F("OK!"));

  Serial.println(F("4. Calibrando giróscopo (NO MOVER!)..."));
  inicio = millis();
  bool gyroCalibrado = false;

  while (millis() - inicio < 5000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    Serial.print(F("   SYS=")); Serial.print(sys);
    Serial.print(F(" GYR=")); Serial.print(gyro);
    Serial.print(F(" ACC=")); Serial.print(accel);
    Serial.print(F(" MAG=")); Serial.println(mag);

    if (gyro >= 3) {
      Serial.print(F("   -> Giróscopo calibrado en "));
      Serial.print(millis() - inicio);
      Serial.println(F("ms!"));
      gyroCalibrado = true;
      break;
    }

    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
    delay(250);
  }

  if (!gyroCalibrado) {
    Serial.println(F("   ADVERTENCIA: Calibración incompleta, continuando..."));
  }

  // Captura el heading inicial: este es el "ángulo objetivo" para siempre
  Serial.println(F("5. Capturando angulo inicial (= objetivo del PID)..."));
  float suma = 0.0;
  for (int i = 0; i < 10; i++) {
    sensors_event_t event;
    bno.getEvent(&event);
    suma += event.orientation.x;
    Serial.print(F("   Lectura ")); Serial.print(i + 1);
    Serial.print(F("/10: ")); Serial.println(event.orientation.x, 1);
    delay(50);
  }
  headingOffset = suma / 10.0;

  Serial.print(F("   -> ANGULO INICIAL capturado: "));
  Serial.print(headingOffset, 1);
  Serial.println(F("° — el robot volverá siempre a este ángulo."));
  Serial.println(F("\n=== GIROSCOPO LISTO ==="));
  digitalWrite(LED_BUILTIN, HIGH);

  return true;
}

// =============================================================================
// FUNCIÓN: botonPresionado() — igual a test-4-movimientos
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
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n"));
  Serial.println(F("****************************************************"));
  Serial.println(F("*  TEST: MANTENER ANGULO INICIAL CON PID           *"));
  Serial.println(F("*  Robot: ROBOT 2 (delantero)                      *"));
  Serial.println(F("*  BNO055 + 3 motores omnidireccionales            *"));
  Serial.println(F("****************************************************"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(PIN_BOTON, INPUT);

  // Pines de motor
  pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);

  parar();

  // Inicializar giróscopo y capturar ángulo inicial
  bnoOK = inicializarGyro();

  if (!bnoOK) {
    Serial.println(F("\n*** ERROR FATAL: Giróscopo no detectado en I2C 0x28 ***"));
    // Parpadeo rápido infinito para indicar error
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW);  delay(100);
    }
  }

  Serial.println(F("\n========================================"));
  Serial.println(F("  Posicioná el robot y presioná BOTON"));
  Serial.println(F("  para el test de heading (verificación)."));
  Serial.println(F("========================================\n"));
}

// =============================================================================
// LOOP PRINCIPAL
// =============================================================================
void loop() {

  procesarComandoSerial();

  switch (estadoTest) {

    // =========================================================================
    // ESTADO 1: Esperando primer botón para entrar al test de heading
    // =========================================================================
    case ESPERANDO_INICIO:
      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = TEST_HEADING;
        Serial.println(F("\n>>> TEST HEADING <<<"));
        Serial.println(F("Girá el robot manualmente. El heading debe cambiar."));
        Serial.println(F("Al soltarlo, debería volver a 0° (si ya funciona bien)."));
        Serial.println(F("Presioná BOTON para activar MODO MANTENER.\n"));
      }
      break;

    // =========================================================================
    // ESTADO 2: Test de heading — el usuario gira el robot para verificar
    //           que el BNO055 lee bien antes de activar los motores.
    // =========================================================================
    case TEST_HEADING:
    {
      if (millis() - ultimoPrint > 200) {
        float h = leerHeading();
        Serial.print(F("  [HEADING] "));
        if (h >= 0) Serial.print('+');
        Serial.print(h, 1);
        Serial.println(F("°"));

        // LED parpadea si el ángulo es grande (warning visual)
        if (abs(h) > 20.0) {
          digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
        } else {
          digitalWrite(LED_BUILTIN, HIGH);
        }

        ultimoPrint = millis();
      }

      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = MODO_MANTENER;
        resetPID();
        contadorCiclos = 0;
        Serial.println(F("\n**** MODO MANTENER ANGULO ACTIVADO ****"));
        Serial.print(F("Angulo objetivo: "));
        Serial.print(headingOffset, 1);
        Serial.println(F("° (relativo = 0.0°)"));
        Serial.println(F("Comandos: 'r'=recalibrar  'p'=pausa  'v'=valores  '+'/'-'=ajustar Kp\n"));
      }
    }
    break;

    // =========================================================================
    // ESTADO 3: MODO MANTENER — el robot se corrige continuamente.
    //           Si lo empujás o girás, vuelve al ángulo inicial.
    // =========================================================================
    case MODO_MANTENER:
      if (!motorsPaused) {
        aplicarCorreccion();
      } else {
        // Aunque esté pausado, seguir leyendo el heading para el debug
        if (millis() - ultimoPrint > 500) {
          float h = leerHeading();
          Serial.print(F("  [PAUSADO] Heading = "));
          if (h >= 0) Serial.print('+');
          Serial.print(h, 1);
          Serial.println(F("°"));
          ultimoPrint = millis();
        }
      }
      break;
  }
}
