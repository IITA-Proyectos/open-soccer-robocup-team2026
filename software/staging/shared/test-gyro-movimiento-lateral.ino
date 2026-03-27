// =============================================================================
// TEST: Movimiento lateral (estilo metegol) con corrección de heading
// Archivo: staging/shared/test-gyro-movimiento-lateral.ino
// 
// PROPÓSITO: 
//   - El robot se mueve hacia los costados manteniendo la orientación
//   - Como un jugador de metegol: mira al frente, se desplaza lateralmente
//   - Usa el Motor 3 para movimiento lateral
//   - Usa Motores 1 y 2 para corregir heading con PID
//   - Loop infinito: derecha -> izquierda -> derecha -> izquierda...
//
// INSTRUCCIONES:
//   1. Subir este programa al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER el robot durante la calibración inicial (~5 segundos)
//   4. Usar BOTON (pin 9) para avanzar entre los tests:
//      - Test 1: Verificar heading (girar robot manualmente)
//      - Test 2: Loop infinito derecha/izquierda (3 seg cada uno)
//   5. El robot va a ir a la derecha 3 seg, izquierda 3 seg, y repetir
//
// BASADO EN: test-gyro-movimiento-basico.ino
// NO USA zirconLib - pines definidos directamente
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// CONFIGURACIÓN DE PINES - ROBOT 2 (delantero)
// =============================================================================
// Motores 1 y 2: usados para corregir heading (mantener orientación)
#define INA1 8
#define INB1 7
#define PWM1 6

#define INA2 11
#define INB2 12
#define PWM2 4

// Motor 3: movimiento lateral
#define INA3 2
#define INB3 5
#define PWM3 3

#define PIN_BOTON 9

// =============================================================================
// CONFIGURACIÓN DE MOVIMIENTO
// =============================================================================
const long BAUD_RATE = 19200;
const int VELOCIDAD_LATERAL = 120;      // Velocidad del motor lateral
const int VELOCIDAD_CORRECCION = 60;    // Velocidad base para corrección de heading
const unsigned long TIEMPO_MOVIMIENTO = 3000;  // 3 segundos cada dirección
const unsigned long PAUSA_ENTRE_MOVIMIENTOS = 500;  // Pausa de 0.5 seg

// =============================================================================
// CONFIGURACIÓN DEL CONTROL PID (para mantener orientación)
// =============================================================================
float Kp = 3.0;
float Ki = 0.05;
float Kd = 0.5;

const float MAX_CORRECCION = 80;
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
  LOOP_DERECHA,
  LOOP_PAUSA_1,
  LOOP_IZQUIERDA,
  LOOP_PAUSA_2
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

  Serial.println("5. Estableciendo posición cero (FRENTE)...");
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
  Serial.println("El robot mirará siempre hacia esta dirección (heading=0)");
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
// FUNCIÓN: aplicarCorreccionHeading()
// Usa motores 1 y 2 para corregir la orientación mientras se mueve lateral
// =============================================================================
void aplicarCorreccionHeading(float correccion) {
  // Si la corrección es significativa, usar motores 1 y 2 para corregir
  if (abs(correccion) > 5) {
    int velCorreccion = (int)abs(correccion);
    velCorreccion = constrain(velCorreccion, 0, VELOCIDAD_CORRECCION);
    
    if (correccion > 0) {
      // Girar hacia la derecha (corregir desviación a la izquierda)
      // Motor 1 adelante, Motor 2 atrás (o viceversa según config)
      analogWrite(PWM1, velCorreccion);
      digitalWrite(INA1, 1);
      digitalWrite(INB1, 0);
      
      analogWrite(PWM2, velCorreccion);
      digitalWrite(INA2, 1);
      digitalWrite(INB2, 0);
    } else {
      // Girar hacia la izquierda (corregir desviación a la derecha)
      analogWrite(PWM1, velCorreccion);
      digitalWrite(INA1, 0);
      digitalWrite(INB1, 1);
      
      analogWrite(PWM2, velCorreccion);
      digitalWrite(INA2, 0);
      digitalWrite(INB2, 1);
    }
  } else {
    // Sin corrección necesaria - motores 1 y 2 apagados
    analogWrite(PWM1, 0);
    digitalWrite(INA1, 0);
    digitalWrite(INB1, 0);
    
    analogWrite(PWM2, 0);
    digitalWrite(INA2, 0);
    digitalWrite(INB2, 0);
  }
}

