// ============================================================================
// ⚠️ STAGING CONGELADO (2026-06-03) — NO subir más material a software/staging/.
// Antes de tocar o agregar algo acá, LEÉ:
//   software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md
// Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
//   software/teensy/Soccer 2026/src/  (ver el "mapa de reúso" del documento).
// ============================================================================

// =============================================================================
// TEST: 4 movimientos combinados (adelante/atrás + derecha/izquierda)
// Archivo: staging/shared/test-4-movimientos.ino
//
// PROPÓSITO:
//   Combina el test de movimiento básico (adelante/atrás) con el test de
//   movimiento lateral (derecha/izquierda) en un solo programa.
//   Loop infinito: ADELANTE 3s → ATRAS 3s → DERECHA 3s → IZQUIERDA 3s → repetir
//
// INSTRUCCIONES:
//   1. Subir al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER durante calibración (~5 segundos)
//   4. BOTON (pin 9) para avanzar:
//      - Presión 1: Test heading (girar manualmente)
//      - Presión 2: Inicia loop de 4 movimientos
//   5. Comandos Serial (durante movimiento lateral):
//      '+' sube anticipación 10ms | '-' baja 10ms
//      'c' toggle auto-calibración | 'v' ver valores
//
// BASADO EN:
//   - test-gyro-movimiento-basico.ino (adelante/atrás, PID Kp=3 Ki=0.08 Kd=0.8)
//   - test-motores-lateral-simple.ino v4 (derecha/izquierda, PID Kp=3 Ki=0.05 Kd=0.5)
//
// ROBOT 2 (delantero) - NO USA zirconLib
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 2 (delantero)
// =============================================================================
#define INA1 8
#define INB1 7
#define PWM1 6

#define INA2 11
#define INB2 12
#define PWM2 4

#define INA3 2
#define INB3 5
#define PWM3 3

#define PIN_BOTON 9

// =============================================================================
// CONFIGURACIÓN DE MOVIMIENTO
// =============================================================================
const long BAUD_RATE = 19200;
const unsigned long TIEMPO_MOVIMIENTO = 3000;       // 3 segundos cada dirección
const unsigned long PAUSA_ENTRE_MOVIMIENTOS = 500;   // 0.5 segundos de pausa

// --- Velocidad adelante/atrás (del programa básico) ---
const int VELOCIDAD_BASE = 150;

// --- Velocidades laterales (del programa lateral) ---
int VEL_M1 = 55;
int VEL_M2 = 55;
int VEL_M3 = 100;

// --- Direcciones laterales (calibradas por María) ---
int DIR_M1 = -1;
int DIR_M2 = 1;
int DIR_M3 = 1;

// --- Signos de rotación para corrección lateral ---
int ROT_M1 = 1;
int ROT_M2 = -1;
int ROT_M3 = 1;

// --- Factor de rotación lateral ---
float FACTOR_ROTACION = 0.5;

// =============================================================================
// PID — Dos juegos de parámetros (uno por tipo de movimiento)
// =============================================================================

// PID para adelante/atrás (del programa básico, tal cual)
const float Kp_basico = 3.0;
const float Ki_basico = 0.08;
const float Kd_basico = 0.8;
const float MAX_CORRECCION_basico = 80;
const float INTEGRAL_MAX_basico = 50;

// PID para lateral (del programa lateral, tal cual)
const float Kp_lateral = 3.0;
const float Ki_lateral = 0.05;
const float Kd_lateral = 0.5;
const float MAX_CORRECCION_lateral = 50;
const float INTEGRAL_MAX_lateral = 40;

// PID activo (se cambia según el modo)
float Kp = 3.0;
float Ki = 0.08;
float Kd = 0.8;
float MAX_CORRECCION_actual = 80;
float INTEGRAL_MAX_actual = 50;

// =============================================================================
// COMPENSACIÓN DE FRENADO M3 (del programa lateral v4)
// =============================================================================
float BASE_ANTICIPACION_MS = 60.0;
bool autoCalActivada = true;
float UMBRAL_DRIFT = 1.5;
float PASO_AUTOCAL = 5.0;
float MAX_ANTICIPACION = 300;
float MIN_ANTICIPACION = 0;

