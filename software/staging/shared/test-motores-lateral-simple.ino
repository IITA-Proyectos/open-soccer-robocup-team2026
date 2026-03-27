// =============================================================================
// TEST: Movimiento lateral con CINEMÁTICA OMNIDIRECCIONAL 3 RUEDAS 120°
// Archivo: staging/shared/test-motores-lateral-simple.ino
// 
// PROPÓSITO: 
//   - Movimiento lateral usando cinemática correcta de 3 ruedas a 120°
//   - Los 3 motores trabajan simultáneamente con diferentes potencias
//   - SIN giróscopo, SIN PID - solo para probar la cinemática
//
// CINEMÁTICA:
//   Para robot con ruedas a 30°, 150°, 270° (configuración estándar):
//   - Movimiento DERECHA: M1=-0.5, M2=-0.5, M3=+1.0
//   - Movimiento IZQUIERDA: M1=+0.5, M2=+0.5, M3=-1.0
//
// CONTROL por Serial:
//   'p' = PARAR
//   'r' = REANUDAR
//   '1'-'9' = Cambiar velocidad (1=50 ... 9=250)
//   'a' = Probar configuración A (30°, 150°, 270°)
//   'b' = Probar configuración B (90°, 210°, 330°)
//   'c' = Probar configuración C (0°, 120°, 240°)
//
// ROBOT 2 (delantero)
// =============================================================================

#include <Arduino.h>

// =============================================================================
// PINES DE MOTORES - ROBOT 2
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
// CONFIGURACIÓN
// =============================================================================
const long BAUD_RATE = 19200;
int VELOCIDAD = 100;
const unsigned long TIEMPO_MOVIMIENTO = 3000;
const unsigned long TIEMPO_PAUSA = 500;

// =============================================================================
// ÁNGULOS DE LAS RUEDAS (en grados)
// Ajustar según la geometría real del robot
// =============================================================================
// Configuración A: Rueda frontal arriba, dos ruedas atrás
float ANGULO_M1 = 30.0;   // Motor 1: frente-derecha
float ANGULO_M2 = 150.0;  // Motor 2: frente-izquierda  
float ANGULO_M3 = 270.0;  // Motor 3: atrás (abajo)

// Factores calculados (se actualizan con calcularFactores())
float factorM1_lateral = 0;
float factorM2_lateral = 0;
float factorM3_lateral = 0;

// =============================================================================
// VARIABLES
// =============================================================================
bool enMovimiento = true;
bool yendoDerecha = true;
unsigned long tiempoInicio = 0;
bool enPausa = false;

// =============================================================================
// FUNCIONES MATEMÁTICAS
// =============================================================================
float gradosARadianes(float grados) {
  return grados * PI / 180.0;
}

// Calcula los factores de velocidad para cada motor basado en los ángulos
void calcularFactores() {
  // Para movimiento lateral (Vx=1, Vy=0, omega=0):
  // V_rueda = -sin(theta) * Vx
  factorM1_lateral = -sin(gradosARadianes(ANGULO_M1));
  factorM2_lateral = -sin(gradosARadianes(ANGULO_M2));
  factorM3_lateral = -sin(gradosARadianes(ANGULO_M3));
  
  // Normalizar para que el máximo sea 1.0
  float maxFactor = max(abs(factorM1_lateral), max(abs(factorM2_lateral), abs(factorM3_lateral)));
  if (maxFactor > 0) {
    factorM1_lateral /= maxFactor;
    factorM2_lateral /= maxFactor;
    factorM3_lateral /= maxFactor;
  }
}

