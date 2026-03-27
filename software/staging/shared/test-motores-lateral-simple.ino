// =============================================================================
// TEST: Movimiento lateral - AJUSTAR VALORES MANUALMENTE
// Archivo: staging/shared/test-motores-lateral-simple.ino
// 
// INSTRUCCIONES:
//   1. Modificar los valores en la sección "AJUSTAR ESTOS VALORES"
//   2. Compilar y subir al robot
//   3. El robot va a moverse: derecha 3 seg, pausa, izquierda 3 seg, repite
//   4. Observar si va recto o gira
//   5. Volver al paso 1 y ajustar valores hasta que vaya recto
//
// CINEMÁTICA 3 RUEDAS OMNIDIRECCIONAL:
//   Para movimiento lateral, los motores delanteros giran en direcciones
//   OPUESTAS entre sí, y el trasero empuja lateralmente.
//
// ROBOT 2 (delantero)
// =============================================================================

#include <Arduino.h>

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

// VELOCIDADES (0 a 255)
// Si el robot gira, ajustar estas velocidades hasta que vaya recto
int VEL_M1 = 50;     // Velocidad Motor 1 (frontal)
int VEL_M2 = 50;     // Velocidad Motor 2 (frontal)
int VEL_M3 = 100;    // Velocidad Motor 3 (trasero - rueda lateral)

// DIRECCIONES para ir a la DERECHA
// M1 y M2 giran en direcciones OPUESTAS (así funciona omnidireccional)
// Si un motor gira al revés de lo esperado, cambiar su signo
int DIR_M1 = 1;      // Motor 1: adelante (+1)
int DIR_M2 = -1;     // Motor 2: atrás (-1)  ← OPUESTO a M1
int DIR_M3 = 1;      // Motor 3: hacia derecha (+1)

// TIEMPO de movimiento en cada dirección (milisegundos)
unsigned long TIEMPO_MOVIMIENTO = 3000;  // 3 segundos
unsigned long TIEMPO_PAUSA = 500;        // 0.5 segundos de pausa

// =============================================================================
// ******************** FIN AJUSTES ********************
// =============================================================================

// Variables de estado
bool yendoDerecha = true;
bool enPausa = false;
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

void moverDerecha() {
  motor1(VEL_M1, DIR_M1);
  motor2(VEL_M2, DIR_M2);
  motor3(VEL_M3, DIR_M3);
}

void moverIzquierda() {
  // Izquierda = direcciones invertidas
  motor1(VEL_M1, -DIR_M1);
  motor2(VEL_M2, -DIR_M2);
  motor3(VEL_M3, -DIR_M3);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(19200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("****************************************************");
  Serial.println("*  TEST MOVIMIENTO LATERAL - AJUSTE MANUAL         *");
  Serial.println("****************************************************");
  Serial.println();
  Serial.println("VALORES ACTUALES:");
  Serial.print("  VEL_M1 = "); Serial.println(VEL_M1);
  Serial.print("  VEL_M2 = "); Serial.println(VEL_M2);
  Serial.print("  VEL_M3 = "); Serial.println(VEL_M3);
  Serial.print("  DIR_M1 = "); Serial.println(DIR_M1);
  Serial.print("  DIR_M2 = "); Serial.println(DIR_M2);
  Serial.print("  DIR_M3 = "); Serial.println(DIR_M3);
  Serial.println();
  Serial.println("Para DERECHA: M1=adelante, M2=atras, M3=derecha");
  Serial.println("Para IZQUIERDA: se invierten todas las direcciones");
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
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);  // Parpadeo rápido
    
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
  
  // Estado: EN MOVIMIENTO
  if (yendoDerecha) {
    moverDerecha();
    digitalWrite(LED_BUILTIN, HIGH);  // LED encendido = derecha
  } else {
    moverIzquierda();
    digitalWrite(LED_BUILTIN, LOW);   // LED apagado = izquierda
  }
  
  // Mostrar estado cada 500ms
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 500) {
    int d = yendoDerecha ? 1 : -1;
    Serial.print("  ");
    Serial.print(yendoDerecha ? "[DER]" : "[IZQ]");
    Serial.print(" M1="); Serial.print(VEL_M1 * DIR_M1 * d);
    Serial.print(" M2="); Serial.print(VEL_M2 * DIR_M2 * d);
    Serial.print(" M3="); Serial.print(VEL_M3 * DIR_M3 * d);
    Serial.print(" | t="); Serial.print(tiempoTranscurrido / 1000.0, 1);
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