// =============================================================================
// GIRÓSCOPO
// =============================================================================
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

// =============================================================================
// VARIABLES DEL PID
// =============================================================================
float errorAnterior = 0;
float integral = 0;
unsigned long tiempoAnteriorPID = 0;

// =============================================================================
// VARIABLES DE DEBUG (para lateral)
// =============================================================================
float ultimoHeading = 0;
float ultimaCorreccion = 0;
float debugM1 = 0, debugM2 = 0, debugM3 = 0;

// =============================================================================
// VARIABLES DE FRENADO M3
// =============================================================================
bool m3Frenado = false;
float headingAlFrenar = 0;
bool headingAlFrenarCapturado = false;
float anticipacionReal = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest {
  ESPERANDO_INICIO,
  TEST_HEADING,
  ESPERANDO_LOOP,
  LOOP_ADELANTE,
  LOOP_PAUSA_1,
  LOOP_ATRAS,
  LOOP_PAUSA_2,
  LOOP_DERECHA,
  LOOP_PAUSA_3,
  LOOP_IZQUIERDA,
  LOOP_PAUSA_4
};
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioTest = 0;
int cicloActual = 0;

// =============================================================================
// CONTROL DEL BOTÓN
// =============================================================================
bool botonAnterior = false;

// =============================================================================
// FUNCIONES DE CAMBIO DE MODO PID
// =============================================================================
void setPIDBasico() {
  Kp = Kp_basico;
  Ki = Ki_basico;
  Kd = Kd_basico;
  MAX_CORRECCION_actual = MAX_CORRECCION_basico;
  INTEGRAL_MAX_actual = INTEGRAL_MAX_basico;
}

void setPIDLateral() {
  Kp = Kp_lateral;
  Ki = Ki_lateral;
  Kd = Kd_lateral;
  MAX_CORRECCION_actual = MAX_CORRECCION_lateral;
  INTEGRAL_MAX_actual = INTEGRAL_MAX_lateral;
}

// =============================================================================
// FUNCIÓN: calcularAnticipacionReal() (del lateral v4)
// =============================================================================
float calcularAnticipacionReal() {
  return BASE_ANTICIPACION_MS * (float)VEL_M3 / 100.0;
}

// =============================================================================
// FUNCIÓN: procesarComandoSerial() (del lateral v4)
// =============================================================================
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '+':
      case 'a':
        BASE_ANTICIPACION_MS += 10;
        if (BASE_ANTICIPACION_MS > MAX_ANTICIPACION) BASE_ANTICIPACION_MS = MAX_ANTICIPACION;
        anticipacionReal = calcularAnticipacionReal();
        Serial.print("  >> BASE_ANTICIPACION = ");
        Serial.print(BASE_ANTICIPACION_MS, 0);
        Serial.print("ms -> real = ");
        Serial.print(anticipacionReal, 0);
        Serial.println("ms");
        break;
      case '-':
      case 'z':
        BASE_ANTICIPACION_MS -= 10;
        if (BASE_ANTICIPACION_MS < MIN_ANTICIPACION) BASE_ANTICIPACION_MS = MIN_ANTICIPACION;
        anticipacionReal = calcularAnticipacionReal();
        Serial.print("  >> BASE_ANTICIPACION = ");
        Serial.print(BASE_ANTICIPACION_MS, 0);
        Serial.print("ms -> real = ");
        Serial.print(anticipacionReal, 0);
        Serial.println("ms");
        break;
      case 'c':
        autoCalActivada = !autoCalActivada;
        Serial.print("  >> Auto-calibración: ");
        Serial.println(autoCalActivada ? "ACTIVADA" : "DESACTIVADA");
        break;
      case 'v':
        Serial.println("\n  === VALORES ACTUALES ===");
        Serial.print("  VEL_M3 = "); Serial.println(VEL_M3);
        Serial.print("  BASE_ANTICIPACION_MS = "); Serial.print(BASE_ANTICIPACION_MS, 0); Serial.println("ms");
        Serial.print("  anticipacionReal = "); Serial.print(anticipacionReal, 0); Serial.println("ms");
        Serial.print("  autoCalActivada = "); Serial.println(autoCalActivada ? "SI" : "NO");
        Serial.print("  PID modo = "); Serial.println((Ki == Ki_basico) ? "BASICO" : "LATERAL");
        Serial.println("  =======================\n");
        break;
    }
  }
}

