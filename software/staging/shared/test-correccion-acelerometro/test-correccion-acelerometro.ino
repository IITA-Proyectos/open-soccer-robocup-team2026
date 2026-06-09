// =============================================================================
// TEST: Corrección heading + desplazamiento (v7)
// Archivo: staging/shared/test-correccion-acelerometro/test-correccion-acelerometro.ino
//
// v7 — HEADING SIEMPRE ACTIVO:
//   El robot mantiene su orientación CON EL GIROSCOPIO en TODO momento.
//   Si lo girás sobre su eje, vuelve al frente original.
//   ADEMÁS, si lo empujás (desplazamiento), corrige con el acelerómetro.
//
//   Las dos correcciones funcionan simultáneamente:
//   - Heading PID: corre SIEMPRE, en todos los estados
//   - Corrección posición: solo cuando detecta un empujón
//
// PINES Y MOTORES: Idénticos a test-4-movimientos.ino
// ROBOT 2 (delantero)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 2 (idénticos a test-4-movimientos)
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
// ******************** PARÁMETROS AJUSTABLES ********************
// =============================================================================

// --- Heading PID (para mantener frente) ---
// Estos valores se usan SIEMPRE, en todos los estados.
float Kp_heading = 3.0;    // Más alto que v6 porque ahora QUEREMOS que corrija heading
float Ki_heading = 0.05;
float Kd_heading = 0.5;
const float MAX_CORRECCION_HEADING = 80.0;
const float INTEGRAL_MAX_HEADING = 50.0;

// L_ROTACION: peso de la rotación en la cinemática.
//   Para heading solo (sin traslación): 0.5 funciona bien.
//   Durante corrección de posición se usa un valor menor para
//   que el heading no domine sobre la traslación.
const float L_ROTACION_HEADING = 0.5;     // Para heading solo
const float L_ROTACION_CORRECCION = 0.2;  // Para heading + traslación

// --- Detección de empujón ---
float UMBRAL_ACCEL_INICIO = 40.0;
float UMBRAL_ACCEL_FIN = 18.0;
const unsigned long TIEMPO_QUIETO_PARA_FIN = 200;
float UMBRAL_ACCEL_INTEGRAR = 8.0;

// --- Corrección de posición ---
float Kp_pos = 6.0;
float TIEMPO_CORRECCION_POR_CM = 150.0;
const unsigned long TIEMPO_CORRECCION_MIN = 150;
const unsigned long TIEMPO_CORRECCION_MAX = 2000;
const unsigned long TIEMPO_PAUSA_POST = 500;
const unsigned long TIEMPO_ENFRIAMIENTO = 500;

// --- Decay basado en TIEMPO ---
float DECAY_VEL_POR_SEG = 0.4;
float DECAY_POS_POR_SEG = 0.85;

// --- Motor ---
const float VEL_MAX_CORRECCION = 120.0;
int PWM_MINIMO_MOTOR = 35;
const int MOTOR_MAX = 200;

// =============================================================================
// VARIABLES
// =============================================================================

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

// PID de heading (con integral)
float headingErrorAnterior = 0;
float headingIntegral = 0;
unsigned long pidTiempoAnterior = 0;

// Acelerómetro
float vel_x = 0, vel_y = 0;
float pos_x = 0, pos_y = 0;
unsigned long accelTiempoAnterior = 0;

// Empujón capturado
float empujon_x = 0, empujon_y = 0;
float empujon_magnitud = 0;

// Sub-estados
enum SubEstado { ESCUCHANDO, EMPUJANDO, CORRIGIENDO, PAUSA_POST, ENFRIAMIENTO };
SubEstado subEstado = ESCUCHANDO;
unsigned long tiempoInicioFase = 0;
unsigned long duracionCorreccion = 0;
unsigned long tiempoUltimaAccelAlta = 0;

enum Estado { ESPERANDO_INICIO, MOSTRANDO_ACCEL, ACTIVO };
Estado estado = ESPERANDO_INICIO;
bool botonAnterior = false;

