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
constexpr float MIX_G  = 0.3f;   // girando
constexpr float MIX_A  = 0.4f;   // apuntando pelota
constexpr float MIX_C  = 0.4f;   // centrando (2025 ROBOT2)
constexpr float MIX_IC = 0.55f;  // impulso centrando (2025 ROBOT2)
constexpr float MIX_PD = 1.0f;   // avances proporcionales (2025 'pd')

// ============================================================
// Tolerancias 2025 (en mm para cercanía/centrado; en grados para apuntado).
// ============================================================
constexpr float MIX_TOL_CENTRADO = 30.0f;  // tolerancia_centrado
constexpr float MIX_TOL_CERCANIA = 50.0f;  // tolerancia_cercania
constexpr float MIX_TOL_APUNTADO = 15.0f;  // tolerancia_apuntado (grados)

// ============================================================
// Kicker / patada — port 1:1 del 2025 (el "kicker" es empuje por inercia, sin
// solenoide físico; el robot acelera hacia adelante con rampa).
// ============================================================
constexpr int MIX_KICK_VEL_FINAL    = 240;  // velocidadFinalPateo
constexpr int MIX_KICK_PASO         = 5;    // pasoPateo (incremento de PWM)
constexpr int MIX_KICK_INTERVALO_MS = 20;   // intervaloPateo (ms entre incrementos)

// Retroceso de patada (PWM crudo por motor) — port 1:1 del 2025.
constexpr int MIX_PATAD_M1 = 250;  // patadM1
constexpr int MIX_PATAD_M2 = 170;  // patadM2

// ============================================================
// Heading — control de rumbo del 2025 (error = currentYaw - initialYaw, kp=0.3).
// SELECTOR de fuente de heading:
//   - DEFAULT: BNO055 (como el 2025).
//   - Con -DMIX_HEADING_OTOS: usa el heading del OTOS (otos_heading_deg de DOWN).
// mix_comm debe poblar mix_io.heading_deg desde la fuente elegida y dejar SIEMPRE
// otos_heading_deg crudo disponible para diagnóstico/A-B.
// ============================================================
constexpr float MIX_HEADING_KP = 0.3f;  // kp del control de rumbo 2025

#ifdef MIX_HEADING_OTOS
constexpr bool MIX_HEADING_SOURCE_IS_OTOS = true;
#else
constexpr bool MIX_HEADING_SOURCE_IS_OTOS = false;  // DEFAULT: BNO
#endif

// BNO055 (solo relevante si la fuente es BNO).
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