// =============================================================================
// FUNCIÓN: inicializarGyro() (del programa básico)
// =============================================================================
bool inicializarGyro() {
  Serial.println("\n=== INICIALIZANDO GIROSCOPO BNO055 ===");
  Serial.println("NO MOVER EL ROBOT!");
  Serial.println();

  Serial.print("1. Detectando BNO055... ");
  unsigned long inicio = millis();

  while (!bno.begin()) {
    if (millis() - inicio > 3000) {
      Serial.println("ERROR!");
      return false;
    }
    delay(100);
  }
  Serial.println("OK!");

  Serial.print("2. Configurando cristal externo... ");
  bno.setExtCrystalUse(true);
  Serial.println("OK!");

  Serial.print("3. Esperando estabilización (1 segundo)... ");
  delay(1000);
  Serial.println("OK!");

  Serial.println("4. Calibrando giróscopo (NO MOVER!)...");
  inicio = millis();
  bool gyroCalibrado = false;

  while (millis() - inicio < 5000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    Serial.print("   Calibración: SYS=");
    Serial.print(sys);
    Serial.print(" GYR=");
    Serial.print(gyro);
    Serial.print(" ACC=");
    Serial.print(accel);
    Serial.print(" MAG=");
    Serial.println(mag);

    if (gyro >= 3) {
      Serial.print("   -> Giróscopo calibrado en ");
      Serial.print(millis() - inicio);
      Serial.println("ms!");
      gyroCalibrado = true;
      break;
    }

    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
    delay(250);
  }

  if (!gyroCalibrado) {
    Serial.println("   ADVERTENCIA: Calibración incompleta, continuando...");
  }

  Serial.println("5. Estableciendo posición cero...");
  float suma = 0;

  for (int i = 0; i < 10; i++) {
    sensors_event_t event;
    bno.getEvent(&event);
    suma += event.orientation.x;
    Serial.print("   Lectura ");
    Serial.print(i + 1);
    Serial.print("/10: ");
    Serial.println(event.orientation.x, 1);
    delay(50);
  }

  headingOffset = suma / 10.0;
  Serial.print("   -> Offset establecido: ");
  Serial.println(headingOffset, 1);

  Serial.println("\n=== GIROSCOPO LISTO ===");
  digitalWrite(LED_BUILTIN, HIGH);

  return true;
}

// =============================================================================
// FUNCIÓN: leerHeading()
// =============================================================================
float leerHeading() {
  if (!bnoOK) return 0;

  sensors_event_t event;
  bno.getEvent(&event);
  float heading = event.orientation.x - headingOffset;

  if (heading > 180) heading -= 360;
  if (heading < -180) heading += 360;

  return heading;
}

// =============================================================================
// FUNCIÓN: resetPID()
// =============================================================================
void resetPID() {
  errorAnterior = 0;
  integral = 0;
  tiempoAnteriorPID = millis();
}

// =============================================================================
// FUNCIÓN: calcularCorreccionPID()
// Usa los parámetros PID activos (básico o lateral según el modo)
// =============================================================================
float calcularCorreccionPID(float headingActual) {
  float error = 0 - headingActual;

  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnteriorPID) / 1000.0;
  if (dt <= 0) dt = 0.01;
  tiempoAnteriorPID = ahora;

  float P = Kp * error;

  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_MAX_actual, INTEGRAL_MAX_actual);
  float I = Ki * integral;

  float derivada = (error - errorAnterior) / dt;
  float D = Kd * derivada;
  errorAnterior = error;

  float correccion = P + I + D;
  correccion = constrain(correccion, -MAX_CORRECCION_actual, MAX_CORRECCION_actual);

  return correccion;
}

