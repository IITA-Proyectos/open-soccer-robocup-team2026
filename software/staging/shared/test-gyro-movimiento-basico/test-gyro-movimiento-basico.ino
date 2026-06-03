// ============================================================================
// ⚠️ STAGING CONGELADO (2026-06-03) — NO subir más material a software/staging/.
// Antes de tocar o agregar algo acá, LEÉ:
//   software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md
// Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
//   software/teensy/Soccer 2026/src/  (ver el "mapa de reúso" del documento).
// ============================================================================

// =============================================================================
// TEST: Inicialización de giróscopo BNO055 y movimiento básico adelante/atrás
// Archivo: staging/shared/test-gyro-movimiento-basico.ino
// 
// PROPÓSITO: 
//   - Verificar inicialización correcta del giróscopo BNO055
//   - El giróscopo debe arrancar y establecer la posición cero
//   - Probar movimiento adelante y atrás en línea recta CON CORRECCIÓN PID
//   - Loop infinito: adelante -> atrás -> adelante -> atrás...
//
// INSTRUCCIONES:
//   1. Subir este programa al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER el robot durante la calibración inicial (~5 segundos)
//   4. Usar BOTON (pin 9) para avanzar entre los tests:
//      - Test 1: Verificar heading (girar robot manualmente)
//      - Test 2: Loop infinito adelante/atrás (5 seg cada uno)
//   5. El robot va a ir adelante 5 seg, atrás 5 seg, y repetir
//
// BASADO EN: definitivo-delantero (ROBOT2)
// NO USA zirconLib - pines definidos directamente
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
const int VELOCIDAD_BASE = 150;// 100 en prueva 
const unsigned long TIEMPO_MOVIMIENTO = 5000;  // 5 segundos cada dirección
const unsigned long PAUSA_ENTRE_MOVIMIENTOS = 500;  // Pausa de 0.5 seg entre cambios

// =============================================================================
// CONFIGURACIÓN DEL CONTROL PID
// =============================================================================
float Kp = 3.0;
float Ki = 0.08;
float Kd = 0.8;

const float MAX_CORRECCION = 80;// en pruebas 80
const float INTEGRAL_MAX = 50;

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
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest {
  ESPERANDO_INICIO,
  TEST_HEADING,
  ESPERANDO_LOOP,
  LOOP_ADELANTE,
  LOOP_PAUSA_1,
  LOOP_ATRAS,
  LOOP_PAUSA_2
};
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioTest = 0;
int cicloActual = 0;  // Contador de ciclos

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
// FUNCIÓN: moverAdelante()
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
// FUNCIÓN: moverAtras()
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
// FUNCIÓN: parar()
// =============================================================================
void parar() {
  analogWrite(PWM1, 0);
  digitalWrite(INA1, 0);
  digitalWrite(INB1, 0);
  
  analogWrite(PWM2, 0);
  digitalWrite(INA2, 0);
  digitalWrite(INB2, 0);
  
  analogWrite(PWM3, 0);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 0);
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
      Serial.println("(Adelante 5s -> Atrás 5s -> repetir)");
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
  Serial.println("*  TEST: GIROSCOPO + MOVIMIENTO CON PID (LOOP)     *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Tiempo: 5 segundos cada dirección               *");
  Serial.println("*  Modo: LOOP INFINITO (adelante/atrás)            *");
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
  
  imprimirMenu();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  static unsigned long ultimoHeadingPrint = 0;
  
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
        Serial.println("\n***** INICIANDO LOOP INFINITO *****");
        Serial.println(">>> CICLO 1: ADELANTE (5 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ADELANTE;
      }
      break;
    
    // ===== LOOP: ADELANTE =====
    case LOOP_ADELANTE:
      moverAdelante(VELOCIDAD_BASE);
      digitalWrite(LED_BUILTIN, HIGH);  // LED fijo = adelante
      
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
    
    // ===== LOOP: PAUSA 1 =====
    case LOOP_PAUSA_1:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);  // Parpadeo rápido = pausa
      
      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": ATRAS (5 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ATRAS;
      }
      break;
    
    // ===== LOOP: ATRAS =====
    case LOOP_ATRAS:
      moverAtras(VELOCIDAD_BASE);
      digitalWrite(LED_BUILTIN, LOW);  // LED apagado = atrás
      
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
    
    // ===== LOOP: PAUSA 2 (y volver a empezar) =====
    case LOOP_PAUSA_2:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
      
      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        cicloActual++;
        Serial.println("\n>>> CICLO " + String(cicloActual) + ": ADELANTE (5 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_ADELANTE;  // Vuelve a adelante -> LOOP INFINITO
      }
      break;
  }
}
