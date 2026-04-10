// =============================================================================
// TEST: Avance rápido en línea recta (2 segundos, rampa de velocidad)
// Archivo: staging/shared/test-avance-rapido/test-avance-rapido.ino
//
// PROPÓSITO:
//   Avanzar lo más rápido posible durante 2 segundos manteniendo la línea
//   recta con PID de heading. Usa rampa de aceleración para no patinar
//   y corrección PID reducida a alta velocidad para no oscilar.
//
// INSTRUCCIONES:
//   1. Subir al Teensy del ROBOT 1 (arquero) o ROBOT 2 (cambiar pines)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER durante calibración (~5 segundos)
//   4. BOTON (pin 9):
//      - Presión 1: Test heading (verificar giroscopio)
//      - Presión 2: Preparar avance
//      - Presión 3: AVANCE RÁPIDO 2 segundos
//      - Presión 4: Repetir
//
// ESTRATEGIA DE VELOCIDAD:
//   - Rampa: 60 → 230 en 400ms (sube linealmente)
//   - Crucero: 230 PWM durante ~1.6 segundos
//   - Freno: para en seco al terminar
//   - PID: corrección BAJA a alta velocidad (máx ±25) para no oscilar
//     A vel 230, los motores van entre 205 y 240 → seguro y estable
//
// ROBOT 1 (arquero) - NO USA zirconLib
// Para ROBOT 2 (delantero): cambiar pines a M1=8/7/6, M2=11/12/4, M3=2/5/3
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 1 (arquero)
// Para ROBOT 2: M1=8/7/6, M2=11/12/4, M3=2/5/3
// =============================================================================
#define INA1 2
#define INB1 5
#define PWM1 3

#define INA2 8
#define INB2 7
#define PWM2 6

#define INA3 11
#define INB3 12
#define PWM3 4

#define PIN_BOTON 9

// =============================================================================
// CONFIGURACIÓN DE AVANCE RÁPIDO
// =============================================================================
const long BAUD_RATE = 19200;

// Velocidad máxima de crucero (PWM). 230 es rápido y deja margen de corrección.
// NO subir a más de 240 (los motores se pueden quemar a 250+).
const int VELOCIDAD_CRUCERO = 230;

// Tiempo total de avance (milisegundos)
const unsigned long TIEMPO_AVANCE = 2000;

// Rampa de aceleración
const unsigned long TIEMPO_RAMPA = 400;       // ms para llegar a velocidad crucero
const int VELOCIDAD_MINIMA_RAMPA = 60;        // PWM inicial (para que arranque sin saltar)

// =============================================================================
// PID — Parámetros para alta velocidad
// =============================================================================
// A alta velocidad la corrección tiene que ser CHICA.
// Si es grande, un motor sube a 255 y el otro baja mucho → el robot oscila.
// Con corrección máxima de 25, a vel 230 los motores van entre 205 y 240.
float Kp = 4.5;      // Subido: el robot curvaba, necesita reaccionar fuerte
float Ki = 0.05;     // Igual
float Kd = 0.8;      // Subido: para frenar la corrección antes de pasarse