// =============================================================================
// FUNCIONES DE MOTOR — IDÉNTICAS a test-4-movimientos.ino
// =============================================================================
void motor1(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM1, vel);
  if (dir > 0)      { digitalWrite(INA1, 1); digitalWrite(INB1, 0); }
  else if (dir < 0) { digitalWrite(INA1, 0); digitalWrite(INB1, 1); }
  else               { digitalWrite(INA1, 0); digitalWrite(INB1, 0); }
}

void motor2(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM2, vel);
  if (dir > 0)      { digitalWrite(INA2, 0); digitalWrite(INB2, 1); } // INVERTIDO
  else if (dir < 0) { digitalWrite(INA2, 1); digitalWrite(INB2, 0); }
  else               { digitalWrite(INA2, 0); digitalWrite(INB2, 0); }
}

void motor3(int vel, int dir) {
  vel = constrain(abs(vel), 0, 255);
  analogWrite(PWM3, vel);
  if (dir > 0)      { digitalWrite(INA3, 1); digitalWrite(INB3, 0); }
  else if (dir < 0) { digitalWrite(INA3, 0); digitalWrite(INB3, 1); }
  else               { digitalWrite(INA3, 0); digitalWrite(INB3, 0); }
}

void parar() { motor1(0, 0); motor2(0, 0); motor3(0, 0); }

// =============================================================================
// GIRÓSCOPO
// =============================================================================
bool inicializarGyro() {
  Serial.println("\n=== INICIALIZANDO BNO055 (NO MOVER!) ===");
  Serial.print("1. Detectando... ");
  unsigned long ini = millis();
  while (!bno.begin()) {
    if (millis() - ini > 3000) { Serial.println("ERROR!"); return false; }
    delay(100);
  }
  Serial.println("OK!");
  bno.setExtCrystalUse(true);
  Serial.print("2. Estabilización... "); delay(1000); Serial.println("OK!");

  Serial.println("3. Calibrando gyro...");
  ini = millis();
  while (millis() - ini < 5000) {
    uint8_t s, g, a, m;
    bno.getCalibration(&s, &g, &a, &m);
    Serial.print("   SYS="); Serial.print(s); Serial.print(" GYR="); Serial.println(g);
    if (g >= 3) { Serial.println("   Calibrado!"); break; }
    digitalWrite(LED_BUILTIN, (millis() / 200) % 2);
    delay(250);
  }

  float suma = 0;
  for (int i = 0; i < 10; i++) {
    sensors_event_t ev; bno.getEvent(&ev); suma += ev.orientation.x; delay(50);
  }
  headingOffset = suma / 10.0;
  Serial.print("4. Offset: "); Serial.println(headingOffset, 1);
  Serial.println("=== LISTO ===");
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}

float leerHeading() {
  if (!bnoOK) return 0;
  sensors_event_t ev; bno.getEvent(&ev);
  float h = ev.orientation.x - headingOffset;
  if (h > 180) h -= 360;
  if (h < -180) h += 360;
  return h;
}

void leerAceleracionLineal(float &ax, float &ay, float &az) {
  imu::Vector<3> a = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
  ax = a.x() * 100.0;
  ay = a.y() * 100.0;
  az = a.z() * 100.0;
}

// =============================================================================
// FUNCIÓN: calcularOmegaHeading()
// Calcula la velocidad angular de corrección del heading usando PID completo.
// Retorna omega para usar en la cinemática.
// =============================================================================
float calcularOmegaHeading() {
  float headingActual = leerHeading();
  float error = 0 - headingActual;
  if (error > 180) error -= 360;
  if (error < -180) error += 360;

  unsigned long ahora = millis();
  float dt = (ahora - pidTiempoAnterior) / 1000.0;
  if (dt <= 0) dt = 0.01;
  pidTiempoAnterior = ahora;

  float P = Kp_heading * error;

  headingIntegral += error * dt;
  headingIntegral = constrain(headingIntegral, -INTEGRAL_MAX_HEADING, INTEGRAL_MAX_HEADING);
  float I = Ki_heading * headingIntegral;

  float derivada = (error - headingErrorAnterior) / dt;
  float D = Kd_heading * derivada;
  headingErrorAnterior = error;

  float correccion = P + I + D;
  correccion = constrain(correccion, -MAX_CORRECCION_HEADING, MAX_CORRECCION_HEADING);

  return correccion;
}

