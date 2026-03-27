// =============================================================================
// TEST: Movimiento lateral con DATA LOGGING y cinemática omnidireccional
// Archivo: staging/shared/test-gyro-movimiento-lateral.ino
// 
// PROPÓSITO: 
//   - Movimiento lateral RECTO (estilo metegol) usando cinemática omnidireccional
//   - DATA LOGGING: guarda heading, aceleración, velocidades de motores
//   - Al terminar: vuelca datos por Serial en formato CSV para análisis
//
// INSTRUCCIONES:
//   1. Subir al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER durante calibración (~5 segundos)
//   4. BOTON (pin 9):
//      - Presión 1: Inicia test heading
//      - Presión 2: Inicia loop lateral (3s der, 3s izq, repite)
//      - Presión 3 (durante pausa): VOLCAR DATOS CSV por Serial
//   5. Copiar datos CSV del Serial Monitor para análisis
//
// BASADO EN: test-gyro-movimiento-basico.ino
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// CONFIGURACIÓN DE PINES - ROBOT 2 (delantero)
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
const int VELOCIDAD_LATERAL = 100;      // Velocidad base para movimiento lateral
const unsigned long TIEMPO_MOVIMIENTO = 3000;  // 3 segundos cada dirección
const unsigned long PAUSA_ENTRE_MOVIMIENTOS = 500;

// =============================================================================
// CONFIGURACIÓN CINEMÁTICA OMNIDIRECCIONAL
// Los 3 motores deben trabajar coordinados para movimiento lateral puro
// Estos factores se ajustan según la geometría del robot
// =============================================================================
// Para movimiento lateral derecha: M1 hacia atrás, M2 hacia adelante, M3 según geometría
// Ajustar estos valores experimentalmente
float FACTOR_M1_LATERAL = 0.5;   // Factor para Motor 1 en movimiento lateral
float FACTOR_M2_LATERAL = 0.5;   // Factor para Motor 2 en movimiento lateral
float FACTOR_M3_LATERAL = 1.0;   // Factor para Motor 3 (rueda lateral principal)

// =============================================================================
// CONFIGURACIÓN DEL CONTROL PID (para mantener orientación)
// =============================================================================
float Kp = 4.0;    // Aumentado para mejor respuesta
float Ki = 0.1;
float Kd = 0.8;

const float MAX_CORRECCION = 60;
const float INTEGRAL_MAX = 40;

// =============================================================================
// DATA LOGGING
// =============================================================================
const int MAX_SAMPLES = 600;  // ~30 segundos a 50ms = 600 muestras
const int SAMPLE_INTERVAL = 50;  // Muestreo cada 50ms

struct DataSample {
  unsigned long timestamp;
  float heading;
  float accelX;
  float accelY;
  float accelZ;
  int velM1;
  int velM2;
  int velM3;
  int8_t dirM1;  // 1=adelante, -1=atrás, 0=parado
  int8_t dirM2;
  int8_t dirM3;
  char estado;   // 'D'=derecha, 'I'=izquierda, 'P'=pausa
};

DataSample dataLog[MAX_SAMPLES];
int sampleCount = 0;
unsigned long lastSampleTime = 0;
bool loggingEnabled = false;
unsigned long loggingStartTime = 0;

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
// VARIABLES DE MOTORES (para logging)
// =============================================================================
int currentVelM1 = 0, currentVelM2 = 0, currentVelM3 = 0;
int8_t currentDirM1 = 0, currentDirM2 = 0, currentDirM3 = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest {
  ESPERANDO_INICIO,
  TEST_HEADING,
  ESPERANDO_LOOP,
  LOOP_DERECHA,
  LOOP_PAUSA_1,
  LOOP_IZQUIERDA,
  LOOP_PAUSA_2,
  VOLCANDO_DATOS
};
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioTest = 0;
int cicloActual = 0;

// =============================================================================
// CONTROL DEL BOTÓN
// =============================================================================
bool botonAnterior = false;