const float MAX_CORRECCION = 50;    // Subido de 35: a vel 230, motores van entre 180-240
const float INTEGRAL_MAX = 25;      // Igual

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
  ESPERANDO_AVANCE,
  AVANZANDO,
  TERMINADO
};
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioAvance = 0;
int intentoActual = 0;

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
// FUNCIÓN: moverAdelanteRapido()
//
// Avanza con rampa + PID suave.
// La velocidad sube linealmente durante TIEMPO_RAMPA y después se mantiene.
// La corrección PID es chica (±25 máx) para no desestabilizar a alta vel.
// =============================================================================
void moverAdelanteRapido(unsigned long tiempoTranscurrido) {
  // --- Calcular velocidad actual (rampa) ---
  int velocidadActual;

  if (tiempoTranscurrido < TIEMPO_RAMPA) {
    // Fase de rampa: subir linealmente de VELOCIDAD_MINIMA_RAMPA a VELOCIDAD_CRUCERO
    float progreso = (float)tiempoTranscurrido / (float)TIEMPO_RAMPA;  // 0.0 a 1.0
    velocidadActual = VELOCIDAD_MINIMA_RAMPA + (int)((VELOCIDAD_CRUCERO - VELOCIDAD_MINIMA_RAMPA) * progreso);
  } else {
    // Fase de crucero: velocidad máxima constante
    velocidadActual = VELOCIDAD_CRUCERO;
  }

  // --- Corrección PID ---
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  // --- Aplicar velocidad + corrección a M1 y M2 ---
  int velM1 = velocidadActual + (int)correccion;
  int velM2 = velocidadActual - (int)correccion;

  // Limitar a rango seguro: nunca pasar de 240, nunca bajar de 0
  velM1 = constrain(velM1, 0, 240);
  velM2 = constrain(velM2, 0, 240);

  // M1: adelante
  analogWrite(PWM1, velM1);
  digitalWrite(INA1, 1);
  digitalWrite(INB1, 0);

  // M2: adelante
  analogWrite(PWM2, velM2);
  digitalWrite(INA2, 0);
  digitalWrite(INB2, 1);

  // M3: apagado (no contribuye al avance recto)
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
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);

  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST: AVANCE RAPIDO (2 seg, rampa + PID)        *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Vel crucero: 230 PWM                            *");
  Serial.println("*  Rampa: 60 -> 230 en 400ms                       *");
  Serial.println("*  PID suave: Kp=2.5 Ki=0.03 Kd=0.6 max=25        *");
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

  Serial.println("\n========================================");
  Serial.println("PRESIONAR BOTON para TEST HEADING");
  Serial.println("========================================\n");
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
        Serial.println("\n>>> TEST HEADING: girar robot manualmente <<<");
        Serial.println(">>> BOTON para preparar avance rápido <<<\n");
      }
      break;

    // ===== TEST HEADING =====
    case TEST_HEADING:
      if (millis() - ultimoPrint > 200) {
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
        ultimoPrint = millis();
      }

      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = ESPERANDO_AVANCE;
        Serial.println("\n========================================");
        Serial.println("LISTO PARA AVANCE RAPIDO");
        Serial.print("  Vel crucero: "); Serial.print(VELOCIDAD_CRUCERO); Serial.println(" PWM");
        Serial.print("  Tiempo: "); Serial.print(TIEMPO_AVANCE / 1000.0, 1); Serial.println(" seg");
        Serial.print("  Rampa: "); Serial.print(TIEMPO_RAMPA); Serial.println("ms");
        Serial.println("");
        Serial.println("PRESIONAR BOTON para ARRANCAR");
        Serial.println("  (poner el robot en posicion primero!)");
        Serial.println("========================================\n");
      }
      break;

    // ===== ESPERANDO AVANCE =====
    case ESPERANDO_AVANCE:
      if (botonPresionado()) {
        esperarSoltarBoton();
        intentoActual++;
        Serial.print("\n***** AVANCE RAPIDO #"); Serial.print(intentoActual); Serial.println(" *****");
        Serial.println(">>> ARRANCANDO! <<<\n");
        resetPID();
        tiempoInicioAvance = millis();
        estadoTest = AVANZANDO;
      }
      break;

    // ===== AVANZANDO =====
    case AVANZANDO:
    {
      unsigned long tiempoTranscurrido = millis() - tiempoInicioAvance;

      // Ejecutar movimiento
      moverAdelanteRapido(tiempoTranscurrido);
      digitalWrite(LED_BUILTIN, HIGH);

      // Debug cada 100ms (más frecuente por ser rápido)
      if (millis() - ultimoPrint > 100) {
        float heading = leerHeading();

        // Calcular velocidad actual para mostrar
        int velMostrar;
        if (tiempoTranscurrido < TIEMPO_RAMPA) {
          float progreso = (float)tiempoTranscurrido / (float)TIEMPO_RAMPA;
          velMostrar = VELOCIDAD_MINIMA_RAMPA + (int)((VELOCIDAD_CRUCERO - VELOCIDAD_MINIMA_RAMPA) * progreso);
        } else {
          velMostrar = VELOCIDAD_CRUCERO;
        }

        Serial.print("  [");
        if (tiempoTranscurrido < TIEMPO_RAMPA) {
          Serial.print("RAMPA");
        } else {
          Serial.print("CRUCE");
        }
        Serial.print("] vel:");
        Serial.print(velMostrar);
        Serial.print(" H:");
        if (heading >= 0) Serial.print("+");
        Serial.print(heading, 1);
        Serial.print(" t=");
        Serial.print(tiempoTranscurrido / 1000.0, 2);
        Serial.println("s");

        ultimoPrint = millis();
      }

      // ¿Terminó?
      if (tiempoTranscurrido >= TIEMPO_AVANCE) {
        parar();
        float headingFinal = leerHeading();
        Serial.println("\n  >>> TERMINADO <<<");
        Serial.print("  Heading final: ");
        if (headingFinal >= 0) Serial.print("+");
        Serial.print(headingFinal, 1);
        Serial.println("°");
        Serial.print("  Tiempo real: ");
        Serial.print((millis() - tiempoInicioAvance) / 1000.0, 2);
        Serial.println("s");
        Serial.println("");
        Serial.println("  BOTON para repetir");
        Serial.println("");
        digitalWrite(LED_BUILTIN, LOW);
        estadoTest = TERMINADO;
      }
    }
      break;

    // ===== TERMINADO =====
    case TERMINADO:
      // Parpadeo lento = terminado
      digitalWrite(LED_BUILTIN, (millis() / 500) % 2);

      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = ESPERANDO_AVANCE;
        Serial.println("\n========================================");
        Serial.println("PRESIONAR BOTON para nuevo avance");
        Serial.println("========================================\n");
      }
      break;
  }
}