// =============================================================================
// FUNCIONES DE MOTORES (del programa lateral)
// =============================================================================
void motor1(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM1, vel);
  if (dir > 0) {
    digitalWrite(INA1, 1);
    digitalWrite(INB1, 0);
  } else if (dir < 0) {
    digitalWrite(INA1, 0);
    digitalWrite(INB1, 1);
  } else {
    digitalWrite(INA1, 0);
    digitalWrite(INB1, 0);
  }
}

void motor2(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM2, vel);
  if (dir > 0) {
    digitalWrite(INA2, 0);
    digitalWrite(INB2, 1);
  } else if (dir < 0) {
    digitalWrite(INA2, 1);
    digitalWrite(INB2, 0);
  } else {
    digitalWrite(INA2, 0);
    digitalWrite(INB2, 0);
  }
}

void motor3(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM3, vel);
  if (dir > 0) {
    digitalWrite(INA3, 1);
    digitalWrite(INB3, 0);
  } else if (dir < 0) {
    digitalWrite(INA3, 0);
    digitalWrite(INB3, 1);
  } else {
    digitalWrite(INA3, 0);
    digitalWrite(INB3, 0);
  }
}

void parar() {
  motor1(0, 0);
  motor2(0, 0);
  motor3(0, 0);
}

// =============================================================================
// FUNCIÓN: moverAdelante() (del programa básico, tal cual)
// =============================================================================
void moverAdelante(int velocidadBase) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  int velM1 = velocidadBase + (int)correccion;
  int velM2 = velocidadBase - (int)correccion;

  velM1 = constrain(velM1, 0, 255);
  velM2 = constrain(velM2, 0, 255);

  analogWrite(PWM1, velM1);
  digitalWrite(INA1, 1);
  digitalWrite(INB1, 0);

  analogWrite(PWM2, velM2);
  digitalWrite(INA2, 0);
  digitalWrite(INB2, 1);

  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
}

// =============================================================================
// FUNCIÓN: moverAtras() (del programa básico, tal cual)
// =============================================================================
void moverAtras(int velocidadBase) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  int velM1 = velocidadBase - (int)correccion;  // Invertido
  int velM2 = velocidadBase + (int)correccion;  // Invertido

  velM1 = constrain(velM1, 0, 255);
  velM2 = constrain(velM2, 0, 255);

  analogWrite(PWM1, velM1);
  digitalWrite(INA1, 0);
  digitalWrite(INB1, 1);

  analogWrite(PWM2, velM2);
  digitalWrite(INA2, 1);
  digitalWrite(INB2, 0);

  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
}

// =============================================================================
// FUNCIÓN: moverLateral() (del programa lateral v4, tal cual)
// =============================================================================
void moverLateral(int direccion, bool frenarM3) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  ultimoHeading = heading;
  ultimaCorreccion = correccion;

  // 1. Componente LATERAL
  float lat_M1 = (float)(DIR_M1 * VEL_M1 * direccion);
  float lat_M2 = (float)(DIR_M2 * VEL_M2 * direccion);
  float lat_M3 = (float)(DIR_M3 * VEL_M3 * direccion);

  // 2. Componente ROTACIÓN del PID
  float rot_M1 = ROT_M1 * correccion * FACTOR_ROTACION;
  float rot_M2 = ROT_M2 * correccion * FACTOR_ROTACION;
  float rot_M3 = ROT_M3 * correccion * FACTOR_ROTACION;

  // 3. Sumar lateral + rotación
  float total_M1 = lat_M1 + rot_M1;
  float total_M2 = lat_M2 + rot_M2;
  float total_M3 = lat_M3 + rot_M3;

  // v4: Si estamos en fase de pre-frenado, M3 se apaga
  if (frenarM3) {
    total_M3 = 0;
  }

  // 4. Saturación PROPORCIONAL
  float maxM = max(abs(total_M1), max(abs(total_M2), abs(total_M3)));
  if (maxM > 255) {
    float escala = 255.0 / maxM;
    total_M1 *= escala;
    total_M2 *= escala;
    total_M3 *= escala;
  }

  debugM1 = total_M1;
  debugM2 = total_M2;
  debugM3 = total_M3;

  // 5. Aplicar a motores
  int d1 = (total_M1 > 0) ? 1 : ((total_M1 < 0) ? -1 : 0);
  int d2 = (total_M2 > 0) ? 1 : ((total_M2 < 0) ? -1 : 0);
  int d3 = (total_M3 > 0) ? 1 : ((total_M3 < 0) ? -1 : 0);

  motor1((int)abs(total_M1), d1);
  motor2((int)abs(total_M2), d2);
  motor3((int)abs(total_M3), d3);
}