// =============================================================================
// FUNCIÓN: inicializarGyro()
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
    Serial.println("   ADVERTENCIA: Calibración incompleta");
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
// FUNCIÓN: leerAceleracion()
// =============================================================================
void leerAceleracion(float &ax, float &ay, float &az) {
  imu::Vector<3> accel = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  ax = accel.x();
  ay = accel.y();
  az = accel.z();
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
// FUNCIÓN: setMotor1()
// =============================================================================
void setMotor1(int velocidad, int8_t direccion) {
  currentVelM1 = velocidad;
  currentDirM1 = direccion;
  
  analogWrite(PWM1, velocidad);
  if (direccion > 0) {
    digitalWrite(INA1, 1);
    digitalWrite(INB1, 0);
  } else if (direccion < 0) {
    digitalWrite(INA1, 0);
    digitalWrite(INB1, 1);
  } else {
    digitalWrite(INA1, 0);
    digitalWrite(INB1, 0);
  }
}

// =============================================================================
// FUNCIÓN: setMotor2()
// =============================================================================
void setMotor2(int velocidad, int8_t direccion) {
  currentVelM2 = velocidad;
  currentDirM2 = direccion;
  
  analogWrite(PWM2, velocidad);
  if (direccion > 0) {
    digitalWrite(INA2, 0);
    digitalWrite(INB2, 1);
  } else if (direccion < 0) {
    digitalWrite(INA2, 1);
    digitalWrite(INB2, 0);
  } else {
    digitalWrite(INA2, 0);
    digitalWrite(INB2, 0);
  }
}

// =============================================================================
// FUNCIÓN: setMotor3()
// =============================================================================
void setMotor3(int velocidad, int8_t direccion) {
  currentVelM3 = velocidad;
  currentDirM3 = direccion;
  
  analogWrite(PWM3, velocidad);
  if (direccion > 0) {
    digitalWrite(INA3, 1);
    digitalWrite(INB3, 0);
  } else if (direccion < 0) {
    digitalWrite(INA3, 0);
    digitalWrite(INB3, 1);
  } else {
    digitalWrite(INA3, 0);
    digitalWrite(INB3, 0);
  }
}

// =============================================================================
// FUNCIÓN: moverLateral()
// Movimiento lateral usando cinemática omnidireccional
// direccion: 1 = derecha, -1 = izquierda
// =============================================================================
void moverLateral(int velocidadBase, int direccion) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);
  
  // Cinemática omnidireccional para movimiento lateral
  // Motor 3 es el principal para movimiento lateral
  // Motores 1 y 2 compensan para mantener trayectoria recta
  
  int velM3 = (int)(velocidadBase * FACTOR_M3_LATERAL);
  int velM1 = (int)(velocidadBase * FACTOR_M1_LATERAL);
  int velM2 = (int)(velocidadBase * FACTOR_M2_LATERAL);
  
  // Aplicar corrección de heading
  // La corrección se suma/resta a M1 y M2 para corregir rotación
  velM1 = velM1 + (int)(correccion * 0.5);
  velM2 = velM2 - (int)(correccion * 0.5);
  
  // Constrains
  velM1 = constrain(abs(velM1), 0, 255);
  velM2 = constrain(abs(velM2), 0, 255);
  velM3 = constrain(velM3, 0, 255);
  
  if (direccion > 0) {
    // Derecha
    setMotor3(velM3, 1);
    // M1 y M2 ayudan a mantener la trayectoria recta
    // Ajustar signos según geometría real del robot
    setMotor1(velM1, -1);  // Hacia atrás
    setMotor2(velM2, -1);  // Hacia atrás
  } else {
    // Izquierda
    setMotor3(velM3, -1);
    setMotor1(velM1, 1);   // Hacia adelante
    setMotor2(velM2, 1);   // Hacia adelante
  }
}

// =============================================================================
// FUNCIÓN: parar()
// =============================================================================
void parar() {
  setMotor1(0, 0);
  setMotor2(0, 0);
  setMotor3(0, 0);
}

// =============================================================================
// FUNCIÓN: registrarMuestra()
// =============================================================================
void registrarMuestra(char estado) {
  if (!loggingEnabled || sampleCount >= MAX_SAMPLES) return;
  
  unsigned long ahora = millis();
  if (ahora - lastSampleTime < SAMPLE_INTERVAL) return;
  
  lastSampleTime = ahora;
  
  DataSample &s = dataLog[sampleCount];
  s.timestamp = ahora - loggingStartTime;
  s.heading = leerHeading();
  leerAceleracion(s.accelX, s.accelY, s.accelZ);
  s.velM1 = currentVelM1;
  s.velM2 = currentVelM2;
  s.velM3 = currentVelM3;
  s.dirM1 = currentDirM1;
  s.dirM2 = currentDirM2;
  s.dirM3 = currentDirM3;
  s.estado = estado;
  
  sampleCount++;
}

