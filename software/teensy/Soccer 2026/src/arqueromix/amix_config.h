// amix_config.h — Constantes del programa AUTOCONTENIDO "arqueromix".
//
// CONTRATO. Es el HERMANO ARQUERO de centralmix: un PORT del ARQUERO 2025
// (software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026, rama
// #define ROBOT1 = ARQUERO), con el mismo enfoque que centralmix (delantero):
//   - FSM 2025 portada fiel + PRIMITIVAS DE MOTOR DIRECTAS (analogWrite/digitalWrite),
//   - alimentado por TOP/DOWN (NO por los sensores locales del 2025),
//   - SIN world_model (variables planas estilo 2025, ver amix_io.h).
//
// QUÉ REEMPLAZA (pedido de Virginia, igual que el delantero del viernes):
//   - BNO local del 2025        → HEADING del SNAPSHOT del TOP (por serie).
//   - cámara local v1 del 2025  → pelota/arcos del SNAPSHOT del TOP (por serie).
//   - 3 sensores de luz locales → LÍNEA del DOWN (LineStatusV2, por serie).
//
// PINES de motor = ZIRCON ACTUAL (R1 y R2 comparten layout en 2026):
//   M1 = INA2 / INB5 / PWM3   (delantera IZQUIERDA, U5)
//   M2 = INA8 / INB7 / PWM6   (delantera DERECHA,   U17)
//   M3 = INA11 / INB12 / PWM4 (TRASERA,             U7)
//   MOTOR_INVERT = {+1, +1, +1}  (banco 2026, ambos robots)
//
// ⚠️ OJO MAPEO 2025↔2026: el arquero 2025 era #define ROBOT1 con pines
// M1=2/5/3, M2=8/7/6, M3=11/12/4 (FIEL §3.1). Coincide con el layout 2026 → se usan
// esos. Pero el SENTIDO FÍSICO de cada primitiva se RE-VERIFICA en banco (compila != anda).
//
// ⚠️ NO TESTEADO EN HARDWARE. Los valores numéricos son el port 1:1 del 2025
// (docs/internal/ANALISIS-FIEL-ARQUERO-2025.md); el sentido y los umbrales en mm se
// validan/re-tunean en banco (TASK del equipo).

#pragma once
#include <stdint.h>

