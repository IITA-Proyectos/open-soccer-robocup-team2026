// =============================================================================
// TEST: Ajuste MANUAL de velocidades para movimiento lateral
// Archivo: staging/shared/test-motores-lateral-simple.ino
// 
// PROPÓSITO: 
//   - Encontrar el balance correcto de velocidades para cada motor
//   - Ajustar cada motor individualmente hasta que vaya recto
//   - Una vez encontrado el balance, anotar los valores
//
// CONTROL por Serial:
//   'p' = PARAR
//   'r' = REANUDAR
//   'd' = Solo DERECHA (sin loop)
//   'i' = Solo IZQUIERDA (sin loop)
//   'l' = LOOP derecha/izquierda
//
//   AJUSTAR VELOCIDADES (mientras está en movimiento):
//   'q/a' = M1 +10/-10
//   'w/s' = M2 +10/-10
//   'e/d' = M3 +10/-10
//
//   INVERTIR DIRECCION DE MOTOR:
//   'z' = Invertir M1
//   'x' = Invertir M2
//   'c' = Invertir M3
//
//   '0' = Reset a valores por defecto
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
const unsigned long TIEMPO_MOVIMIENTO = 3000;
const unsigned long TIEMPO_PAUSA = 500;

// =============================================================================
// VELOCIDADES AJUSTABLES POR MOTOR
// Estos son los valores que hay que encontrar experimentalmente
// =============================================================================
int velM1 = 50;    // Velocidad Motor 1
int velM2 = 50;    // Velocidad Motor 2
int velM3 = 100;   // Velocidad Motor 3

// Direcciones (1 o -1) - para invertir si un motor gira al revés
int dirM1_base = -1;  // Para derecha, M1 va hacia atrás (-1)
int dirM2_base = -1;  // Para derecha, M2 va hacia atrás (-1)
int dirM3_base = 1;   // Para derecha, M3 va hacia adelante (1)

// =============================================================================
// VARIABLES
// =============================================================================
enum Modo { PARADO, DERECHA, IZQUIERDA, LOOP_DER, LOOP_IZQ, LOOP_PAUSA };
Modo modoActual = PARADO;
unsigned long tiempoInicio = 0;

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
// MOVIMIENTO
// =============================================================================
void moverDerecha() {
  motor1(velM1, dirM1_base);
  motor2(velM2, dirM2_base);
  motor3(velM3, dirM3_base);
}

void moverIzquierda() {
  // Izquierda = direcciones invertidas
  motor1(velM1, -dirM1_base);
  motor2(velM2, -dirM2_base);
  motor3(velM3, -dirM3_base);
}

// =============================================================================
// IMPRIMIR ESTADO
// =============================================================================
void imprimirConfig() {
  Serial.println("\n========== CONFIGURACION ACTUAL ==========");
  Serial.println("Velocidades para DERECHA:");
  Serial.print("  M1: vel="); Serial.print(velM1);
  Serial.print(" dir="); Serial.println(dirM1_base);
  Serial.print("  M2: vel="); Serial.print(velM2);
  Serial.print(" dir="); Serial.println(dirM2_base);
  Serial.print("  M3: vel="); Serial.print(velM3);
  Serial.print(" dir="); Serial.println(dirM3_base);
  Serial.println("==========================================");
  Serial.println("Ajustes: q/a=M1  w/s=M2  e/d=M3 (+10/-10)");
  Serial.println("Invertir: z=M1  x=M2  c=M3");
  Serial.println("==========================================\n");
}