// =============================================================================
// FUNCIÓN: aplicarMotoresOmni()
// Aplica vx, vy (traslación) + omega (rotación) a los 3 motores.
// L_rot controla el peso de la rotación en la cinemática.
// =============================================================================
void aplicarMotoresOmni(float vx, float vy, float omega, float L_rot) {
  float m1 = -0.5 * vx + 0.866 * vy + L_rot * omega;
  float m2 = -0.5 * vx - 0.866 * vy + L_rot * omega;
  float m3 =  1.0 * vx + 0.0   * vy + L_rot * omega;

  // Saturación proporcional
  float maxM = max(abs(m1), max(abs(m2), abs(m3)));
  if (maxM > MOTOR_MAX) {
    float s = (float)MOTOR_MAX / maxM;
    m1 *= s; m2 *= s; m3 *= s;
  }

  // Si todo es muy chico, no mover
  if (abs(m1) + abs(m2) + abs(m3) < 5) {
    parar();
    return;
  }

  int d1 = (m1 > 0) ? 1 : ((m1 < 0) ? -1 : 0);
  int d2 = (m2 > 0) ? 1 : ((m2 < 0) ? -1 : 0);
  int d3 = (m3 > 0) ? 1 : ((m3 < 0) ? -1 : 0);

  int pwm1 = (int)abs(m1); int pwm2 = (int)abs(m2); int pwm3 = (int)abs(m3);
  if (pwm1 > 5 && pwm1 < PWM_MINIMO_MOTOR) pwm1 = PWM_MINIMO_MOTOR;
  if (pwm2 > 5 && pwm2 < PWM_MINIMO_MOTOR) pwm2 = PWM_MINIMO_MOTOR;
  if (pwm3 > 5 && pwm3 < PWM_MINIMO_MOTOR) pwm3 = PWM_MINIMO_MOTOR;

  motor1(pwm1, d1);
  motor2(pwm2, d2);
  motor3(pwm3, d3);
}

// =============================================================================
// FUNCIÓN: aplicarSoloHeading()
// Solo corrige heading (traslación = 0). Para usar en estados sin empujón.
// =============================================================================
void aplicarSoloHeading() {
  float omega = calcularOmegaHeading();
  aplicarMotoresOmni(0, 0, omega, L_ROTACION_HEADING);
}

// =============================================================================
// FUNCIÓN: aplicarCorreccionCompleta()
// Corrige heading + traslación opuesta al empujón.
// =============================================================================
void aplicarCorreccionCompleta() {
  float vx_corr = Kp_pos * empujon_x;
  float vy_corr = Kp_pos * empujon_y;

  float mag = sqrt(vx_corr * vx_corr + vy_corr * vy_corr);
  if (mag > VEL_MAX_CORRECCION) {
    float s = VEL_MAX_CORRECCION / mag;
    vx_corr *= s; vy_corr *= s;
  }

  float omega = calcularOmegaHeading();
  aplicarMotoresOmni(vx_corr, vy_corr, omega, L_ROTACION_CORRECCION);
}

// =============================================================================
// Acelerómetro
// =============================================================================
float integrarAcelerometro() {
  float ax, ay, az;
  leerAceleracionLineal(ax, ay, az);
  float magnitudAccel = sqrt(ax * ax + ay * ay);

  unsigned long ahora = millis();
  float dt = (ahora - accelTiempoAnterior) / 1000.0;
  if (dt <= 0 || dt > 0.1) { accelTiempoAnterior = ahora; return magnitudAccel; }
  accelTiempoAnterior = ahora;

  float ax_f = (abs(ax) > UMBRAL_ACCEL_INTEGRAR) ? ax : 0;
  float ay_f = (abs(ay) > UMBRAL_ACCEL_INTEGRAR) ? ay : 0;

  vel_x += ax_f * dt;
  vel_y += ay_f * dt;
  float dv = pow(DECAY_VEL_POR_SEG, dt);
  vel_x *= dv; vel_y *= dv;

  pos_x += vel_x * dt;
  pos_y += vel_y * dt;
  float dp = pow(DECAY_POS_POR_SEG, dt);
  pos_x *= dp; pos_y *= dp;

  return magnitudAccel;
}

