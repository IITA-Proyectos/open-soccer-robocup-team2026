// mix_config.h — Constantes del programa AUTOCONTENIDO "centralmix".
//
// CONTRATO. Esto es un PORT del delantero 2025
// (software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp), pero:
//   - alimentado por datos de TOP/DOWN (NO por la lectura serial cruda 2025),
//   - SIN usar world_model (variables planas estilo 2025, ver mix_io.h),
//   - con PRIMITIVAS DE MOTOR DIRECTAS (analogWrite/digitalWrite) como el 2025.
//
// Pines de motor = ZIRCON ACTUAL ROBOT1 (R1), tomados de
// src/central/config_central.h rama ROBOT1:
//   M1 = INA2 / INB5 / PWM3   (delantera IZQUIERDA, U5)
//   M2 = INA8 / INB7 / PWM6   (delantera DERECHA,   U17)
//   M3 = INA11 / INB12 / PWM4 (TRASERA,             U7)
//   MOTOR_INVERT = {+1, +1, +1}
//
// ⚠️ OJO MAPEO 2025↔2026: el delantero 2025 con #define ROBOT2 usaba OTROS pines
// (M1=8/7/6, M2=11/12/4, M3=2/5/3). ESTE archivo NO copia esos: usa los pines R1
// del Zircon 2026 (config_central.h), como pide la tarea. Los implementadores de
// mix_motors.cpp deben respetar ESTE pinout, no el del .cpp 2025.
//
// ⚠️ NO TESTEADO EN HARDWARE. Contrato de constantes — los valores numéricos son
// el port 1:1 del 2025; el sentido físico de cada primitiva se valida en banco.

#pragma once
#include <stdint.h>   // uint8_t (MIX_LINE_DEPTH_TRIGGER)