namespace iitasoccer {
namespace arqmix {

// ============================================================
// Pines de motor — Zircon 2026 (espejo de config_central.h + centralmix/mix_config.h).
// ============================================================
constexpr int AMIX_PIN_INA1 = 2;   // M1 delantera IZQUIERDA (U5)
constexpr int AMIX_PIN_INB1 = 5;
constexpr int AMIX_PIN_PWM1 = 3;
constexpr int AMIX_PIN_INA2 = 8;   // M2 delantera DERECHA (U17)
constexpr int AMIX_PIN_INB2 = 7;
constexpr int AMIX_PIN_PWM2 = 6;
constexpr int AMIX_PIN_INA3 = 11;  // M3 TRASERA (U7)
constexpr int AMIX_PIN_INB3 = 12;
constexpr int AMIX_PIN_PWM3 = 4;

// Sentido por motor ({+1,+1,+1} banco 2026). [0]=M1, [1]=M2, [2]=M3.
constexpr int AMIX_MOTOR_INVERT[3] = { +1, +1, +1 };
constexpr int AMIX_MAX_PWM = 255;

// ============================================================
// PATRULLA — adproporcional() / aiproporcional() (FIEL §5, código 2025 L186-233).
// Strafe lateral con CORRECCIÓN DE RUMBO en 3 bandas según el signo de `error`
// (error = heading - heading_inicial). Las delanteras (M1,M2) van al MISMO sentido;
// la trasera (M3) al opuesto, y su magnitud cambia por banda para enderezar.
// Valores 2025 (multiplicados por `pd` en runtime). Los comentarios //NN del 2025 eran
// valores ANTIGUOS (no activos) — acá van los ACTIVOS.
// ============================================================
//  Banda CENTRADA (-1 < error < 1):
constexpr int AMIX_PROP_FRONT_CENTER = 50;   // M1 y M2
constexpr int AMIX_PROP_REAR_CENTER  = 89;   // M3
//  Banda error > 0:
constexpr int AMIX_PROP_FRONT_EPOS   = 50;   // M1 y M2 (ambas funciones)
constexpr int AMIX_AD_REAR_EPOS      = 100;  // adproporcional (derecha) M3
constexpr int AMIX_AI_REAR_EPOS      = 40;   // aiproporcional  (izquierda) M3
//  Banda error < 0 (M1 y M2 difieren entre sí):
constexpr int AMIX_PROP_M1_ENEG      = 40;   // "motor derecho" (idx0=M1) 2025
constexpr int AMIX_PROP_M2_ENEG      = 65;   // "motor izquierdo" (idx1=M2) 2025
constexpr int AMIX_AD_REAR_ENEG      = 40;   // adproporcional M3
constexpr int AMIX_AI_REAR_ENEG      = 100;  // aiproporcional  M3

// `pd` (factor proporcional): 1.0 sin pelota (patrulla base), 1.5 con pelota desviada.
constexpr float AMIX_PD_BASE = 1.0f;
constexpr float AMIX_PD_BALL = 1.5f;

// ============================================================
// Impulso inicial / anti-traba del borde (FIEL §2, L1018-1020 + impulso_*).
// ============================================================
// impulso_inicial (arquero, estado inicial): strafe fuerte 40 ms.
//   M1=1.8*50=90, M2=1.8*50=90, M3=1.8*85=153 (mismo sentido fronts, opuesto rear).
constexpr int AMIX_IMP_INI_FRONT = 90;   // M1, M2
constexpr int AMIX_IMP_INI_REAR  = 153;  // M3 (⚠️ >150 = roza el techo térmico; transitorio 40 ms tolerable)

// ============================================================
// Avance / despeje (FIEL §2/§5).
// ============================================================
// avanzar(): M1=100, M2=100(sentido opuesto), M3=0. (L152-154)
constexpr int AMIX_AVANZAR = 100;
// avanzar_patear() (arquero): PWM FIJO inmediato (sin rampa), M1=patadM1, M2=patadM2, M3=0.
constexpr int AMIX_PATAD_M1 = 250;   // patadM1 (R1=R2)
constexpr int AMIX_PATAD_M2 = 150;   // patadM2 (R1=150; R2 2025 usaba 200 → tunable)
// PATEANDO_atras_arquero (inline 2025, L1186-1188): retroceso recto M1=150, M2=150, M3=0.
constexpr int AMIX_ATRAS = 150;

// ============================================================
// Tolerancias de la pelota — ⚠️ UNIDADES CAMBIADAS 2025(cámara)→2026(mm). RE-TUNEAR.
// En 2025 (cámara local): Xp = profundidad/distancia (chico=cerca), Yp = lateral.
// En 2026 (snapshot TOP, marco +X=derecha, +Y=adelante, mm):
//   ball_y_mm  ≈ profundidad (lo que era Xp)  →  "cerca" = ball_y_mm pequeño.
//   ball_x_mm  ≈ lateral     (lo que era Yp)  →  "centrada" = |ball_x_mm| pequeño.
// ⚠️ El SIGNO lateral (a qué lado mover) y los valores en mm se CONFIRMAN en banco.
// ============================================================
constexpr float AMIX_TOL_CERCANIA_MM = 140.0f; // patea si profundidad <= esto (era Xp<=140) <RE-TUNE>
constexpr float AMIX_TOL_CENTRADO_MM = 30.0f;  // "centrada": |lateral| <= esto (era |Yp|<=3) <RE-TUNE>
constexpr float AMIX_TOL_DESVIO_MM   = 50.0f;  // "desviada": |lateral| >= esto (era |Yp|>=5) <RE-TUNE>

// ============================================================
// Tiempos / timeouts del ARQUERO (FIEL §3.5) — ms. Iguales 2025.
// ============================================================
constexpr unsigned long AMIX_T_IMP_INICIAL   = 40;    // impulso_inicial → patrulla
constexpr unsigned long AMIX_T_IMP_LATERAL   = 350;   // impulso_der/izq (anti-traba borde)
constexpr unsigned long AMIX_T_PAT_PAUSA_INI = 200;   // PATEANDO_pausa_inicial_arquero
constexpr unsigned long AMIX_T_PAT_ADELANTE  = 450;   // PATEANDO_adelante_arquero
constexpr unsigned long AMIX_T_PAT_PAUSA     = 1000;  // PATEANDO_pausa_arquero
constexpr unsigned long AMIX_T_AVANCE_POST   = 1000;  // avanzar_despues_de_patear
// (PATEANDO_atras_arquero NO tiene timeout en 2025: retrocede hasta ver blanco. Acá se
//  agrega un timeout de SEGURIDAD para no colgarse — ver amix_fsm. <MEJORA 2026>)
constexpr unsigned long AMIX_T_ATRAS_SAFETY  = 4000;  // tope de seguridad del retroceso (NO estaba en 2025)

// ============================================================
// Línea (de DOWN) — reemplaza los 3 sensores de luz locales del 2025.
// El 2025 detectaba el borde con s1>=blanco1 || s2>=blanco2 (no s3) durante la
// patrulla, y s1||s2||s3 para "volví a la línea" tras el retroceso. Acá la línea
// llega como line_present/line_depth (amix_io, de DOWN). line_depth>=TRIGGER = tocando.
// ============================================================
constexpr uint8_t AMIX_LINE_DEPTH_TRIGGER = 1;  // ≥1 sensor en blanco = línea presente

// ============================================================
// Comunicación — enlaces TOP (Serial7) y DOWN (Serial1), 230400 (= comm_top/comm_down).
// ============================================================
constexpr long AMIX_UART_BAUD = 230400;

// ============================================================
// Heading — FUENTE = SNAPSHOT del TOP por default (pedido de Virginia: el BNO se lee
// por serie de la placa superior, NO local). Con -DARQMIX_HEADING_OTOS usa el heading
// crudo del OTOS (Pose2D de DOWN) para A/B. NO se inicializa ningún BNO local.
// ============================================================
#ifdef ARQMIX_HEADING_OTOS
constexpr bool AMIX_HEADING_SOURCE_IS_OTOS = true;
#else
constexpr bool AMIX_HEADING_SOURCE_IS_OTOS = false;  // DEFAULT: heading del TOP (snapshot)
#endif

}  // namespace arqmix
}  // namespace iitasoccer