void resetearEstimacion() {
  vel_x = 0; vel_y = 0; pos_x = 0; pos_y = 0;
  accelTiempoAnterior = millis();
}

void resetPIDHeading() {
  headingErrorAnterior = 0;
  headingIntegral = 0;
  pidTiempoAnterior = millis();
}

// =============================================================================
// Serial
// =============================================================================
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'p': Kp_pos += 1.0;
        Serial.print("  >> Kp_pos="); Serial.println(Kp_pos, 1); break;
      case 'P': Kp_pos = max(1.0f, Kp_pos - 1.0f);
        Serial.print("  >> Kp_pos="); Serial.println(Kp_pos, 1); break;
      case 'h': Kp_heading += 0.5;
        Serial.print("  >> Kp_head="); Serial.println(Kp_heading, 1); break;
      case 'H': Kp_heading = max(0.5f, Kp_heading - 0.5f);
        Serial.print("  >> Kp_head="); Serial.println(Kp_heading, 1); break;
      case 'd': TIEMPO_CORRECCION_POR_CM += 20;
        Serial.print("  >> T/cm="); Serial.print(TIEMPO_CORRECCION_POR_CM, 0); Serial.println("ms"); break;
      case 'D': TIEMPO_CORRECCION_POR_CM = max(50.0f, TIEMPO_CORRECCION_POR_CM - 20.0f);
        Serial.print("  >> T/cm="); Serial.print(TIEMPO_CORRECCION_POR_CM, 0); Serial.println("ms"); break;
      case 't': UMBRAL_ACCEL_INICIO += 2.0;
        Serial.print("  >> UMB_INI="); Serial.println(UMBRAL_ACCEL_INICIO, 0); break;
      case 'T': UMBRAL_ACCEL_INICIO = max(5.0f, UMBRAL_ACCEL_INICIO - 2.0f);
        Serial.print("  >> UMB_INI="); Serial.println(UMBRAL_ACCEL_INICIO, 0); break;
      case 'm': PWM_MINIMO_MOTOR = min(120, PWM_MINIMO_MOTOR + 5);
        Serial.print("  >> PWM_MIN="); Serial.println(PWM_MINIMO_MOTOR); break;
      case 'M': PWM_MINIMO_MOTOR = max(20, PWM_MINIMO_MOTOR - 5);
        Serial.print("  >> PWM_MIN="); Serial.println(PWM_MINIMO_MOTOR); break;
      case 'r': resetearEstimacion(); resetPIDHeading();
        Serial.println("  >> Reset todo"); break;
      case 'v':
        Serial.println("\n  === VALORES ===");
        Serial.print("  Kp_pos="); Serial.print(Kp_pos, 1);
        Serial.print("  Kp_head="); Serial.println(Kp_heading, 1);
        Serial.print("  Ki_head="); Serial.print(Ki_heading, 2);
        Serial.print("  Kd_head="); Serial.println(Kd_heading, 2);
        Serial.print("  L_ROT_H="); Serial.print(L_ROTACION_HEADING, 2);
        Serial.print("  L_ROT_C="); Serial.println(L_ROTACION_CORRECCION, 2);
        Serial.print("  T/cm="); Serial.print(TIEMPO_CORRECCION_POR_CM, 0);
        Serial.print("ms UMB_INI="); Serial.println(UMBRAL_ACCEL_INICIO, 0);
        Serial.print("  PWM_MIN="); Serial.print(PWM_MINIMO_MOTOR);
        Serial.print("  Heading="); Serial.print(leerHeading(), 1); Serial.println("°");
        Serial.println("  ================\n"); break;
    }
  }
}

