// =============================================================================
// TEST: Dibujar un círculo sin girar (manteniendo heading con PID)
// Archivo: staging/shared/test-circulo/test-circulo.ino
//
// PROPÓSITO:
//   El robot dibuja un círculo sobre el piso SIN GIRAR (como un cangrejo
//   que camina en círculo siempre mirando al mismo lado).
//   Combina movimiento adelante/atrás + lateral cambiando la proporción
//   continuamente para trazar la trayectoria circular.
//
// CÓMO FUNCIONA:
//   Un ángulo (0° a 360°) va incrementándose con el tiempo.
//   En cada instante, la dirección de traslación del robot es:
//     - factor_derecha  = cos(ángulo)  → componente lateral
//     - factor_adelante = sin(ángulo)  → componente adelante/atrás
//   Esto hace que el robot se mueva: derecha → adelante → izquierda → atrás
//   trazando un círculo completo.
//   El PID del giróscopo mantiene el heading en 0° todo el tiempo.
//
// INSTRUCCIONES:
//   1. Subir al Teensy del ROBOT 2 (delantero)
//   2. Abrir Serial Monitor a 19200 baud
//   3. NO MOVER durante calibración (~5 segundos)
//   4. BOTON: test heading → BOTON: inicia círculo
//   5. Comandos Serial en tiempo real:
//      '+' / '-'  = más rápido / más lento (cambia tiempo del círculo)
//      'v'        = ver valores actuales
//      'd'        = cambiar sentido (horario / antihorario)
//      'p'        = pausar / reanudar
//
// VALORES TOMADOS DE:
//   - Lateral: VEL_M1=55, VEL_M2=55, VEL_M3=100 (test-motores-lateral-simple)
//   - Adelante/atrás: escalado a ~85 para igualar velocidad lateral
//   - PID: Kp=3.0, Ki=0.05, Kd=0.5, FACTOR_ROTACION=0.5 (del lateral)
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
// CONFIGURACIÓN DEL CÍRCULO
// =============================================================================
const long BAUD_RATE = 19200;

// Tiempo para completar UN círculo (milisegundos)
// Más largo = círculo más grande (el robot avanza más antes de cambiar dirección)
// Más corto = círculo más chico
unsigned long TIEMPO_CIRCULO = 5000;  // 10 segundos por vuelta

// Sentido: 1 = antihorario (der→adelante→izq→atrás), -1 = horario
int SENTIDO_CIRCULO = 1;

// Pausado
bool pausado = false;

// =============================================================================
// VELOCIDADES CALIBRADAS (de los programas que funcionan)
// =============================================================================

// Componente LATERAL (del test-motores-lateral-simple, calibrado por María)
int VEL_M1 = 55;
int VEL_M2 = 55;
int VEL_M3 = 100;

int DIR_M1 = -1;   // Dirección M1 para ir a la derecha
int DIR_M2 = 1;    // Dirección M2 para ir a la derecha
int DIR_M3 = 1;    // Dirección M3 para ir a la derecha

// Componente ADELANTE/ATRÁS
// Escalado para que la velocidad de traslación sea similar a la lateral.
// En lateral, M3 empuja a 100. Para adelante, M1+M2 empujan juntos.
// Por la geometría de 3 ruedas a 120°, M1 y M2 a ~85 cada uno producen
// una velocidad de traslación similar a M3 a 100.
int VELOCIDAD_CIRCULO = 85;  // Ajustable con Serial si el círculo sale ovalado

// =============================================================================
// SIGNOS DE ROTACIÓN (para corrección PID heading)
// =============================================================================
int ROT_M1 = 1;
int ROT_M2 = -1;
int ROT_M3 = 1;

// =============================================================================
// PID (del programa lateral, que usa los 3 motores)
// =============================================================================
float Kp = 3.0;
float Ki = 0.05;
float Kd = 0.5;

const float MAX_CORRECCION = 50;
const float INTEGRAL_MAX = 40;
float FACTOR_ROTACION = 0.5;

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
// VARIABLES DE DEBUG
// =============================================================================
float ultimoHeading = 0;
float ultimaCorreccion = 0;
float debugM1 = 0, debugM2 = 0, debugM3 = 0;
float anguloActual = 0;

// =============================================================================
// MÁQUINA DE ESTADOS
// =============================================================================
enum EstadoTest {
  ESPERANDO_INICIO,
  TEST_HEADING,
  ESPERANDO_CIRCULO,
  CIRCULO
};
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioCirculo = 0;
int vueltaActual = 0;

// =============================================================================
// CONTROL DEL BOTÓN
// =============================================================================
bool botonAnterior = false;

