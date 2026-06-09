// ⚠️ STAGING CONGELADO — NO FLASHEAR. Sketch de prueba 2025 con BNO local + lib vieja; el equivalente VIVO esta en src/diag/ (ver software/staging/README.md).
// ============================================================================
// ⚠️ STAGING CONGELADO (2026-06-03) — NO subir más material a software/staging/.
// Antes de tocar o agregar algo acá, LEÉ:
//   software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md
// Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
//   software/teensy/Soccer 2026/src/  (ver el "mapa de reúso" del documento).
// ============================================================================

// =============================================================================
// TEST: Movimiento lateral CON GIRÓSCOPO Y PID
// Archivo: staging/shared/test-motores-lateral-simple.ino
//
// PROPÓSITO:
//   - Movimiento lateral usando cinemática omnidireccional correcta
//   - Giróscopo BNO055 para mantener orientación
//   - Control PID para corregir desviaciones en los 3 MOTORES
//
// CAMBIOS v4 (2026-03-27, Claude bajo supervisión María):
//   - NUEVO: Anticipación de frenado DINÁMICA proporcional a VEL_M3
//     (si bajás VEL_M3, la anticipación baja sola)
//   - NUEVO: Auto-calibración con giroscopio: después de cada frenado, mide
//     la desviación del heading y ajusta la anticipación automáticamente
//   - NUEVO: Comandos Serial para ajustar en tiempo real:
//       '+' sube anticipación base 10ms
//       '-' baja anticipación base 10ms
//       'c' activa/desactiva auto-calibración
//       'v' muestra valores actuales
//
// CAMBIOS v3 (2026-03-27, Claude bajo supervisión María):
//   - FIX: M3 frenaba tarde por mayor inercia, pre-frenado con anticipación
//
// CAMBIOS v2 (2026-03-27, Claude bajo supervisión Gustavo):
//   - FIX: PID se calculaba 2 veces por loop (corrompía derivada)
//   - FIX: constrain(0,255) mataba rueda en vez de invertir dirección
//   - FIX: M3 no participaba en corrección de heading (ahora los 3 corrigen)
//   - NUEVO: saturación proporcional, FACTOR_ROTACION, debug serial
//
// ROBOT 2 (delantero)
// Referencia: docs/internal/lecciones-pid-movimiento-lateral.md
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 2 (NO MODIFICAR)
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

// =============================================================================
// ******************** AJUSTAR ESTOS VALORES ********************
// =============================================================================

// VELOCIDADES BASE para movimiento lateral (0 a 255)
int VEL_M1 = 55;     // Velocidad Motor 1 (frontal)
int VEL_M2 = 55;     // Velocidad Motor 2 (frontal)
int VEL_M3 = 100;    // Velocidad Motor 3 (trasero - rueda lateral principal)

// DIRECCIONES para ir a la DERECHA (calibradas por María en robot físico)
int DIR_M1 = -1;     // Motor 1
int DIR_M2 = 1;      // Motor 2 (opuesto a M1 por hardware invertido)
int DIR_M3 = 1;      // Motor 3

// SIGNOS DE ROTACIÓN para corrección de heading
int ROT_M1 = 1;
int ROT_M2 = -1;
int ROT_M3 = 1;

// PARÁMETROS PID
float Kp = 3.0;
float Ki = 0.05;
float Kd = 0.5;

// FACTOR_ROTACION: peso de la corrección de heading
float FACTOR_ROTACION = 0.5;

// =====================================================================
// COMPENSACIÓN DE FRENADO M3 (v4 — DINÁMICO)
// =====================================================================
// BASE_ANTICIPACION_MS: anticipación en ms calibrada para VEL_M3 = 100.
//   La anticipación REAL se calcula automáticamente:
//     anticipacionReal = BASE_ANTICIPACION_MS * VEL_M3 / 100
//   Así si bajás VEL_M3 a 60, la anticipación baja proporcionalmente.
//
// Ajustar con Serial: '+' sube 10ms, '-' baja 10ms
// O cambiar este valor directamente en el código.
float BASE_ANTICIPACION_MS = 60.0;  // ms (calibrado para VEL_M3=100)