// =============================================================================
// FUNCIÓN: moverDerecha()
// Motor 3 mueve lateralmente, Motores 1-2 corrigen heading
// =============================================================================
void moverDerecha(int velocidad) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);
  
  // Motor 3: movimiento lateral hacia la derecha
  analogWrite(PWM3, velocidad);
  digitalWrite(INA3, 1);
  digitalWrite(INB3, 0);
  
  // Motores 1 y 2: corrección de heading
  aplicarCorreccionHeading(correccion);
}

// =============================================================================
// FUNCIÓN: moverIzquierda()
// Motor 3 mueve lateralmente, Motores 1-2 corrigen heading
// =============================================================================
void moverIzquierda(int velocidad) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);
  
  // Motor 3: movimiento lateral hacia la izquierda
  analogWrite(PWM3, velocidad);
  digitalWrite(INA3, 0);
  digitalWrite(INB3, 1);
  
  // Motores 1 y 2: corrección de heading
  aplicarCorreccionHeading(correccion);
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
      Serial.println("Verificar que heading=0 es el FRENTE");
      Serial.println("PRESIONAR BOTON para iniciar LOOP LATERAL");
      break;
      
    case ESPERANDO_LOOP:
      Serial.println("PRESIONAR BOTON para iniciar LOOP");
      Serial.println("(Derecha 3s -> Izquierda 3s -> repetir)");
      Serial.println("El robot mirará siempre al frente!");
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
  Serial.println("*  TEST: MOVIMIENTO LATERAL (ESTILO METEGOL)       *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Tiempo: 3 segundos cada dirección               *");
  Serial.println("*  Modo: LOOP INFINITO (derecha/izquierda)         *");
  Serial.println("*  Orientación: Siempre mirando al frente          *");
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
        Serial.println("Girar robot para verificar heading");
        Serial.println("heading=0 debe ser el FRENTE del robot");
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
        Serial.print("° ");
        
        // Indicador visual de dirección
        if (heading > 10) {
          Serial.println(" <-- Robot girado a la DERECHA");
        } else if (heading < -10) {
          Serial.println(" --> Robot girado a la IZQUIERDA");
        } else {
          Serial.println(" [FRENTE OK]");
        }
        
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
        Serial.println("\n***** INICIANDO LOOP LATERAL (METEGOL) *****");
        Serial.println(">>> CICLO 1: DERECHA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_DERECHA;
      }
      break;
    
    // ===== LOOP: DERECHA =====
    case LOOP_DERECHA:
      moverDerecha(VELOCIDAD_LATERAL);
      digitalWrite(LED_BUILTIN, HIGH);  // LED fijo = derecha
      
      if (millis() - ultimoHeadingPrint > 200) {
        float heading = leerHeading();
        float correccion = calcularCorreccionPID(heading);
        Serial.print("  [DERECHA]  H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | Corr:");
        Serial.print(correccion, 1);
        Serial.print(" | t=");
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
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);  // Parpadeo rápido
      
      if (millis() - tiempoInicioTest >= PAUSA_ENTRE_MOVIMIENTOS) {
        Serial.println(">>> CICLO " + String(cicloActual) + ": IZQUIERDA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_IZQUIERDA;
      }
      break;
    
    // ===== LOOP: IZQUIERDA =====
    case LOOP_IZQUIERDA:
      moverIzquierda(VELOCIDAD_LATERAL);
      digitalWrite(LED_BUILTIN, LOW);  // LED apagado = izquierda
      
      if (millis() - ultimoHeadingPrint > 200) {
        float heading = leerHeading();
        float correccion = calcularCorreccionPID(heading);
        Serial.print("  [IZQUIERDA] H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print("° | Corr:");
        Serial.print(correccion, 1);
        Serial.print(" | t=");
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
        Serial.println("\n>>> CICLO " + String(cicloActual) + ": DERECHA (3 seg) <<<");
        resetPID();
        tiempoInicioTest = millis();
        estadoTest = LOOP_DERECHA;  // Vuelve a derecha -> LOOP INFINITO
      }
      break;
  }
}