// =============================================================================
// FUNCIÓN: procesarComandoSerial()
// =============================================================================
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '+':
      case 'a':
        // Más rápido = tiempo más corto
        if (TIEMPO_CIRCULO > 2000) TIEMPO_CIRCULO -= 1000;
        Serial.print("  >> Tiempo círculo = ");
        Serial.print(TIEMPO_CIRCULO / 1000.0, 1);
        Serial.println("s (más rápido)");
        break;

      case '-':
      case 'z':
        // Más lento = tiempo más largo
        if (TIEMPO_CIRCULO < 30000) TIEMPO_CIRCULO += 1000;
        Serial.print("  >> Tiempo círculo = ");
        Serial.print(TIEMPO_CIRCULO / 1000.0, 1);
        Serial.println("s (más lento)");
        break;

      case 'd':
        SENTIDO_CIRCULO *= -1;
        Serial.print("  >> Sentido: ");
        Serial.println(SENTIDO_CIRCULO > 0 ? "ANTIHORARIO" : "HORARIO");
        break;

      case 'p':
        pausado = !pausado;
        Serial.println(pausado ? "  >> PAUSADO" : "  >> REANUDADO");
        break;

      case 'v':
        Serial.println("\n  === VALORES ACTUALES ===");
        Serial.print("  TIEMPO_CIRCULO = "); Serial.print(TIEMPO_CIRCULO / 1000.0, 1); Serial.println("s");
        Serial.print("  VELOCIDAD_CIRCULO = "); Serial.println(VELOCIDAD_CIRCULO);
        Serial.print("  SENTIDO = "); Serial.println(SENTIDO_CIRCULO > 0 ? "ANTIHORARIO" : "HORARIO");
        Serial.print("  VEL lateral M1/M2/M3 = "); Serial.print(VEL_M1);
        Serial.print("/"); Serial.print(VEL_M2);
        Serial.print("/"); Serial.println(VEL_M3);
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
// FUNCIÓN: moverCirculo()
//
// Esta es la función clave. Mezcla el movimiento lateral (calibrado) con
// el movimiento adelante/atrás según el ángulo actual en el círculo.
//
// ángulo = 0°:   puro DERECHA   → M1=-55, M2=+55, M3=+100
// ángulo = 90°:  puro ADELANTE  → M1=+85, M2=+85, M3=0
// ángulo = 180°: puro IZQUIERDA → M1=+55, M2=-55, M3=-100
// ángulo = 270°: puro ATRÁS     → M1=-85, M2=-85, M3=0
//
// Entre estos puntos, los valores se mezclan suavemente con sin/cos.
// =============================================================================
void moverCirculo(float anguloGrados) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);

  ultimoHeading = heading;
  ultimaCorreccion = correccion;

  float rad = anguloGrados * PI / 180.0;
  float factor_derecha  = cos(rad);  // 1=derecha, -1=izquierda
  float factor_adelante = sin(rad);  // 1=adelante, -1=atrás

  // ---- Componente LATERAL (del test lateral calibrado) ----
  // Cuando factor_derecha = 1: M1=-55, M2=+55, M3=+100 (derecha)
  // Cuando factor_derecha = -1: M1=+55, M2=-55, M3=-100 (izquierda)
  float lat_M1 = factor_derecha * (float)(DIR_M1 * VEL_M1);
  float lat_M2 = factor_derecha * (float)(DIR_M2 * VEL_M2);
  float lat_M3 = factor_derecha * (float)(DIR_M3 * VEL_M3);

  // ---- Componente ADELANTE/ATRÁS (del test básico) ----
  // Cuando factor_adelante = 1: M1=+85, M2=+85 (adelante)
  // Cuando factor_adelante = -1: M1=-85, M2=-85 (atrás)
  // M3 no participa en adelante/atrás (es la rueda trasera perpendicular)
  float fwd_M1 = factor_adelante * (float)VELOCIDAD_CIRCULO;
  float fwd_M2 = factor_adelante * (float)VELOCIDAD_CIRCULO;
  float fwd_M3 = 0;

  // ---- Sumar ambas componentes ----
  float total_M1 = lat_M1 + fwd_M1;
  float total_M2 = lat_M2 + fwd_M2;
  float total_M3 = lat_M3 + fwd_M3;

  // ---- Corrección PID de heading (los 3 motores participan) ----
  total_M1 += ROT_M1 * correccion * FACTOR_ROTACION;
  total_M2 += ROT_M2 * correccion * FACTOR_ROTACION;
  total_M3 += ROT_M3 * correccion * FACTOR_ROTACION;

  // ---- Saturación PROPORCIONAL ----
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

  // ---- Aplicar a motores ----
  int d1 = (total_M1 > 0) ? 1 : ((total_M1 < 0) ? -1 : 0);
  int d2 = (total_M2 > 0) ? 1 : ((total_M2 < 0) ? -1 : 0);
  int d3 = (total_M3 > 0) ? 1 : ((total_M3 < 0) ? -1 : 0);

  motor1((int)abs(total_M1), d1);
  motor2((int)abs(total_M2), d2);
  motor3((int)abs(total_M3), d3);
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
  Serial.println("*  TEST: DIBUJAR CIRCULO SIN GIRAR                 *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                      *");
  Serial.println("*  Combina lateral + adelante/atrás con sin/cos    *");
  Serial.println("*  PID mantiene heading en 0° todo el tiempo       *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("COMANDOS SERIAL:");
  Serial.println("  '+' = círculo más rápido (tiempo -1s)");
  Serial.println("  '-' = círculo más lento (tiempo +1s)");
  Serial.println("  'd' = cambiar sentido (horario/antihorario)");
  Serial.println("  'p' = pausar / reanudar");
  Serial.println("  'v' = ver valores actuales");
  Serial.println();
  Serial.print("TIEMPO_CIRCULO = "); Serial.print(TIEMPO_CIRCULO / 1000.0, 1); Serial.println("s");
  Serial.print("VELOCIDAD_CIRCULO = "); Serial.println(VELOCIDAD_CIRCULO);
  Serial.print("VEL lateral M1/M2/M3 = "); Serial.print(VEL_M1);
  Serial.print("/"); Serial.print(VEL_M2);
  Serial.print("/"); Serial.println(VEL_M3);
  Serial.println();

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

  procesarComandoSerial();

  switch (estadoTest) {

    // ===== ESPERANDO INICIO =====
    case ESPERANDO_INICIO:
      if (botonPresionado()) {
        esperarSoltarBoton();
        estadoTest = TEST_HEADING;
        Serial.println("\n>>> TEST HEADING: girar robot manualmente <<<");
        Serial.println(">>> BOTON para iniciar círculo <<<\n");
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
        estadoTest = ESPERANDO_CIRCULO;
        Serial.println("\n========================================");
        Serial.println("PRESIONAR BOTON para iniciar CIRCULO");
        Serial.print("Tiempo por vuelta: ");
        Serial.print(TIEMPO_CIRCULO / 1000.0, 1);
        Serial.println("s");
        Serial.print("Sentido: ");
        Serial.println(SENTIDO_CIRCULO > 0 ? "ANTIHORARIO" : "HORARIO");
        Serial.println("========================================\n");
      }
      break;

    // ===== ESPERANDO CÍRCULO =====
    case ESPERANDO_CIRCULO:
      if (botonPresionado()) {
        esperarSoltarBoton();
        vueltaActual = 1;
        Serial.println("\n***** INICIANDO CIRCULO *****");
        Serial.println(">>> VUELTA 1 <<<");
        resetPID();
        tiempoInicioCirculo = millis();
        estadoTest = CIRCULO;
      }
      break;

    // ===== CÍRCULO =====
    case CIRCULO:
    {
      if (pausado) {
        parar();
        digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
        break;
      }

      // Calcular ángulo actual en el círculo (0° a 360°)
      unsigned long tiempoEnCirculo = millis() - tiempoInicioCirculo;
      float progreso = (float)(tiempoEnCirculo % TIEMPO_CIRCULO) / (float)TIEMPO_CIRCULO;
      anguloActual = progreso * 360.0 * (float)SENTIDO_CIRCULO;

      // Detectar vuelta completa
      int vueltaCalculada = (tiempoEnCirculo / TIEMPO_CIRCULO) + 1;
      if (vueltaCalculada > vueltaActual) {
        vueltaActual = vueltaCalculada;
        Serial.println("\n>>> VUELTA " + String(vueltaActual) + " <<<");
      }

      // Ejecutar movimiento circular
      moverCirculo(anguloActual);

      // LED indica cuadrante: encendido = adelante/derecha, apagado = atrás/izquierda
      float angNorm = fmod(abs(anguloActual), 360.0);
      digitalWrite(LED_BUILTIN, (angNorm < 180) ? HIGH : LOW);

      // Debug cada 200ms
      if (millis() - ultimoPrint > 200) {
        Serial.print("  [CIRCULO] ang:");
        Serial.print(anguloActual, 0);
        Serial.print("° H:");
        if (ultimoHeading >= 0) Serial.print("+");
        Serial.print(ultimoHeading, 1);
        Serial.print("° M1:");
        Serial.print((int)debugM1);
        Serial.print(" M2:");
        Serial.print((int)debugM2);
        Serial.print(" M3:");
        Serial.print((int)debugM3);
        Serial.print(" | vuelta ");
        Serial.println(vueltaActual);
        ultimoPrint = millis();
      }
    }
      break;
  }
}