// =============================================================================
// FUNCIÓN: iniciarLogging()
// =============================================================================
void iniciarLogging() {
  sampleCount = 0;
  loggingEnabled = true;
  loggingStartTime = millis();
  lastSampleTime = 0;
  Serial.println(">>> LOGGING INICIADO <<<");
  Serial.print("    Capacidad: "); Serial.print(MAX_SAMPLES);
  Serial.print(" muestras ("); Serial.print(MAX_SAMPLES * SAMPLE_INTERVAL / 1000);
  Serial.println(" segundos)");
}

// =============================================================================
// FUNCIÓN: detenerLogging()
// =============================================================================
void detenerLogging() {
  loggingEnabled = false;
  Serial.println(">>> LOGGING DETENIDO <<<");
  Serial.print("    Muestras capturadas: "); Serial.println(sampleCount);
}

// =============================================================================
// FUNCIÓN: volcarDatosCSV()
// =============================================================================
void volcarDatosCSV() {
  Serial.println("\n\n========================================");
  Serial.println("=== INICIO DATOS CSV ===");
  Serial.println("========================================");
  Serial.println("Copiar desde aquí:");
  Serial.println();
  
  // Header
  Serial.println("timestamp_ms,heading_deg,accel_x,accel_y,accel_z,vel_m1,vel_m2,vel_m3,dir_m1,dir_m2,dir_m3,estado");
  
  // Data
  for (int i = 0; i < sampleCount; i++) {
    DataSample &s = dataLog[i];
    Serial.print(s.timestamp); Serial.print(",");
    Serial.print(s.heading, 2); Serial.print(",");
    Serial.print(s.accelX, 2); Serial.print(",");
    Serial.print(s.accelY, 2); Serial.print(",");
    Serial.print(s.accelZ, 2); Serial.print(",");
    Serial.print(s.velM1); Serial.print(",");
    Serial.print(s.velM2); Serial.print(",");
    Serial.print(s.velM3); Serial.print(",");
    Serial.print((int)s.dirM1); Serial.print(",");
    Serial.print((int)s.dirM2); Serial.print(",");
    Serial.print((int)s.dirM3); Serial.print(",");
    Serial.println(s.estado);
    
    // Pequeña pausa para no saturar el buffer serial
    if (i % 50 == 0) delay(10);
  }
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("=== FIN DATOS CSV ===");
  Serial.println("========================================");
  Serial.print("Total muestras: "); Serial.println(sampleCount);
  Serial.println("\nPresionar BOTON para reiniciar test");
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
      Serial.println("BOTON -> Test heading");
      break;
      
    case TEST_HEADING:
      Serial.println("Test heading activo");
      Serial.println("BOTON -> Iniciar loop lateral");
      break;
      
    case ESPERANDO_LOOP:
      Serial.println("BOTON -> Iniciar LOOP LATERAL + LOGGING");
      Serial.println("(Der 3s -> Izq 3s -> repite)");
      break;
      
    case VOLCANDO_DATOS:
      Serial.println("BOTON -> Reiniciar test");
      break;
      
    default:
      Serial.println("En movimiento... BOTON en pausa -> Volcar datos");
      break;
  }
  
  Serial.println("========================================\n");
}

