// =============================================================================
// TEST: Movimiento lateral CON GIRÓSCOPO Y PID
// Archivo: staging/shared/test-motores-lateral-simple.ino
// 
// PROPÓSITO:
//   - Movimiento lateral usando cinemática omnidireccional correcta
//   - Giróscopo BNO055 para mantener orientación
//   - Control PID para corregir desviaciones
//
// CINEMÁTICA ROBOT OMNIDIRECCIONAL 3 RUEDAS (120°):
//   Para movimiento LATERAL (DERECHA):
//     - M1 (frontal): adelante (+1)
//     - M2 (frontal): atrás (-1) ← OPUESTO a M1
//     - M3 (trasero): hacia derecha (+1)
//   Para IZQUIERDA: se invierten todas las direcciones
//
// ROBOT 2 (delantero)
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

// VELOCIDADES BASE (0 a 255)
int VEL_M1 = 55;     // Velocidad Motor 1 (frontal)
int VEL_M2 = 55;     // Velocidad Motor 2 (frontal)
int VEL_M3 = 100;    // Velocidad Motor 3 (trasero - rueda lateral)

// DIRECCIONES para ir a la DERECHA
// M1 y M2 giran en direcciones OPUESTAS (cinemática omnidireccional)
int DIR_M1 = -1;      // Motor 1: adelante (+1)
int DIR_M2 = 1;     // Motor 2: atrás (-1)  ← OPUESTO a M1
int DIR_M3 = 1;      // Motor 3: hacia derecha (+1)

// PARÁMETROS PID (ajustar si oscila o no corrige suficiente)
float Kp = 3.0;      // Proporcional - respuesta al error actual
float Ki = 0.05;     // Integral - corrige error acumulado
float Kd = 0.5;      // Derivativo - suaviza la respuesta

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
// FUNCIÓN: moverLateral()
// Movimiento lateral con corrección PID de heading
// direccion: 1 = derecha, -1 = izquierda
// =============================================================================
void moverLateral(int direccion) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);
  
  // Aplicar corrección a M1 y M2 (los frontales)
  // La corrección ajusta las velocidades para corregir el giro
  int velM1_final = VEL_M1 + (int)correccion;
  int velM2_final = VEL_M2 - (int)correccion;
  
  // Asegurar que las velocidades estén en rango válido
  velM1_final = constrain(velM1_final, 0, 255);
  velM2_final = constrain(velM2_final, 0, 255);
  
  // Aplicar movimiento con direcciones según cinemática omnidireccional
  motor1(velM1_final, DIR_M1 * direccion);
  motor2(velM2_final, DIR_M2 * direccion);
  motor3(VEL_M3, DIR_M3 * direccion);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(19200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST LATERAL CON GIROSCOPO Y PID                *");
  Serial.println("*  Cinemática omnidireccional 3 ruedas             *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("VALORES ACTUALES:");
  Serial.print("  VEL_M1="); Serial.print(VEL_M1);
  Serial.print("  VEL_M2="); Serial.print(VEL_M2);
  Serial.print("  VEL_M3="); Serial.println(VEL_M3);
  Serial.print("  Kp="); Serial.print(Kp);
  Serial.print("  Ki="); Serial.print(Ki);
  Serial.print("  Kd="); Serial.println(Kd);
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
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 200) {
    float heading = leerHeading();
    float correccion = calcularCorreccionPID(heading);
    
    Serial.print("  ");
    Serial.print(yendoDerecha ? "[DER]" : "[IZQ]");
    Serial.print(" H:");
    if (heading >= 0) Serial.print("+");
    Serial.print(heading, 1);
    Serial.print("° C:");
    Serial.print(correccion, 1);
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