// AUTO-CALIBRACIÓN CON GIROSCOPIO:
//   Después de cada frenado, el robot mide cuánto se desvió el heading.
//   Si se desvió más de UMBRAL_DRIFT grados, ajusta BASE_ANTICIPACION_MS
//   automáticamente en PASO_AUTOCAL ms.
//   Activar/desactivar con 'c' en Serial.
bool autoCalActivada = true;
float UMBRAL_DRIFT = 1.5;     // grados: debajo de esto se considera "ok"
float PASO_AUTOCAL = 5.0;     // ms que ajusta en cada auto-calibración
float MAX_ANTICIPACION = 300;  // límite máximo de seguridad
float MIN_ANTICIPACION = 0;    // límite mínimo

// TIEMPO de movimiento
unsigned long TIEMPO_MOVIMIENTO = 3000;  // 3 segundos cada dirección
unsigned long TIEMPO_PAUSA = 500;        // 0.5 segundos de pausa

// =============================================================================
// ******************** FIN AJUSTES ********************
// =============================================================================

// Límites del PID
const float MAX_CORRECCION = 50;
const float INTEGRAL_MAX = 40;

// Giróscopo
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

// Variables PID
float errorAnterior = 0;
float integral = 0;
unsigned long tiempoAnteriorPID = 0;

// Variables de debug
float ultimoHeading = 0;
float ultimaCorreccion = 0;
float debugM1 = 0, debugM2 = 0, debugM3 = 0;

// Variables de estado
bool yendoDerecha = true;
bool enPausa = false;
bool m3Frenado = false;
unsigned long tiempoInicio = 0;

// v4: Variables de auto-calibración
float headingAlFrenar = 0;       // heading en el instante que se paran todos los motores
bool headingAlFrenarCapturado = false;
float anticipacionReal = 0;      // valor calculado dinámicamente

// =============================================================================
// FUNCIÓN: calcularAnticipacionReal()
// Escala la anticipación base proporcionalmente a VEL_M3
// =============================================================================
float calcularAnticipacionReal() {
  return BASE_ANTICIPACION_MS * (float)VEL_M3 / 100.0;
}

// =============================================================================
// FUNCIÓN: procesarComandoSerial()
// Comandos: '+' sube, '-' baja, 'c' toggle autocal, 'v' muestra valores
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
        Serial.print("  UMBRAL_DRIFT = "); Serial.print(UMBRAL_DRIFT, 1); Serial.println("°");
        Serial.println("  =======================\n");
        break;
    }
  }
}

// =============================================================================
// FUNCIÓN: inicializarGyro()
// =============================================================================
bool inicializarGyro() {
  Serial.println("\n=== INICIALIZANDO GIROSCOPO BNO055 ===");
  Serial.println("NO MOVER EL ROBOT!");

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

  Serial.print("3. Esperando estabilización... ");
  delay(1000);
  Serial.println("OK!");

  Serial.println("4. Calibrando giróscopo (NO MOVER!)...");
  inicio = millis();
  bool gyroCalibrado = false;

  while (millis() - inicio < 5000) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    Serial.print("   SYS="); Serial.print(sys);
    Serial.print(" GYR="); Serial.print(gyro);
    Serial.print(" ACC="); Serial.print(accel);
    Serial.print(" MAG="); Serial.println(mag);

    if (gyro >= 3) {
      Serial.println("   -> Calibrado!");
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
    delay(50);
  }
  headingOffset = suma / 10.0;
  Serial.print("   Offset: "); Serial.println(headingOffset, 1);

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
// =============================================================================
float calcularCorreccionPID(float headingActual) {
  float error = 0 - headingActual;

  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnteriorPID) / 1000.0;
  if (dt <= 0) dt = 0.01;
  tiempoAnteriorPID = ahora;

  float P = Kp * error;

  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I = Ki * integral;

  float derivada = (error - errorAnterior) / dt;
  float D = Kd * derivada;
  errorAnterior = error;

  float correccion = P + I + D;
  correccion = constrain(correccion, -MAX_CORRECCION, MAX_CORRECCION);

  return correccion;
}