// =============================================================================
// FUNCIÓN: imprimirConfiguracion()
// =============================================================================
void imprimirConfiguracion() {
  Serial.println("\n--- CONFIGURACION ACTUAL ---");
  Serial.print("Velocidad lateral: "); Serial.println(VELOCIDAD_LATERAL);
  Serial.print("Factores M1/M2/M3: ");
  Serial.print(FACTOR_M1_LATERAL); Serial.print("/");
  Serial.print(FACTOR_M2_LATERAL); Serial.print("/");
  Serial.println(FACTOR_M3_LATERAL);
  Serial.print("PID Kp/Ki/Kd: ");
  Serial.print(Kp); Serial.print("/");
  Serial.print(Ki); Serial.print("/");
  Serial.println(Kd);
  Serial.println("----------------------------\n");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST LATERAL CON DATA LOGGING                   *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Cinemática omnidireccional + PID                *");
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
  
  imprimirConfiguracion();
  imprimirMenu();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  static unsigned long ultimoPrint = 0;
  
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
    
    // ===== TEST HEADING =====
    case TEST_HEADING:
      if (millis() - ultimoPrint > 200) {
        float heading = leerHeading();
        float ax, ay, az;
        leerAceleracion(ax, ay, az);
        
        Serial.print("H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | Acc X:");
        Serial.print(ax, 1);
        Serial.print(" Y:");
        Serial.print(ay, 1);
        Serial.print(" Z:");
        Serial.println(az, 1);
        
        digitalWrite(LED_BUILTIN, abs(heading) < 10 ? HIGH : (millis() / 200) % 2);
        ultimoPrint = millis();
      }
      
      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = ESPERANDO_LOOP;
        imprimirMenu();
      }
      break;
    
    // ===== ESPERANDO LOOP =====
    case ESPERANDO_LOOP:
      if (botonPresionado()) {
        esperarSoltarBoton();
        cicloActual = 1;
        iniciarLogging();
        Serial.println("\n***** INICIANDO LOOP LATERAL *****");
        Serial.println(">>> CICLO 1: DERECHA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_DERECHA;
      }
      break;
    
    // ===== LOOP: DERECHA =====
    case LOOP_DERECHA:
      moverLateral(VELOCIDAD_LATERAL, 1);
      registrarMuestra('D');
      digitalWrite(LED_BUILTIN, HIGH);
      
      if (millis() - ultimoPrint > 200) {
        float heading = leerHeading();
        Serial.print("  [DER] H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | M1:");
        Serial.print(currentVelM1 * currentDirM1);
        Serial.print(" M2:");
        Serial.print(currentVelM2 * currentDirM2);
        Serial.print(" M3:");
        Serial.print(currentVelM3 * currentDirM3);
        Serial.print(" | t=");
        Serial.println((millis() - tiempoInicioTest) / 1000.0, 1);
        ultimoPrint = millis();
      }
      
      if (millis() - tiempoInicioTest >= TIEMPO_MOVIMIENTO) {
        parar();
        Serial.println("  -> Pausa (BOTON = volcar datos)");
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_1;
      }
      break;
    
    // ===== LOOP: PAUSA 1 =====
    case LOOP_PAUSA_1:
      parar();
      registrarMuestra('P');
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
      
      if (botonPresionado()) {
        esperarSoltarBoton();
        detenerLogging();
        volcarDatosCSV();
        estadoTest = VOLCANDO_DATOS;
        break;
      }
      
      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": IZQUIERDA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_IZQUIERDA;
      }
      break;
    
    // ===== LOOP: IZQUIERDA =====
    case LOOP_IZQUIERDA:
      moverLateral(VELOCIDAD_LATERAL, -1);
      registrarMuestra('I');
      digitalWrite(LED_BUILTIN, LOW);
      
      if (millis() - ultimoPrint > 200) {
        float heading = leerHeading();
        Serial.print("  [IZQ] H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | M1:");
        Serial.print(currentVelM1 * currentDirM1);
        Serial.print(" M2:");
        Serial.print(currentVelM2 * currentDirM2);
        Serial.print(" M3:");
        Serial.print(currentVelM3 * currentDirM3);
        Serial.print(" | t=");
        Serial.println((millis() - tiempoInicioTest) / 1000.0, 1);
        ultimoPrint = millis();
      }
      
      if (millis() - tiempoInicioTest >= TIEMPO_MOVIMIENTO) {
        parar();
        Serial.println("  -> Pausa (BOTON = volcar datos)");
        tiempoInicioTest = millis();
        estadoTest = LOOP_PAUSA_2;
      }
      break;
    
    // ===== LOOP: PAUSA 2 =====
    case LOOP_PAUSA_2:
      parar();
      registrarMuestra('P');
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
      
      if (botonPresionado()) {
        esperarSoltarBoton();
        detenerLogging();
        volcarDatosCSV();
        estadoTest = VOLCANDO_DATOS;
        break;
      }
      
      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        cicloActual++;
        Serial.println("\n>>> CICLO " + String(cicloActual) + ": DERECHA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_DERECHA;
      }
      break;
    
    // ===== VOLCANDO DATOS =====
    case VOLCANDO_DATOS:
      if (botonPresionado()) {
        esperarSoltarBoton();
        Serial.println("\n>>> REINICIANDO TEST <<<");
        sampleCount = 0;
        estadoTest = ESPERANDO_LOOP;
        imprimirMenu();
      }
      break;
  }
}