namespace iitasoccer {
namespace mix {

// ============================================================
// Pines de motor — R1 (Zircon actual). Espejo de config_central.h rama ROBOT1.
// ============================================================
//   M1 = delantera IZQUIERDA (U5)
constexpr int MIX_PIN_INA1 = 2;
constexpr int MIX_PIN_INB1 = 5;
constexpr int MIX_PIN_PWM1 = 3;
//   M2 = delantera DERECHA (U17)
constexpr int MIX_PIN_INA2 = 8;
constexpr int MIX_PIN_INB2 = 7;
constexpr int MIX_PIN_PWM2 = 6;
//   M3 = TRASERA (U7)
constexpr int MIX_PIN_INA3 = 11;
constexpr int MIX_PIN_INB3 = 12;
constexpr int MIX_PIN_PWM3 = 4;

// Sentido por motor (+1 normal, -1 invertido por HW). R1 banco 2026: {+1,+1,+1}.
// Índices: [0]=M1, [1]=M2, [2]=M3. mix_set_motor() debe aplicar este signo.
constexpr int MIX_MOTOR_INVERT[3] = { +1, +1, +1 };

constexpr int MIX_MAX_PWM = 255;  // rango de analogWrite

// ============================================================
// Umbrales de línea (sensores analógicos) — port del 2025.
// El 2025 leía 3 sensores analógicos (LINE_PIN1/2/3) y comparaba contra blanco1/2/3.
// En centralmix la LÍNEA llega por DOWN (mix_io: line_present / line_angle_deg /
// line_depth), NO por analogRead. Estos umbrales quedan como REFERENCIA del 2025 y
// por si se cablean sensores locales de respaldo. line_depth>=MIX_LINE_DEPTH_TRIGGER
// es el criterio sugerido para "tocando línea" cuando se usa el dato de DOWN.
// ============================================================
constexpr int MIX_LINE_BLANCO1 = 650;  // 2025 ROBOT2 blanco1
constexpr int MIX_LINE_BLANCO2 = 650;  // 2025 ROBOT2 blanco2
constexpr int MIX_LINE_BLANCO3 = 750;  // 2025 ROBOT2 blanco3
// Umbral sugerido sobre el dato de DOWN (line_depth = # sensores / profundidad).
constexpr uint8_t MIX_LINE_DEPTH_TRIGGER = 1;  // ≥1 sensor en blanco = línea presente

// ============================================================
// Constantes de velocidad 2025 (multiplicadores de PWM).
//   g  = girando
//   a  = apuntando pelota
//   c  = velocidad centrando
//   ic = velocidad impulso centrando
// (ROBOT2 2025: c=0.4, ic=0.55)
// ============================================================
constexpr float MIX_G  = 0.4f;   // girando
constexpr float MIX_A  = 0.35f;   // apuntando pelota
constexpr float MIX_C  = 0.4f;   // centrando (2025 ROBOT2)
constexpr float MIX_IC = 0.55f;  // impulso centrando (2025 ROBOT2)
constexpr float MIX_PD = 1.0f;   // avances proporcionales (2025 'pd')

// ============================================================
// Tolerancias 2025 (en mm para cercanía/centrado; en grados para apuntado).
// ============================================================
constexpr float MIX_TOL_CENTRADO = 60.0f;  // tolerancia_centrado
constexpr float MIX_TOL_CERCANIA = 6000.0f;  // tolerancia_cercania
constexpr float MIX_TOL_APUNTADO = 30.0f;  // tolerancia_apuntado (grados)

// ============================================================
// Kicker / patada — port 1:1 del 2025 (el "kicker" es empuje por inercia, sin
// solenoide físico; el robot acelera hacia adelante con rampa).
// ============================================================
constexpr int MIX_KICK_VEL_FINAL    = 240;  // velocidadFinalPateo
constexpr int MIX_KICK_PASO         = 20;    // pasoPateo (incremento de PWM)
constexpr int MIX_KICK_INTERVALO_MS = 10;   // intervaloPateo (ms entre incrementos)

// Retroceso de patada (PWM crudo por motor) — port 1:1 del 2025.
constexpr int MIX_PATAD_M1 = 250;  // patadM1
constexpr int MIX_PATAD_M2 = 170;  // patadM2

// ============================================================
// Candidato de ÓRBITA 2026 para centrar_*  (detrás de flag, APAGADO por defecto).
//
// PROBLEMA: el centrar_*() actual (port 2025) decodifica a CASI PURO STRAFE en la
// geometría de ruedas 2026 (vx≈±64, ω·R≈±8) → orbita mal: la pelota se escapa de
// costado. Verificado por análisis de cinemática (decode contra
// src/shared/kinematics.cpp; workflow 2026-06-19, 6 agentes, 0 refutaciones fatales).
//
// CANDIDATO: rebalancea a rotación-dominante MANTENIENDO la misma dirección de
// órbita (horario = strafe-IZQ + giro CW) y supera los pisos físicos {70,70,107}.
// Valores DIRECTOS de PWM por rueda (centralmix es mixer-free): delanteras (M1,M2)
// = MIX_CENTRAR_FRONT, trasera (M3) = MIX_CENTRAR_REAR (signo opuesto a las delanteras).
// Default [80,80,170] → vx≈-60 (strafe IZQ), ω·R≈+110 (CW), ratio ω/strafe≈1.83.
//
// ⚠️ TRADE-OFF DE 3 VÍAS (no hay óptimo libre): piso delantero (≥70) vs ratio de
// órbita ideal (≈R/d≈1.0) vs TECHO TÉRMICO (~150 PWM sostenido; motores brushed
// 5V@7.4V se queman >~70%). Una órbita geométricamente perfecta (ratio≈1.0) forzaría
// la trasera a ≥200 (quema). Por eso el default sobre-rota un poco (la corrección
// APUNTAR_PELOTA del FSM recoge la deriva residual). Trasera=170 ya está algo sobre
// el techo → usar órbitas CORTAS y vigilar temperatura.
//
// BENCH-TUNING (Elías): MEDIR d (distancia centro-pelota) y ajustar SOLO estas 2:
//   - pelota deriva hacia ADENTRO del giro → sobra rotación → BAJAR MIX_CENTRAR_REAR.
//   - pelota se va de COSTADO (afuera)     → falta rotación → SUBIR MIX_CENTRAR_REAR.
//   - si la trasera calienta → bajar REAR hacia 150 (acepta más sobre-rotación).
//
// ⚠️ NO TESTEADO EN HARDWARE. El sentido físico de R1 (izq/der, CW/CCW) se CONFIRMA en
// banco (config_central.h marca el +180 de traslación R1 "A VERIFICAR"). Si orbita al
// revés: intercambiar etiquetas horario↔antihorario en el FSM, NO tocar los signos acá.
// Se habilita con -DMIX_CENTRAR_ORBIT_2026 en build_flags (ver platformio.ini).
// ============================================================
constexpr int MIX_CENTRAR_FRONT = 80;   // M1,M2 (delanteras) — ≥70 (piso físico)
constexpr int MIX_CENTRAR_REAR  = 170;  // M3 (trasera) — ≥107 (piso); ojo techo térmico ~150

// ============================================================
// KICKOFF / arranque — PRIMER estado del FSM (coach 2026-06-21, SIN flag: KICKOFF_SEEK es
// el arranque de TODAS las builds del centralmix). Pedido de Elías: al arrancar, si VE
// la pelota va hacia ella; si NO la ve, da un IMPULSO FUERTE y CORTO de MEDIALUNA (arco)
// para despegarse hacia el centro, y después cae a la búsqueda por giro (GIRANDO).
//
// La medialuna se arma por SUPERPOSICIÓN de las 2 bases que YA existen (en un omni las
// velocidades de rueda SUMAN linealmente, así que esto SÍ da un arco, no el casi-strafe
// del centrar_*):
//   AVANCE (base avanzar(): M1=+F, M2=-F, M3=0)  +  GIRO (base girar(): M1=M2=M3=+T)
//   ⇒  M1 = +F + dir·T ,  M2 = -F + dir·T ,  M3 = dir·T     (ver kickoff_medialuna()).
// F = cuánto AVANZA · T = cuánta CURVA · dir = lado (±1). FUERTE = F alto; CORTO = ARC_MS
// chico → térmicamente seguro aunque el PWM sea alto, porque dura poco.
//
// ⚠️ NO TESTEADO EN HARDWARE. En banco se confirma: (1) que curva para el lado correcto
// (si no, invertir MIX_KICKOFF_ARC_DIR); (2) F/T/duración; (3) que "hacia el centro" tenga
// sentido para tu lado de saque (sin pose absoluta el lado es FIJO → setearlo acá).
// ============================================================
constexpr int MIX_KICKOFF_ARC_PWD  = 1.0;  // F: componente de AVANCE (fuerte; > piso ~70)
constexpr int MIX_KICKOFF_ARC_CURV = 1.0;   // T: componente de GIRO/curvatura de la medialuna
constexpr int MIX_KICKOFF_ARC_DIR  = +1;   // lado de la curva (+1 / -1) — confirmar en banco
constexpr int MIX_KICKOFF_ARC_MS   = 50;  // duración del impulso (CORTO), en ms

// ============================================================
// Heading — control de rumbo del 2025 (error = currentYaw - initialYaw, kp=0.3).
// SELECTOR de fuente de heading (TRES modos, mutuamente excluyentes):
//   - DEFAULT (sin flag): BNO055 LOCAL en la CENTRAL (Wire @ 0x28), como el 2025.
//       ⚠️ R1 2026 NO tiene BNO local (los 2 BNO viven en el TOP) → este modo NO sirve
//       para R1: el read local falla y deja heading_valid=false. Sólo úsalo en una placa
//       que REALMENTE tenga un BNO en su propio bus.
//   - Con -DMIX_HEADING_SNAPSHOT: heading = BNO del TOP que llega por el WorldSnapshot
//       (Serial7), SIN tocar ningún BNO local. ESTE es el modo para R1 2026 (BNO del TOP
//       arreglado + heading validado en banco 2026-06-21; ver src/top/sensors_imu.cpp:279).
//   - Con -DMIX_HEADING_OTOS: heading = OTOS (otos_heading_deg de DOWN). Fallback SIN gyro
//       (era para cuando el BNO de R1 estaba "muerto"; hoy el BNO anda → preferí SNAPSHOT).
// mix_comm puebla mix_io.heading_deg desde la fuente elegida y deja SIEMPRE
// otos_heading_deg crudo disponible para diagnóstico/A-B.
// (SNAPSHOT y OTOS son excluyentes; mix_comm.cpp lo verifica con un #error.)
// ============================================================
constexpr float MIX_HEADING_KP = 0.3f;  // kp del control de rumbo 2025

#if defined(MIX_HEADING_OTOS)
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = true;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = false;
#elif defined(MIX_HEADING_SNAPSHOT)
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = false;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = true;   // BNO del TOP vía snapshot
#else
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = false;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = false;  // DEFAULT: BNO LOCAL (Wire@0x28)
#endif

// BNO055 LOCAL (solo relevante en el modo por defecto = BNO local en la CENTRAL).
constexpr int MIX_BNO055_I2C_ADDR = 0x28;

// ============================================================
// Comunicación — baud de los enlaces TOP/DOWN (referencia comm_top.cpp /
// comm_down.cpp: ambos a 230400). El 2025 usaba 19200 sobre Serial1 crudo; ese
// valor queda SOLO como nota histórica, NO se usa: centralmix consume frames
// proto.h de TOP/DOWN a 230400.
// ============================================================
constexpr long MIX_UART_BAUD = 230400;  // TOP (Serial7) y DOWN (Serial1)

}  // namespace mix
}  // namespace iitasoccer