// =============================================================================
// FUNCIONES DE MOTORES
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
// FUNCIÓN: moverLateral() — v4
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
// FUNCIÓN: autoCalibrarFrenado() — v4
//
// Se llama al INICIO de la pausa (justo después de frenar).
// Espera 100ms para que la inercia termine, mide el heading final,
// y compara con el heading capturado al momento de frenar.
// Si la diferencia supera UMBRAL_DRIFT, ajusta BASE_ANTICIPACION_MS.
//
// LÓGICA:
//   drift > 0 y iba a la derecha → M3 siguió empujando → subir anticipación
//   drift < 0 y iba a la derecha → M3 frenó de más → bajar anticipación
//   (invertido cuando va a la izquierda)
// =============================================================================
void autoCalibrarFrenado(bool ibaDerecha) {
  if (!autoCalActivada || !bnoOK) return;

  // Esperar a que se disipen las vibraciones/inercia residual
  delay(100);

  float headingFinal = leerHeading();
  float drift = headingFinal - headingAlFrenar;

  Serial.print("  [AUTOCAL] H_freno=");
  Serial.print(headingAlFrenar, 1);
  Serial.print("° H_final=");
  Serial.print(headingFinal, 1);
  Serial.print("° drift=");
  Serial.print(drift, 1);
  Serial.print("°");

  if (abs(drift) < UMBRAL_DRIFT) {
    Serial.println(" -> OK (dentro de umbral)");
    return;
  }

  // Determinar si M3 frenó tarde o temprano.
  // M3 es la rueda trasera que produce el movimiento lateral principal.
  // Si el heading se desvió en la dirección del movimiento, M3 siguió empujando
  // (frenó tarde) → necesitamos MÁS anticipación.
  // Si se desvió al revés, M3 frenó antes de tiempo → MENOS anticipación.
  //
  // Como el drift del heading depende de la geometría del robot y la dirección,
  // usamos el signo absoluto del drift: si es grande, ajustamos.
  // El signo del ajuste se determina comparando la dirección de movimiento
  // con la dirección del drift.

  float ajuste = PASO_AUTOCAL;

  // drift positivo + derecha = M3 empujó de más = necesita más anticipación
  // drift negativo + derecha = M3 frenó de más = necesita menos anticipación
  // (invertido para izquierda)
  bool m3FrenoTarde;
  if (ibaDerecha) {
    m3FrenoTarde = (drift > 0);  // heading se fue en dir. de movimiento
  } else {
    m3FrenoTarde = (drift < 0);  // invertido para izquierda
  }

  if (m3FrenoTarde) {
    BASE_ANTICIPACION_MS += ajuste;
    if (BASE_ANTICIPACION_MS > MAX_ANTICIPACION) BASE_ANTICIPACION_MS = MAX_ANTICIPACION;
    Serial.print(" -> M3 tarde, SUBIENDO a ");
  } else {
    BASE_ANTICIPACION_MS -= ajuste;
    if (BASE_ANTICIPACION_MS < MIN_ANTICIPACION) BASE_ANTICIPACION_MS = MIN_ANTICIPACION;
    Serial.print(" -> M3 temprano, BAJANDO a ");
  }

  anticipacionReal = calcularAnticipacionReal();
  Serial.print(BASE_ANTICIPACION_MS, 0);
  Serial.print("ms (real=");
  Serial.print(anticipacionReal, 0);
  Serial.println("ms)");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(19200);
  delay(500);

  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST LATERAL CON GIROSCOPO Y PID  (v4)          *");
  Serial.println("*  Compensación DINÁMICA de inercia M3             *");
  Serial.println("*  Auto-calibración con giroscopio                 *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("COMANDOS SERIAL:");
  Serial.println("  '+' o 'a' = subir anticipación 10ms");
  Serial.println("  '-' o 'z' = bajar anticipación 10ms");
  Serial.println("  'c'       = activar/desactivar auto-calibración");
  Serial.println("  'v'       = ver valores actuales");
  Serial.println();
  Serial.println("VALORES INICIALES:");
  Serial.print("  VEL_M1="); Serial.print(VEL_M1);
  Serial.print("  VEL_M2="); Serial.print(VEL_M2);
  Serial.print("  VEL_M3="); Serial.println(VEL_M3);
  Serial.print("  Kp="); Serial.print(Kp);
  Serial.print("  Ki="); Serial.print(Ki);
  Serial.print("  Kd="); Serial.println(Kd);
  Serial.print("  FACTOR_ROTACION="); Serial.println(FACTOR_ROTACION);
  Serial.print("  BASE_ANTICIPACION_MS="); Serial.print(BASE_ANTICIPACION_MS, 0); Serial.println("ms");

  // Calcular anticipación real inicial
  anticipacionReal = calcularAnticipacionReal();
  Serial.print("  anticipacionReal="); Serial.print(anticipacionReal, 0);
  Serial.println("ms (=BASE * VEL_M3/100)");
  Serial.print("  autoCalibración="); Serial.println(autoCalActivada ? "ACTIVADA" : "DESACTIVADA");
  Serial.println();

  // Configurar pines
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

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

  // Inicializar giróscopo
  bnoOK = inicializarGyro();

  if (!bnoOK) {
    Serial.println("\n*** ERROR: Giróscopo no inicializado ***");
    Serial.println("El robot se moverá SIN corrección PID ni auto-calibración");
  }

  Serial.println("\n>>> INICIANDO EN 2 SEGUNDOS <<<");
  delay(2000);

  resetPID();
  tiempoInicio = millis();
  yendoDerecha = true;
  enPausa = false;
  m3Frenado = false;
  headingAlFrenarCapturado = false;
  Serial.println("\n>>> DERECHA <<<");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;

  // Siempre procesar comandos serial
  procesarComandoSerial();

  // Estado: PAUSA entre movimientos
  if (enPausa) {
    parar();
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

    if (tiempoTranscurrido >= TIEMPO_PAUSA) {
      enPausa = false;
      yendoDerecha = !yendoDerecha;
      m3Frenado = false;
      headingAlFrenarCapturado = false;

      // Recalcular anticipación (por si cambió VEL_M3 o BASE)
      anticipacionReal = calcularAnticipacionReal();

      resetPID();
      tiempoInicio = millis();

      if (yendoDerecha) {
        Serial.println("\n>>> DERECHA <<<");
      } else {
        Serial.println("\n>>> IZQUIERDA <<<");
      }
      Serial.print("  (anticipación M3 = ");
      Serial.print(anticipacionReal, 0);
      Serial.println("ms)");
    }
    return;
  }

  // Estado: EN MOVIMIENTO
  int direccion = yendoDerecha ? 1 : -1;

  // v4: Anticipación dinámica
  bool debeFrenarM3 = (tiempoTranscurrido >= TIEMPO_MOVIMIENTO - (unsigned long)anticipacionReal);

  // Log una sola vez cuando M3 empieza a frenar
  if (debeFrenarM3 && !m3Frenado) {
    m3Frenado = true;
    Serial.print("  -> M3 frena (anticipación=");
    Serial.print(anticipacionReal, 0);
    Serial.println("ms)");
  }

  moverLateral(direccion, debeFrenarM3);
  digitalWrite(LED_BUILTIN, yendoDerecha ? HIGH : LOW);

  // Debug cada 200ms
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 200) {
    Serial.print("  ");
    Serial.print(yendoDerecha ? "[DER]" : "[IZQ]");
    Serial.print(" H:");
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

    ultimoPrint = millis();
  }

  // Verificar si terminó el tiempo de movimiento
  if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
    // v4: Capturar heading justo al momento de frenar TODO
    if (!headingAlFrenarCapturado) {
      headingAlFrenar = leerHeading();
      headingAlFrenarCapturado = true;
    }

    parar();
    enPausa = true;
    tiempoInicio = millis();
    Serial.println("  -> pausa");

    // v4: Auto-calibrar basándose en la desviación post-frenado
    autoCalibrarFrenado(yendoDerecha);

    m3Frenado = false;
  }
}