// =============================================================================
// FUNCIONES DE MOTORES
// =============================================================================
void motor1(int vel, int dir) {
  vel = constrain(vel, 0, 255);
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
  vel = constrain(vel, 0, 255);
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
  vel = constrain(vel, 0, 255);
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
// MOVIMIENTO LATERAL CON CINEMÁTICA
// =============================================================================
void moverLateral(int direccion) {
  // direccion: 1 = derecha, -1 = izquierda
  
  // Calcular velocidades usando los factores de cinemática
  float velM1 = factorM1_lateral * direccion * VELOCIDAD;
  float velM2 = factorM2_lateral * direccion * VELOCIDAD;
  float velM3 = factorM3_lateral * direccion * VELOCIDAD;
  
  // Aplicar a los motores
  int dirM1 = (velM1 >= 0) ? 1 : -1;
  int dirM2 = (velM2 >= 0) ? 1 : -1;
  int dirM3 = (velM3 >= 0) ? 1 : -1;
  
  motor1((int)abs(velM1), dirM1);
  motor2((int)abs(velM2), dirM2);
  motor3((int)abs(velM3), dirM3);
}

// =============================================================================
// CONFIGURACIONES PREDEFINIDAS
// =============================================================================
void configuracionA() {
  // Configuración A: 30°, 150°, 270°
  // Rueda frontal-derecha, frontal-izquierda, trasera
  ANGULO_M1 = 30.0;
  ANGULO_M2 = 150.0;
  ANGULO_M3 = 270.0;
  calcularFactores();
  imprimirConfiguracion();
}

void configuracionB() {
  // Configuración B: 90°, 210°, 330°
  // Rueda frontal, trasera-izquierda, trasera-derecha
  ANGULO_M1 = 90.0;
  ANGULO_M2 = 210.0;
  ANGULO_M3 = 330.0;
  calcularFactores();
  imprimirConfiguracion();
}

void configuracionC() {
  // Configuración C: 0°, 120°, 240°
  // Rueda derecha, trasera-izquierda, trasera-derecha
  ANGULO_M1 = 0.0;
  ANGULO_M2 = 120.0;
  ANGULO_M3 = 240.0;
  calcularFactores();
  imprimirConfiguracion();
}

void imprimirConfiguracion() {
  Serial.println("\n--- CONFIGURACION CINEMATICA ---");
  Serial.print("Angulos: M1="); Serial.print(ANGULO_M1, 0);
  Serial.print("° M2="); Serial.print(ANGULO_M2, 0);
  Serial.print("° M3="); Serial.print(ANGULO_M3, 0);
  Serial.println("°");
  
  Serial.println("Factores para movimiento DERECHA:");
  Serial.print("  M1: "); Serial.print(factorM1_lateral, 3);
  Serial.print(" -> vel="); Serial.println((int)(factorM1_lateral * VELOCIDAD));
  Serial.print("  M2: "); Serial.print(factorM2_lateral, 3);
  Serial.print(" -> vel="); Serial.println((int)(factorM2_lateral * VELOCIDAD));
  Serial.print("  M3: "); Serial.print(factorM3_lateral, 3);
  Serial.print(" -> vel="); Serial.println((int)(factorM3_lateral * VELOCIDAD));
  Serial.println("--------------------------------\n");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST CINEMATICA OMNIDIRECCIONAL 3 RUEDAS 120°   *");
  Serial.println("*  Los 3 motores trabajan simultaneamente          *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("COMANDOS por Serial:");
  Serial.println("  'p' = PARAR");
  Serial.println("  'r' = REANUDAR");
  Serial.println("  '1'-'9' = Velocidad (1=50 ... 9=250)");
  Serial.println("  'a' = Config A: 30°, 150°, 270°");
  Serial.println("  'b' = Config B: 90°, 210°, 330°");
  Serial.println("  'c' = Config C: 0°, 120°, 240°");
  Serial.println();
  
  // Configurar pines
  pinMode(LED_BUILTIN, OUTPUT);
  
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
  
  // Calcular factores iniciales
  calcularFactores();
  imprimirConfiguracion();
  
  Serial.println(">>> INICIANDO EN 2 SEGUNDOS <<<");
  delay(2000);
  
  tiempoInicio = millis();
  Serial.println(">>> DERECHA <<<");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // Leer comandos Serial
  if (Serial.available()) {
    char c = Serial.read();
    
    if (c == 'p' || c == 'P') {
      enMovimiento = false;
      parar();
      Serial.println("\n*** PARADO ***");
    }
    else if (c == 'r' || c == 'R') {
      enMovimiento = true;
      tiempoInicio = millis();
      Serial.println("\n*** REANUDANDO ***");
    }
    else if (c >= '1' && c <= '9') {
      VELOCIDAD = 50 + (c - '1') * 25;
      Serial.print("Velocidad: ");
      Serial.println(VELOCIDAD);
      imprimirConfiguracion();
    }
    else if (c == 'a' || c == 'A') {
      configuracionA();
    }
    else if (c == 'b' || c == 'B') {
      configuracionB();
    }
    else if (c == 'c' || c == 'C') {
      configuracionC();
    }
  }
  
  // Si está parado, no hacer nada
  if (!enMovimiento) {
    digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
    return;
  }
  
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;
  
  // Estado: en pausa
  if (enPausa) {
    parar();
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
    
    if (tiempoTranscurrido >= TIEMPO_PAUSA) {
      enPausa = false;
      yendoDerecha = !yendoDerecha;
      tiempoInicio = millis();
      
      if (yendoDerecha) {
        Serial.println("\n>>> DERECHA <<<");
      } else {
        Serial.println("\n>>> IZQUIERDA <<<");
      }
    }
    return;
  }
  
  // Estado: en movimiento
  int direccion = yendoDerecha ? 1 : -1;
  moverLateral(direccion);
  digitalWrite(LED_BUILTIN, yendoDerecha ? HIGH : LOW);
  
  // Mostrar estado cada 500ms
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 500) {
    int velM1 = (int)(factorM1_lateral * direccion * VELOCIDAD);
    int velM2 = (int)(factorM2_lateral * direccion * VELOCIDAD);
    int velM3 = (int)(factorM3_lateral * direccion * VELOCIDAD);
    
    Serial.print("  ");
    Serial.print(yendoDerecha ? "[DER]" : "[IZQ]");
    Serial.print(" M1="); Serial.print(velM1);
    Serial.print(" M2="); Serial.print(velM2);
    Serial.print(" M3="); Serial.print(velM3);
    Serial.print(" | t="); Serial.print(tiempoTranscurrido / 1000.0, 1);
    Serial.println("s");
    
    ultimoPrint = millis();
  }
  
  // Verificar si terminó el tiempo
  if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
    parar();
    enPausa = true;
    tiempoInicio = millis();
    Serial.println("  -> pausa");
  }
}
