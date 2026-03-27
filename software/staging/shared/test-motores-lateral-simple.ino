// =============================================================================
// TEST: Movimiento lateral CON GIRÓSCOPO Y PID
// Archivo: staging/shared/test-motores-lateral-simple.ino
//
// PROPÓSITO:
//   - Movimiento lateral usando cinemática omnidireccional correcta
//   - Giróscopo BNO055 para mantener orientación
//   - Control PID para corregir desviaciones en los 3 MOTORES
//
// CAMBIOS v2 (2026-03-27, Claude bajo supervisión Gustavo):
//   - FIX: PID se calculaba 2 veces por loop (corrompía derivada)
//   - FIX: constrain(0,255) mataba rueda en vez de invertir dirección
//   - FIX: M3 no participaba en corrección de heading (ahora los 3 corrigen)
//   - NUEVO: saturación proporcional (mantiene dirección correcta)
//   - NUEVO: FACTOR_ROTACION para ajustar peso del PID vs movimiento lateral
//   - NUEVO: serial muestra velocidades individuales de cada motor
//   Valores calibrados por María (VEL, DIR) se mantienen sin cambio.
//
// CINEMÁTICA:
//   Velocidad total de cada motor = componente LATERAL + componente ROTACIÓN
//   - Lateral: lo que hace que el robot se desplace (calibrado por María)
//   - Rotación: lo que corrige el heading (PID, los 3 motores participan)
//   Si algún motor supera 255, se escalan TODOS proporcionalmente.
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
// Derivados de girar() del definitivo: antihorario = M1 dir-1, M2 dir+1, M3 dir-1
// → Para rotación HORARIA (corrección positiva): M1=+1, M2=-1, M3=+1
// Si el robot corrige para el lado EQUIVOCADO, invertir LOS TRES signos.
int ROT_M1 = 1;
int ROT_M2 = -1;
int ROT_M3 = 1;

// PARÁMETROS PID (ajustar si oscila o no corrige suficiente)
float Kp = 3.0;      // Proporcional - respuesta al error actual
float Ki = 0.05;     // Integral - corrige error acumulado
float Kd = 0.5;      // Derivativo - suaviza la respuesta

// FACTOR_ROTACION: peso de la corrección de heading (0.0 = sin corrección, 1.0 = máxima)
// Empezar en 0.5 y subir si no corrige suficiente, bajar si oscila.
float FACTOR_ROTACION = 0.5;

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

// Variables de debug (para evitar llamar PID dos veces por loop)
float ultimoHeading = 0;
float ultimaCorreccion = 0;
float debugM1 = 0, debugM2 = 0, debugM3 = 0;

