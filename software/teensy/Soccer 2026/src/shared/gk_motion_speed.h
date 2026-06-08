// gk_motion_speed.h — Velocidad de comando objetivo por movimiento del ARQUERO (PURO).
//
// POR QUÉ EXISTE
// --------------
// El firmware 2026 NO manda PWM directo: manda VELOCIDAD (vx/vy/omega) y la cinemática
// inversa la reparte a las 3 ruedas. Con los ángulos {330,210,90} un STRAFE puro (vx, vy=0,
// omega=0) produce:
//     wheel[M1 del-izq] = -vx·sin(330°) = +0.5·vx
//     wheel[M2 del-der] = -vx·sin(210°) = +0.5·vx
//     wheel[M3 trasera] = -vx·sin( 90°) = -1.0·vx   (|factor| = 1.0 = la rueda FUERTE)
// → el ratio trasera:delanteras es EXACTAMENTE 2:1, IGUAL al arquero 2025 (50:~100).
//
// Entonces, para reproducir los PWM del arquero 2025 (delanteras ~50 / trasera ~89, x1.1 = 55/98)
// NO hay que tocar la cinemática: alcanza con ELEGIR la velocidad de comando vx tal que el mapa
// velocidad→PWM (wheel_speed_to_pwm) caiga en esos PWM. Este helper hace esa cuenta inversa y,
// CLAVE, la CLAMPEA al cap de potencia (~150 PWM = ~70%; motores 5V a 7.4V se queman por encima).
//
// ⚠️ ACOPLE CON EL LAZO (Capa 3): la trasera-fuerte (rear 2× front) SOLO es segura con un lazo de
// heading corrigiendo el drift. En lazo abierto (BNO roto, sin OTOS) usar el piso balanceado
// {77,77,46}, NO estas velocidades altas. Ver docs/firmware/MOTION-CONTROL-PLAN-2026.md.
//
// PURO: sin Arduino/Wire, sin estado, solo float. Se testea en host (g++). NO cambia el binario:
// nadie lo llama hasta que se cableen las consts en strategy.cpp (gateado -DGK_PWM_2025_TUNE).

#pragma once

namespace iitasoccer {

// Geometría del strafe con WHEEL_ANGLES_DEG={330,210,90} (ver derivación arriba).
// Son los |factores| de velocidad de rueda por unidad de vx en un strafe puro.
constexpr float GK_STRAFE_FRONT_FACTOR = 0.5f;   // delanteras (M1/M2) — oblicuas
constexpr float GK_STRAFE_REAR_FACTOR  = 1.0f;   // trasera (M3) — paralela = la fuerte

// Cap de potencia: PWM máximo seguro por rueda (~70% de 255). Espejo del cap E1.
constexpr int   GK_PWM_CAP             = 150;

// Velocidad de comando (mm/s) cuyo strafe produce `front_pwm` en las DELANTERAS,
// dado el mapa lineal velocidad→PWM del pipeline (wheel_speed_to_pwm):
//     front_pwm = (FRONT_FACTOR · vx / max_speed) · max_pwm
//  => vx = front_pwm · max_speed / (FRONT_FACTOR · max_pwm)
// Luego se CLAMPEA para que la rueda DOMINANTE (la trasera, factor 1.0) no pase pwm_cap.
float gk_strafe_speed_from_front_pwm(int front_pwm, float max_speed_mm_s, int max_pwm,
                                     int pwm_cap = GK_PWM_CAP);

// Aplica el clamp del cap a una velocidad de strafe YA elegida: si a esa vx la rueda
// trasera (factor 1.0) supera pwm_cap, devuelve la vx máxima que deja la trasera en el cap.
// Sign-preserving (sirve para strafe a izquierda, vx<0).
float gk_clamp_strafe_speed_to_cap(float vx_mm_s, float max_speed_mm_s, int max_pwm,
                                   int pwm_cap = GK_PWM_CAP);

// Velocidad de AVANCE (vy, marco robot +Y) cuyo avance produce `front_pwm` en las
// delanteras. En avance la trasera (factor cos(90°)=0) queda APAGADA, igual que el
// arquero 2025 (avanzar = 100/100/0). Factor delanteras en avance = |cos(330°)| = 0.866.
float gk_forward_speed_from_front_pwm(int front_pwm, float max_speed_mm_s, int max_pwm,
                                      int pwm_cap = GK_PWM_CAP);

}  // namespace iitasoccer