// =============================================================================
// FUNCIÓN: autoCalibrarFrenado() (del programa lateral v4, tal cual)
// =============================================================================
void autoCalibrarFrenado(bool ibaDerecha) {
  if (!autoCalActivada || !bnoOK) return;

  delay(100);

  float headingFinal = leerHeading();
  float drift = headingFinal - headingAlFrenar;

  Serial.print("  [AUTOCAL] H_freno=");
  Serial.print(headingAlFrenar, 1);
  Serial.print(" H_final=");
  Serial.print(headingFinal, 1);
  Serial.print(" drift=");
  Serial.print(drift, 1);

  if (abs(drift) < UMBRAL_DRIFT) {
    Serial.println(" -> OK");
    return;
  }

  float ajuste = PASO_AUTOCAL;
  bool m3FrenoTarde;
  if (ibaDerecha) {
    m3FrenoTarde = (drift > 0);
  } else {
    m3FrenoTarde = (drift < 0);
  }

  if (m3FrenoTarde) {
    BASE_ANTICIPACION_MS += ajuste;
    if (BASE_ANTICIPACION_MS > MAX_ANTICIPACION) BASE_ANTICIPACION_MS = MAX_ANTICIPACION;
    Serial.print(" -> SUBIENDO a ");
  } else {
    BASE_ANTICIPACION_MS -= ajuste;
    if (BASE_ANTICIPACION_MS < MIN_ANTICIPACION) BASE_ANTICIPACION_MS = MIN_ANTICIPACION;
    Serial.print(" -> BAJANDO a ");
  }

  anticipacionReal = calcularAnticipacionReal();
  Serial.print(BASE_ANTICIPACION_MS, 0);
  Serial.print("ms (real=");
  Serial.print(anticipacionReal, 0);
  Serial.println("ms)");
}

// =============================================================================
// FUNCIÓN: botonPresionado()
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

// =============================================================================
// FUNCIÓN: esperarSoltarBoton()
// =============================================================================
void esperarSoltarBoton() {
  while (digitalRead(PIN_BOTON) == HIGH) {
    delay(10);
  }
  delay(50);
  botonAnterior = false;
}

