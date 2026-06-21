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
// impulso_inicial (arquero, estado inicial): strafe corto 40 ms para despegarse.
//   2025: M1=M2=90, M3=153. BAJADO 2026-06-21 (banco Virginia): el arranque daba un tirón
//   fuerte; 153 rozaba el techo térmico. Más suave para que el inicio sea prolijo y verificable.
constexpr int AMIX_IMP_INI_FRONT = 70;   // M1, M2 (era 90)
constexpr int AMIX_IMP_INI_REAR  = 110;  // M3 (era 153)

// ============================================================
// Avance / despeje (FIEL §2/§5).
// ============================================================
// avanzar(): M1=100, M2=100(sentido opuesto), M3=0. (L152-154)
constexpr int AMIX_AVANZAR = 100;
// avanzar_patear() (arquero): RAMPA DE ACELERACIÓN como el DELANTERO (centralmix), pedido Virginia
// 2026-06-21 (el despeje fijo asimétrico "no apuntaba bien" → veraba). Sube la velocidad de a pasos
// desde 0 hasta VEL_FINAL → el golpe arranca suave y sale parejo. Patrón SIMÉTRICO M1=+vel, M2=-vel,
// M3=0 = avance RECTO al frente (a diferencia del 250/150 asimétrico del 2025 que torcía).
// Misma receta que el delantero (mix_config.h KICK_*). Potencia moderada (pedido previo de bajarla).
constexpr int AMIX_KICK_VEL_FINAL    = 180;  // velocidad PICO del golpe — subir si no llega a despejar
constexpr int AMIX_KICK_PASO         = 20;   // incremento de PWM por escalón de la rampa
constexpr int AMIX_KICK_INTERVALO_MS = 10;   // ms entre escalones (rampa 0→180 en ~90 ms)
// PATEANDO_atras_arquero (inline 2025, L1186-1188): retroceso recto M1=ATRAS, M2=ATRAS, M3=0.
constexpr int AMIX_ATRAS = 120;      // retroceso del despeje (era 150) — bajado con el resto

// ============================================================
// Tolerancias de la pelota — POR ÁNGULO (como centralmix usa la cámara). FIX 2026-06-21.
// ----------------------------------------------------------------------------------
// PROBLEMA que arregla (banco Virginia): con umbrales en mm (CENTRADO=30/DESVIO=50/
// CERCANIA=140) la pelota caía en la BANDA MUERTA para la escala REAL del snapshot
// (CAMERA_UNIT_TO_MM=10 sin calibrar) → el FSM hacía parar() → la cámara veía la pelota
// pero el robot NO se movía (freeze).
// FIX: el arquero SIGUE la pelota por su ÁNGULO (g_aio.angulo_pelota_deg = atan2(x,y)),
// que NO depende de la escala — igual que el delantero (centralmix). La banda muerta
// angular es ANGOSTA → el arquero trackea cualquier pelota off-center.
//   angulo_pelota_deg: 0=frente, >0=derecha, <0=izquierda.
// ============================================================
constexpr float AMIX_TOL_CENTRADO_DEG = 8.0f;   // |áng| <= esto → ALINEADO (mantiene posición).
                                                // |áng| > esto  → SIGUE la pelota (strafe a su lado).
constexpr float AMIX_TOL_KICK_DEG     = 30.0f;  // para DESPEJAR: la pelota debe estar dentro de
                                                // este ángulo (más amplio que CENTRADO) Y cerca.
constexpr float AMIX_TOL_CERCANIA_MM  = 250.0f; // DESPEJA si la distancia euclídea a la pelota
                                                // (sqrt(x²+y²)) <= esto. ⚠️ EL knob de tuning
                                                // principal (la escala del snapshot está sin
                                                // calibrar): subir si nunca despeja, bajar si
                                                // despeja de lejos. <RE-TUNE EN BANCO>

// ============================================================
// Tiempos / timeouts del ARQUERO (FIEL §3.5) — ms. Iguales 2025.
// ============================================================
// INICIO — homing al área chica (banco Virginia 2026-06-21): retrocede hasta ver la línea del
// área, avanza un poco a ciegas, y recién ahí patrulla.
// ⚠️ SAFETY a 50 s TEMPORAL (pedido Virginia, para observar el retroceso): después bajar a ~4 s.
constexpr unsigned long AMIX_T_INICIO_RETRO_SAFETY = 50000; // TEMP 50 s (era 4000) — bajar luego
constexpr unsigned long AMIX_T_INICIO_AVANCE       = 400;   // avanzar un poco tras ver la línea (SIN leer sensores) — tunable
// Retroceso del homing: primitiva DEDICADA con PWM propio (no acoplada al despeje) y dirección
// flippable. retroceder_inicio() = M1=-PWM, M2=+PWM (= patrón patear_atras = hacia ATRÁS) × SIGN.
// ⚠️ Si al GO el robot va hacia ADELANTE en vez de atrás → flashear con -DARQMIX_FLIP_INICIO_RETRO.
constexpr int AMIX_INICIO_RETRO_PWM = 100;  // PWM del retroceso de inicio (controlado, no a tope)
#ifdef ARQMIX_FLIP_INICIO_RETRO
constexpr int AMIX_INICIO_RETRO_SIGN = -1;  // invierte la dirección del retroceso de inicio
#else
constexpr int AMIX_INICIO_RETRO_SIGN = +1;
#endif
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

// SIGNO de la corrección de rumbo de la patrulla (las 3 bandas de error en ad/aiproporcional).
// ✅ VALIDADO EN BANCO (Virginia 2026-06-21): el signo CORRECTO para este robot es -1 (con +1 el
// arquero se daba vuelta 180° mientras patrullaba; con -1 queda derecho). Es el DEFAULT.
// El flag -DARQMIX_HEADING_SIGN_OLD vuelve al +1 viejo (NO usar salvo diagnóstico). Requiere
// heading válido del TOP para que la corrección actúe.
#ifdef ARQMIX_HEADING_SIGN_OLD
constexpr float AMIX_HEADING_CORRECT_SIGN = +1.0f;  // viejo (daba 180°) — solo fallback/diagnóstico
#else
constexpr float AMIX_HEADING_CORRECT_SIGN = -1.0f;  // ✅ CORRECTO (banco Virginia 2026-06-21)
#endif

}  // namespace arqmix
}  // namespace iitasoccer