// Variables de estado
bool yendoDerecha = true;
bool enPausa = false;
unsigned long tiempoInicio = 0;

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
// IMPORTANTE: llamar UNA SOLA VEZ por ciclo de loop(). Si se necesita el
// valor para debug, usar la variable ultimaCorreccion.
// =============================================================================
float calcularCorreccionPID(float headingActual) {
  float error = 0 - headingActual;  // Queremos mantener heading = 0

  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnteriorPID) / 1000.0;
  if (dt <= 0) dt = 0.01;
  tiempoAnteriorPID = ahora;

  // Proporcional
  float P = Kp * error;

  // Integral
  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I = Ki * integral;

  // Derivativo
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
// FUNCIÓN: moverLateral() — CORREGIDA v2
//
// Cada motor recibe: velocidad_lateral + velocidad_rotacion
//   - Lateral: desplaza el robot (DIR * VEL * direccion)
//   - Rotación: corrige heading (ROT * correccion * FACTOR_ROTACION)
// Si algún motor supera 255, se escalan TODOS proporcionalmente.
// El signo del total determina la dirección del motor (no se clampea a 0).
// =============================================================================
void moverLateral(int direccion) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  // Guardar para debug (NUNCA llamar calcularCorreccionPID de nuevo este ciclo)
  ultimoHeading = heading;
  ultimaCorreccion = correccion;

  // 1. Componente LATERAL (base, calibrada por María)
  float lat_M1 = (float)(DIR_M1 * VEL_M1 * direccion);
  float lat_M2 = (float)(DIR_M2 * VEL_M2 * direccion);
  float lat_M3 = (float)(DIR_M3 * VEL_M3 * direccion);

  // 2. Componente ROTACIÓN del PID (los 3 motores participan)
  float rot_M1 = ROT_M1 * correccion * FACTOR_ROTACION;
  float rot_M2 = ROT_M2 * correccion * FACTOR_ROTACION;
  float rot_M3 = ROT_M3 * correccion * FACTOR_ROTACION;

  // 3. Sumar lateral + rotación
  float total_M1 = lat_M1 + rot_M1;
  float total_M2 = lat_M2 + rot_M2;
  float total_M3 = lat_M3 + rot_M3;

  // 4. Saturación PROPORCIONAL (mantiene dirección correcta)
  //    En vez de clampear cada motor a [0,255] (que mata ruedas),
  //    si alguno supera 255, escalamos TODOS para que el más grande sea 255.
  float maxM = max(abs(total_M1), max(abs(total_M2), abs(total_M3)));
  if (maxM > 255) {
    float escala = 255.0 / maxM;
    total_M1 *= escala;
    total_M2 *= escala;
    total_M3 *= escala;
  }

  // Guardar para debug
  debugM1 = total_M1;
  debugM2 = total_M2;
  debugM3 = total_M3;

  // 5. Aplicar a motores (el signo del total determina la dirección)
  int d1 = (total_M1 > 0) ? 1 : ((total_M1 < 0) ? -1 : 0);
  int d2 = (total_M2 > 0) ? 1 : ((total_M2 < 0) ? -1 : 0);
  int d3 = (total_M3 > 0) ? 1 : ((total_M3 < 0) ? -1 : 0);

  motor1((int)abs(total_M1), d1);
  motor2((int)abs(total_M2), d2);
  motor3((int)abs(total_M3), d3);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(19200);
  delay(500);

  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST LATERAL CON GIROSCOPO Y PID  (v2)          *");
  Serial.println("*  Cinemática omnidireccional 3 ruedas             *");
  Serial.println("*  Corrección PID en los 3 motores                 *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("VALORES ACTUALES:");
  Serial.print("  VEL_M1="); Serial.print(VEL_M1);
  Serial.print("  VEL_M2="); Serial.print(VEL_M2);
  Serial.print("  VEL_M3="); Serial.println(VEL_M3);
  Serial.print("  DIR_M1="); Serial.print(DIR_M1);
  Serial.print("  DIR_M2="); Serial.print(DIR_M2);
  Serial.print("  DIR_M3="); Serial.println(DIR_M3);
  Serial.print("  ROT_M1="); Serial.print(ROT_M1);
  Serial.print("  ROT_M2="); Serial.print(ROT_M2);
  Serial.print("  ROT_M3="); Serial.println(ROT_M3);
  Serial.print("  Kp="); Serial.print(Kp);
  Serial.print("  Ki="); Serial.print(Ki);
  Serial.print("  Kd="); Serial.println(Kd);
  Serial.print("  FACTOR_ROTACION="); Serial.println(FACTOR_ROTACION);
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
    Serial.println("El robot se moverá SIN corrección PID");
  }

  Serial.println("\n>>> INICIANDO EN 2 SEGUNDOS <<<");
  delay(2000);

  resetPID();
  tiempoInicio = millis();
  yendoDerecha = true;
  enPausa = false;
  Serial.println("\n>>> DERECHA <<<");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;

  // Estado: PAUSA entre movimientos
  if (enPausa) {
    parar();
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);

    if (tiempoTranscurrido >= TIEMPO_PAUSA) {
      enPausa = false;
      yendoDerecha = !yendoDerecha;
      resetPID();  // Resetear PID al cambiar de dirección
      tiempoInicio = millis();

      if (yendoDerecha) {
        Serial.println("\n>>> DERECHA <<<");
      } else {
        Serial.println("\n>>> IZQUIERDA <<<");
      }
    }
    return;
  }

  // Estado: EN MOVIMIENTO
  int direccion = yendoDerecha ? 1 : -1;
  moverLateral(direccion);
  digitalWrite(LED_BUILTIN, yendoDerecha ? HIGH : LOW);

  // Mostrar estado cada 200ms
  // IMPORTANTE: usar ultimoHeading y ultimaCorreccion (calculados en moverLateral)
  // NUNCA llamar calcularCorreccionPID() de nuevo acá (corrompe la derivada)
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
    Serial.print(" | t=");
    Serial.print(tiempoTranscurrido / 1000.0, 1);
    Serial.println("s");

    ultimoPrint = millis();
  }

  // Verificar si terminó el tiempo de movimiento
  if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
    parar();
    enPausa = true;
    tiempoInicio = millis();
    Serial.println("  -> pausa");
  }
}