void imprimirEstado() {
  const char* modoStr;
  switch(modoActual) {
    case PARADO: modoStr = "PARADO"; break;
    case DERECHA: modoStr = "DERECHA"; break;
    case IZQUIERDA: modoStr = "IZQUIERDA"; break;
    case LOOP_DER: modoStr = "LOOP-DER"; break;
    case LOOP_IZQ: modoStr = "LOOP-IZQ"; break;
    case LOOP_PAUSA: modoStr = "LOOP-PAUSA"; break;
    default: modoStr = "???";
  }
  
  Serial.print("["); Serial.print(modoStr); Serial.print("] ");
  Serial.print("M1="); Serial.print(velM1 * dirM1_base);
  Serial.print(" M2="); Serial.print(velM2 * dirM2_base);
  Serial.print(" M3="); Serial.print(velM3 * dirM3_base);
  
  if (modoActual != PARADO && modoActual != LOOP_PAUSA) {
    Serial.print(" | t="); 
    Serial.print((millis() - tiempoInicio) / 1000.0, 1);
    Serial.print("s");
  }
  Serial.println();
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST AJUSTE MANUAL - MOVIMIENTO LATERAL         *");
  Serial.println("*  Ajustar velocidades hasta que vaya RECTO        *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("COMANDOS:");
  Serial.println("  'p' = PARAR");
  Serial.println("  'd' = DERECHA continuo");
  Serial.println("  'i' = IZQUIERDA continuo");
  Serial.println("  'l' = LOOP (der 3s, izq 3s)");
  Serial.println();
  Serial.println("AJUSTES (en movimiento):");
  Serial.println("  'q'/'a' = M1 +10/-10");
  Serial.println("  'w'/'s' = M2 +10/-10");
  Serial.println("  'e'/'f' = M3 +10/-10");
  Serial.println();
  Serial.println("INVERTIR MOTOR:");
  Serial.println("  'z' = Invertir M1");
  Serial.println("  'x' = Invertir M2");
  Serial.println("  'c' = Invertir M3");
  Serial.println();
  Serial.println("  '0' = Reset valores por defecto");
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
  imprimirConfig();
  
  Serial.println(">>> Listo. Enviar 'd' para DERECHA, 'i' para IZQUIERDA <<<\n");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // Leer comandos Serial
  if (Serial.available()) {
    char c = Serial.read();
    
    // Comandos de movimiento
    if (c == 'p' || c == 'P') {
      modoActual = PARADO;
      parar();
      Serial.println("\n*** PARADO ***");
      imprimirConfig();
    }
    else if (c == 'd' || c == 'D') {
      modoActual = DERECHA;
      tiempoInicio = millis();
      Serial.println("\n>>> DERECHA continuo <<<");
    }
    else if (c == 'i' || c == 'I') {
      modoActual = IZQUIERDA;
      tiempoInicio = millis();
      Serial.println("\n>>> IZQUIERDA continuo <<<");
    }
    else if (c == 'l' || c == 'L') {
      modoActual = LOOP_DER;
      tiempoInicio = millis();
      Serial.println("\n>>> LOOP iniciado (DERECHA) <<<");
    }
    
    // Ajustes de velocidad
    else if (c == 'q' || c == 'Q') {
      velM1 = constrain(velM1 + 10, 0, 255);
      Serial.print("M1 = "); Serial.println(velM1);
    }
    else if (c == 'a' || c == 'A') {
      velM1 = constrain(velM1 - 10, 0, 255);
      Serial.print("M1 = "); Serial.println(velM1);
    }
    else if (c == 'w' || c == 'W') {
      velM2 = constrain(velM2 + 10, 0, 255);
      Serial.print("M2 = "); Serial.println(velM2);
    }
    else if (c == 's' || c == 'S') {
      velM2 = constrain(velM2 - 10, 0, 255);
      Serial.print("M2 = "); Serial.println(velM2);
    }
    else if (c == 'e' || c == 'E') {
      velM3 = constrain(velM3 + 10, 0, 255);
      Serial.print("M3 = "); Serial.println(velM3);
    }
    else if (c == 'f' || c == 'F') {
      velM3 = constrain(velM3 - 10, 0, 255);
      Serial.print("M3 = "); Serial.println(velM3);
    }
    
    // Invertir direcciones
    else if (c == 'z' || c == 'Z') {
      dirM1_base = -dirM1_base;
      Serial.print("M1 direccion invertida = "); Serial.println(dirM1_base);
    }
    else if (c == 'x' || c == 'X') {
      dirM2_base = -dirM2_base;
      Serial.print("M2 direccion invertida = "); Serial.println(dirM2_base);
    }
    else if (c == 'c' || c == 'C') {
      dirM3_base = -dirM3_base;
      Serial.print("M3 direccion invertida = "); Serial.println(dirM3_base);
    }
    
    // Reset
    else if (c == '0') {
      velM1 = 50;
      velM2 = 50;
      velM3 = 100;
      dirM1_base = -1;
      dirM2_base = -1;
      dirM3_base = 1;
      Serial.println("\n*** RESET a valores por defecto ***");
      imprimirConfig();
    }
  }
  
  // Ejecutar según modo
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;
  
  switch(modoActual) {
    case PARADO:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
      break;
      
    case DERECHA:
      moverDerecha();
      digitalWrite(LED_BUILTIN, HIGH);
      break;
      
    case IZQUIERDA:
      moverIzquierda();
      digitalWrite(LED_BUILTIN, LOW);
      break;
      
    case LOOP_DER:
      moverDerecha();
      digitalWrite(LED_BUILTIN, HIGH);
      if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
        parar();
        modoActual = LOOP_PAUSA;
        tiempoInicio = millis();
        Serial.println("  -> pausa");
      }
      break;
      
    case LOOP_IZQ:
      moverIzquierda();
      digitalWrite(LED_BUILTIN, LOW);
      if (tiempoTranscurrido >= TIEMPO_MOVIMIENTO) {
        parar();
        modoActual = LOOP_PAUSA;
        tiempoInicio = millis();
        Serial.println("  -> pausa");
      }
      break;
      
    case LOOP_PAUSA:
      parar();
      digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
      if (tiempoTranscurrido >= TIEMPO_PAUSA) {
        // Alternar dirección
        static bool proximaDerecha = false;
        proximaDerecha = !proximaDerecha;
        
        if (proximaDerecha) {
          modoActual = LOOP_DER;
          Serial.println("\n>>> DERECHA <<<");
        } else {
          modoActual = LOOP_IZQ;
          Serial.println("\n>>> IZQUIERDA <<<");
        }
        tiempoInicio = millis();
      }
      break;
  }
  
  // Mostrar estado cada 500ms (solo si está en movimiento)
  static unsigned long ultimoPrint = 0;
  if (modoActual != PARADO && millis() - ultimoPrint > 500) {
    imprimirEstado();
    ultimoPrint = millis();
  }
}
