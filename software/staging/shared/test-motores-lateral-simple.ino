// =============================================================================
// TEST: Movimiento lateral SIMPLE - SOLO MOTORES
// Archivo: staging/shared/test-motores-lateral-simple.ino
// 
// PROPÓSITO: 
//   - Probar SOLO el movimiento lateral con los motores
//   - SIN giróscopo, SIN PID, SIN nada más
//   - Loop automático: derecha 3 seg, izquierda 3 seg
//   - Para encontrar la combinación correcta de motores
//
// CONTROL:
//   - Arranca automáticamente al encender
//   - Enviar 'p' por Serial para PARAR
//   - Enviar 'r' por Serial para REANUDAR
//   - Enviar '1' a '9' por Serial para cambiar velocidad (1=50, 9=250)
//
// ROBOT 2 (delantero) - Pines según definitivo-delantero
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
int VELOCIDAD = 100;  // Velocidad base (ajustable por Serial)
const unsigned long TIEMPO_MOVIMIENTO = 3000;  // 3 segundos
const unsigned long TIEMPO_PAUSA = 500;  // 0.5 segundos entre cambios

// =============================================================================
// VARIABLES
// =============================================================================
bool enMovimiento = true;
bool yendoDerecha = true;
unsigned long tiempoInicio = 0;
bool enPausa = false;

// =============================================================================
// FUNCIONES DE MOTORES
// =============================================================================

// Motor 1: adelante = INA1=1, INB1=0
void motor1(int vel, int dir) {
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

// Motor 2: adelante = INA2=0, INB2=1
void motor2(int vel, int dir) {
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

// Motor 3: derecha = INA3=1, INB3=0
void motor3(int vel, int dir) {
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
// MOVIMIENTO LATERAL
// Probá diferentes combinaciones hasta encontrar la correcta
// =============================================================================

void moverDerecha() {
  // OPCIÓN A: Solo Motor 3
  motor3(VELOCIDAD, 1);  // M3 hacia derecha
  motor1(0, 0);
  motor2(0, 0);
  
  // OPCIÓN B: Si necesita compensación, descomentar:
  // motor1(VELOCIDAD/2, -1);  // M1 hacia atrás
  // motor2(VELOCIDAD/2, -1);  // M2 hacia atrás
}

void moverIzquierda() {
  // OPCIÓN A: Solo Motor 3
  motor3(VELOCIDAD, -1);  // M3 hacia izquierda
  motor1(0, 0);
  motor2(0, 0);
  
  // OPCIÓN B: Si necesita compensación, descomentar:
  // motor1(VELOCIDAD/2, 1);  // M1 hacia adelante
  // motor2(VELOCIDAD/2, 1);  // M2 hacia adelante
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST MOTORES LATERAL - SIMPLE                   *");
  Serial.println("*  SIN giroscopo, SIN PID                          *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("COMANDOS por Serial:");
  Serial.println("  'p' = PARAR");
  Serial.println("  'r' = REANUDAR");
  Serial.println("  '1'-'9' = Cambiar velocidad (1=50 ... 9=250)");
  Serial.println();
  Serial.print("Velocidad actual: ");
  Serial.println(VELOCIDAD);
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
      Serial.println("Enviar 'r' para reanudar");
    }
    else if (c == 'r' || c == 'R') {
      enMovimiento = true;
      tiempoInicio = millis();
      Serial.println("\n*** REANUDANDO ***");
    }
    else if (c >= '1' && c <= '9') {
      VELOCIDAD = 50 + (c - '1') * 25;  // 1=50, 2=75, 3=100, ..., 9=250
      Serial.print("Velocidad: ");
      Serial.println(VELOCIDAD);
    }
  }
  
  // Si está parado, no hacer nada
  if (!enMovimiento) {
    digitalWrite(LED_BUILTIN, (millis() / 500) % 2);  // Parpadeo lento
    return;
  }
  
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;
  
  // Estado: en pausa entre movimientos
  if (enPausa) {
    parar();
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);  // Parpadeo rápido
    
    if (tiempoTranscurrido >= TIEMPO_PAUSA) {
      enPausa = false;
      yendoDerecha = !yendoDerecha;  // Cambiar dirección
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
  if (yendoDerecha) {
    moverDerecha();
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    moverIzquierda();
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  // Mostrar estado cada 500ms
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 500) {
    Serial.print("  ");
    Serial.print(yendoDerecha ? "[DER]" : "[IZQ]");
    Serial.print(" vel=");
    Serial.print(VELOCIDAD);
    Serial.print(" t=");
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