bool botonPresionado() {
  bool e = (digitalRead(PIN_BOTON) == HIGH);
  if (e && !botonAnterior) { botonAnterior = e; delay(50); return true; }
  botonAnterior = e; return false;
}
void esperarSoltarBoton() {
  while (digitalRead(PIN_BOTON) == HIGH) delay(10);
  delay(50); botonAnterior = false;
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(19200);
  delay(500);

  Serial.println("\n****************************************************");
  Serial.println("*  HEADING + CORRECCIÓN DESPLAZAMIENTO v7          *");
  Serial.println("*  Mantiene frente (gyro) + corrige empujones      *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                       *");
  Serial.println("****************************************************\n");
  Serial.println("CMD: p/P=Kp_pos h/H=Kp_head d/D=tiempo t/T=umbral");
  Serial.println("     m/M=pwm_min r=reset v=ver\n");

  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
  pinMode(PIN_BOTON, INPUT);
  pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
  pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
  pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);
  parar();

  bnoOK = inicializarGyro();
  if (!bnoOK) {
    Serial.println("*** ERROR BNO055 ***");
    while (true) { digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW); delay(100); }
  }

  Serial.print("Kp_pos="); Serial.print(Kp_pos, 1);
  Serial.print(" Kp_head="); Serial.print(Kp_heading, 1);
  Serial.print(" PWM_MIN="); Serial.println(PWM_MINIMO_MOTOR);
  Serial.println("\n>>> BOTON para acelerómetro en vivo <<<");
  accelTiempoAnterior = millis();
  pidTiempoAnterior = millis();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  static unsigned long ultimoPrint = 0;
  procesarComandoSerial();

  switch (estado) {

    case ESPERANDO_INICIO:
      if (botonPresionado()) {
        esperarSoltarBoton(); estado = MOSTRANDO_ACCEL;
        Serial.println("\n>>> ACELERÓMETRO EN VIVO <<<\n");
      }
      break;

    case MOSTRANDO_ACCEL: {
      if (millis() - ultimoPrint > 150) {
        float ax, ay, az;
        leerAceleracionLineal(ax, ay, az);
        float mag = sqrt(ax * ax + ay * ay);
        Serial.print("X:"); if (ax >= 0) Serial.print("+"); Serial.print(ax, 0);
        Serial.print(" Y:"); if (ay >= 0) Serial.print("+"); Serial.print(ay, 0);
        Serial.print(" mag="); Serial.print(mag, 0);
        Serial.print(" H:"); Serial.print(leerHeading(), 1); Serial.println("°");
        ultimoPrint = millis();
      }
      if (botonPresionado()) {
        esperarSoltarBoton();
        resetearEstimacion(); resetPIDHeading();
        subEstado = ESCUCHANDO; estado = ACTIVO;
        Serial.println("\n*** ACTIVO — Heading + corrección empujones ***");
        Serial.println("El robot mantiene el frente Y corrige si lo empujan.\n");
      }
      break;
    }

    case ACTIVO: {

      // === ESCUCHANDO: HEADING ACTIVO + esperando empujón ===
      if (subEstado == ESCUCHANDO) {
        // Heading PID siempre activo (mantiene el frente)
        aplicarSoloHeading();

        // Leer acelerómetro para detectar empujón
        float magAccel = integrarAcelerometro();
        resetearEstimacion();  // No acumular pos en este estado

        // LED encendido fijo = heading activo, esperando
        digitalWrite(LED_BUILTIN, HIGH);

        if (millis() - ultimoPrint > 500) {
          float h = leerHeading();
          Serial.print("  [HEADING] H:");
          if (h >= 0) Serial.print("+");
          Serial.print(h, 1);
          Serial.print("° acc="); Serial.print(magAccel, 0);
          Serial.println();
          ultimoPrint = millis();
        }

        if (magAccel > UMBRAL_ACCEL_INICIO) {
          resetearEstimacion();
          tiempoUltimaAccelAlta = millis();
          subEstado = EMPUJANDO;
          Serial.println("\n  >>> EMPUJON DETECTADO — integrando...");
        }
      }

      // === EMPUJANDO: HEADING ACTIVO + integrando acelerómetro ===
      else if (subEstado == EMPUJANDO) {
        // Heading sigue activo mientras se empuja
        aplicarSoloHeading();

        float magAccel = integrarAcelerometro();
        digitalWrite(LED_BUILTIN, (millis() / 50) % 2);

        if (magAccel > UMBRAL_ACCEL_FIN) {
          tiempoUltimaAccelAlta = millis();
        }

        if (millis() - ultimoPrint > 100) {
          Serial.print("  [EMPUJANDO] pos(");
          Serial.print(pos_x, 1); Serial.print(","); Serial.print(pos_y, 1);
          Serial.print(")cm acc="); Serial.println(magAccel, 0);
          ultimoPrint = millis();
        }

        if (millis() - tiempoUltimaAccelAlta > TIEMPO_QUIETO_PARA_FIN) {
          empujon_x = pos_x;
          empujon_y = pos_y;
          empujon_magnitud = sqrt(empujon_x * empujon_x + empujon_y * empujon_y);

          if (empujon_magnitud < 0.1) {
            Serial.println("  -> Empujón muy chico, ignorando");
            subEstado = ESCUCHANDO;
          } else {
            duracionCorreccion = (unsigned long)(empujon_magnitud * TIEMPO_CORRECCION_POR_CM);
            duracionCorreccion = constrain(duracionCorreccion, TIEMPO_CORRECCION_MIN, TIEMPO_CORRECCION_MAX);
            float angulo = atan2(empujon_x, empujon_y) * 180.0 / PI;
            tiempoInicioFase = millis();
            subEstado = CORRIGIENDO;

            Serial.print("\n  >>> SOLTO! (");
            Serial.print(empujon_x, 1); Serial.print(","); Serial.print(empujon_y, 1);
            Serial.print(")cm = "); Serial.print(empujon_magnitud, 1);
            Serial.print("cm dir="); Serial.print(angulo, 0);
            Serial.print("° -> "); Serial.print(duracionCorreccion);
            Serial.println("ms <<<\n");
          }
        }
      }

      // === CORRIGIENDO: HEADING + TRASLACIÓN opuesta al empujón ===
      else if (subEstado == CORRIGIENDO) {
        aplicarCorreccionCompleta();
        digitalWrite(LED_BUILTIN, HIGH);

        unsigned long t = millis() - tiempoInicioFase;
        if (millis() - ultimoPrint > 200) {
          Serial.print("  [CORR] t="); Serial.print(t); Serial.print("/"); Serial.print(duracionCorreccion);
          Serial.print("ms H:"); Serial.print(leerHeading(), 1); Serial.println("°");
          ultimoPrint = millis();
        }

        if (t >= duracionCorreccion) {
          tiempoInicioFase = millis();
          subEstado = PAUSA_POST;
          Serial.println("  -> Corrección completa");
        }
      }

      // === PAUSA POST: HEADING ACTIVO, acelerómetro off ===
      else if (subEstado == PAUSA_POST) {
        aplicarSoloHeading();
        digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
        if (millis() - tiempoInicioFase >= TIEMPO_PAUSA_POST) {
          resetearEstimacion();
          tiempoInicioFase = millis();
          subEstado = ENFRIAMIENTO;
          Serial.println("  -> Enfriando...");
        }
      }

      // === ENFRIAMIENTO: HEADING ACTIVO, acelerómetro IGNORADO ===
      else if (subEstado == ENFRIAMIENTO) {
        aplicarSoloHeading();
        digitalWrite(LED_BUILTIN, HIGH);
        if (millis() - tiempoInicioFase >= TIEMPO_ENFRIAMIENTO) {
          resetearEstimacion();
          subEstado = ESCUCHANDO;
          Serial.println("  -> Escuchando...\n");
        }
      }

      if (botonPresionado()) {
        esperarSoltarBoton(); parar();
        resetearEstimacion(); resetPIDHeading();
        subEstado = ESCUCHANDO;
        Serial.println("\n  >> RESET\n");
      }
      break;
    }
  }
}