// =============================================================================
// FUNCIÓN: imprimirMenu()
// =============================================================================
void imprimirMenu() {
  Serial.println("\n========================================");

  switch (estadoTest) {
    case ESPERANDO_INICIO:
      Serial.println("PRESIONAR BOTON para TEST HEADING");
      break;

    case TEST_HEADING:
      Serial.println("TEST HEADING: Girar robot manualmente");
      Serial.println("PRESIONAR BOTON para iniciar LOOP");
      break;

    case ESPERANDO_LOOP:
      Serial.println("PRESIONAR BOTON para iniciar LOOP");
      Serial.println("(Adelante 3s -> Atras 3s -> Derecha 3s -> Izquierda 3s)");
      break;

    default:
      break;
  }

  Serial.println("========================================\n");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);

  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST: 4 MOVIMIENTOS CON PID                    *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Adelante/Atras + Derecha/Izquierda              *");
  Serial.println("*  3 segundos cada dirección                       *");
  Serial.println("****************************************************");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(PIN_BOTON, INPUT);

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

  anticipacionReal = calcularAnticipacionReal();

  bnoOK = inicializarGyro();

  if (!bnoOK) {
    Serial.println("\n*** ERROR: Giróscopo no inicializado ***");
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }

  imprimirMenu();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  static unsigned long ultimoHeadingPrint = 0;

  procesarComandoSerial();

  switch (estadoTest) {

    // ===== ESPERANDO INICIO =====
    case ESPERANDO_INICIO:
      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = TEST_HEADING;
        Serial.println("\n>>> TEST HEADING <<<");
        imprimirMenu();
      }
      break;

    // ===== TEST HEADING (manual) =====
    case TEST_HEADING:
      if (millis() - ultimoHeadingPrint > 200) {
        float heading = leerHeading();
        Serial.print("Heading: ");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.println("°");

        if (abs(heading) > 30) {
          digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
        } else {
          digitalWrite(LED_BUILTIN, HIGH);
        }

        ultimoHeadingPrint = millis();
      }

      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = ESPERANDO_LOOP;
        imprimirMenu();
      }
      break;

    // ===== ESPERANDO PARA INICIAR LOOP =====
    case ESPERANDO_LOOP:
      if (botonPresionado()) {
        esperarSoltarBoton();
        cicloActual = 1;
        Serial.println("\n***** INICIANDO LOOP DE 4 MOVIMIENTOS *****");
        Serial.println(">>> CICLO 1: ADELANTE (3 seg) <<<");
        setPIDBasico();
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ADELANTE;
      }
      break;

    // =========================================================================
    // ADELANTE (del programa básico)
    // =========================================================================
    case LOOP_ADELANTE:
      moverAdelante(VELOCIDAD_BASE);
      digitalWrite(LED_BUILTIN, HIGH);

      if (millis() - ultimoHeadingPrint > 200) {
        float heading = leerHeading();
        Serial.print("  [ADELANTE] H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | t=");
        Serial.print((millis() - tiempoInicioTest) / 1000.0, 1);
        Serial.println("s");
        ultimoHeadingPrint = millis();
      }

      if (millis() - tiempoInicioTest >= TIEMPO_MOVIMIENTO) {
        parar();
        Serial.println("  -> Pausa...");
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_1;
      }
      break;

    // ===== PAUSA 1 =====
    case LOOP_PAUSA_1:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": ATRAS (3 seg) <<<");
        setPIDBasico();
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ATRAS;
      }
      break;

    // =========================================================================
    // ATRÁS (del programa básico)
    // =========================================================================
    case LOOP_ATRAS:
      moverAtras(VELOCIDAD_BASE);
      digitalWrite(LED_BUILTIN, LOW);

      if (millis() - ultimoHeadingPrint > 200) {
        float heading = leerHeading();
        Serial.print("  [ATRAS]    H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | t=");
        Serial.print((millis() - tiempoInicioTest) / 1000.0, 1);
        Serial.println("s");
        ultimoHeadingPrint = millis();
      }

      if (millis() - tiempoInicioTest >= TIEMPO_MOVIMIENTO) {
        parar();
        Serial.println("  -> Pausa...");
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_2;
      }
      break;

    // ===== PAUSA 2 =====
    case LOOP_PAUSA_2:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": DERECHA (3 seg) <<<");
        setPIDLateral();
        resetPID();
        m3Frenado = false;
        headingAlFrenarCapturado = false;
        anticipacionReal = calcularAnticipacionReal();
        tiempoInicioTest = millis();
        estadoTest = LOOP_DERECHA;
      }
      break;

    // =========================================================================
    // DERECHA (del programa lateral v4)
    // =========================================================================
    case LOOP_DERECHA:
    {
      unsigned long tiempoTranscurrido = millis() - tiempoInicioTest;

      bool debeFrenarM3 = (tiempoTranscurrido >= TIEMPO_MOVIMIENTO - (unsigned long)anticipacionReal);

      if (debeFrenarM3 && !m3Frenado) {
        m3Frenado = true;
        Serial.print("  -> M3 frena (anticipación=");
        Serial.print(anticipacionReal, 0);
        Serial.println("ms)");
      }

      moverLateral(1, debeFrenarM3);
      digitalWrite(LED_BUILTIN, HIGH);

      if (millis() - ultimoHeadingPrint > 200) {
        Serial.print("  [DER] H:");
        if (ultimoHeading >= 0) Serial.print("+");
        Serial.print(ultimoHeading, 1);
        Serial.print(" C:");
        Serial.print(ultimaCorreccion, 1);
        Serial.print(" M1:");
        Serial.print((int)debugM1);
        Serial.print(" M2:");
        Serial.print((int)debugM2);
        Serial.print(" M3:");
        Serial.print((int)debugM3);
        if (debeFrenarM3) Serial.print(" [FRENO]");
        Serial.print(" | t=");
        Serial.print(tiempoTranscurrido / 1000.0, 1);
        Serial.println("s");
        ultimoHeadingPrint = millis();
      }

      if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
        if (!headingAlFrenarCapturado) {
          headingAlFrenar = leerHeading();
          headingAlFrenarCapturado = true;
        }
        parar();
        Serial.println("  -> Pausa...");
        autoCalibrarFrenado(true);
        m3Frenado = false;
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_3;
      }
    }
      break;

    // ===== PAUSA 3 =====
    case LOOP_PAUSA_3:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": IZQUIERDA (3 seg) <<<");
        setPIDLateral();
        resetPID();
        m3Frenado = false;
        headingAlFrenarCapturado = false;
        anticipacionReal = calcularAnticipacionReal();
        Serial.print("  (anticipación M3 = ");
        Serial.print(anticipacionReal, 0);
        Serial.println("ms)");
        tiempoInicioTest = millis();
        estadoTest = LOOP_IZQUIERDA;
      }
      break;

    // =========================================================================
    // IZQUIERDA (del programa lateral v4)
    // =========================================================================
    case LOOP_IZQUIERDA:
    {
      unsigned long tiempoTranscurrido = millis() - tiempoInicioTest;

      bool debeFrenarM3 = (tiempoTranscurrido >= TIEMPO_MOVIMIENTO - (unsigned long)anticipacionReal);

      if (debeFrenarM3 && !m3Frenado) {
        m3Frenado = true;
        Serial.print("  -> M3 frena (anticipación=");
        Serial.print(anticipacionReal, 0);
        Serial.println("ms)");
      }

      moverLateral(-1, debeFrenarM3);
      digitalWrite(LED_BUILTIN, LOW);

      if (millis() - ultimoHeadingPrint > 200) {
        Serial.print("  [IZQ] H:");
        if (ultimoHeading >= 0) Serial.print("+");
        Serial.print(ultimoHeading, 1);
        Serial.print(" C:");
        Serial.print(ultimaCorreccion, 1);
        Serial.print(" M1:");
        Serial.print((int)debugM1);
        Serial.print(" M2:");
        Serial.print((int)debugM2);
        Serial.print(" M3:");
        Serial.print((int)debugM3);
        if (debeFrenarM3) Serial.print(" [FRENO]");
        Serial.print(" | t=");
        Serial.print(tiempoTranscurrido / 1000.0, 1);
        Serial.println("s");
        ultimoHeadingPrint = millis();
      }

      if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
        if (!headingAlFrenarCapturado) {
          headingAlFrenar = leerHeading();
          headingAlFrenarCapturado = true;
        }
        parar();
        Serial.println("  -> Pausa...");
        autoCalibrarFrenado(false);
        m3Frenado = false;
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_4;
      }
    }
      break;

    // ===== PAUSA 4 (volver a empezar) =====
    case LOOP_PAUSA_4:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        cicloActual++;
        Serial.println("\n>>> CICLO " + String(cicloActual) + ": ADELANTE (3 seg) <<<");
        setPIDBasico();
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ADELANTE;
      }
      break;
  }
}
