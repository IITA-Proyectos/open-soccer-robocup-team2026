// ============================================================================
// ⚠️ STAGING CONGELADO (2026-06-03) — NO subir más material a software/staging/.
// Antes de tocar o agregar algo acá, LEÉ:
//   software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md
// Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
//   software/teensy/Soccer 2026/src/  (ver el "mapa de reúso" del documento).
// ============================================================================

// ⚠️ STAGING CONGELADO — NO FLASHEAR. Sketch de prueba 2025 con BNO local + lib vieja; el equivalente VIVO esta en src/diag/ (ver software/staging/README.md).
// Lectura de sensores de luz ALSPT19 via multiplexor U1 (CD4051BM)
// Placa: Teensy 4.0 - Placa Base Robot Soccer 2026 - IITA Salta
//
// U1 maneja sensores S1-S8
// COM  → O1 → A0 (pin 14 Teensy)
// E1 → pin 2 Teensy  (A0 del 4051)
// E2 → pin 3 Teensy  (A1 del 4051)
// E3 → pin 4 Teensy  (A2 del 4051)

// ── Pines ──────────────────────────────────────────
#define MUX_COM   A0   // Salida analógica del CD4051 U1
#define MUX_E1     13    // Bit de selección LSB
#define MUX_E2     2
#define MUX_E3     3    // Bit de selección MSB


// Tabla de canales del CD4051
// Canal: E3 E2 E1
//   0:   0 0 0  → S1
//   1:   0 0 1  → S2
//   2:   0 1 0  → S3
//   3:   0 1 1  → S4
//   4:   1 0 0  → S5
//   5:   1 0 1  → S6
//   6:   1 1 0  → S7
//   7:   1 1 1  → S8


// ── Función: selecciona canal del mux y lee el ADC ──
int leerCanalMux() {
  // Escribir los 3 bits de selección
  digitalWrite(MUX_E1, 0);
  digitalWrite(MUX_E2, 0);
  digitalWrite(MUX_E3, 0);

  delay(20); // Tiempo de estabilización del 4051
  return analogRead(A0);;
}

// ── Setup ───────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(MUX_E1, OUTPUT);
  pinMode(MUX_E2, OUTPUT);
  pinMode(MUX_E3, OUTPUT);

  // Teensy 4.0: ADC de 12 bits (0-4095)
  analogReadResolution(12);
}

// ── Loop ────────────────────────────────────────────
void loop() {
  
  Serial.print(leerCanalMux());
  Serial.println();

  delay(100);
}
