// motor_kickstart.h — IMPULSO INICIAL anti-inercia (kickstart) — PURO, host-testeable.
//
// Por qué existe (Capa 2a)
// ------------------------
// Los motores DC con escobillas tienen rozamiento ESTÁTICO (stiction): al arrancar
// desde parado, el PWM de régimen a veces no alcanza para "romper" la inercia y el
// motor zumba sin girar (o arranca tarde, desviando el rumbo). El delantero/arquero
// 2025 lo resolvían con un IMPULSO INICIAL: un pulso de PWM ALTO y BREVE al arrancar,
// que después baja al PWM de régimen. NO es una rampa: es un escalón temporizado.
//
// Valor 2025 portado: factor 1.8 × 40 ms (arquero definitivo-arquero_6-9-2026
// :1018-1022 → M1=M2=1.8*50=90, M3=1.8*85=153). Ver
// docs/firmware/MOTION-CONTROL-HISTORICO.md §Apéndice ARQUERO 2025.
//
// ⚠️ EL ROBOT 2026 ES MÁS PESADO → el x10% va al PWM BASE (el caller ya manda el base
// 10% más alto), NO al factor 1.8. El factor y la ventana se mantienen del 2025.
//
// Cap de seguridad
// ----------------
// Los motores 2026 son DC 5V comunes a 7,4V: > ~70% de duty (~178/255) se queman.
// El boost SIEMPRE se recorta a cap_abs (default 153 = el M3 boosteado real del 2025,
// el pico más alto que el robot 2025 usó sin quemarse). El boost NUNCA sube de ahí.
//
// PUREZA (clave para host-test)
// -----------------------------
// La función es PURA: el TIEMPO entra como parámetro (ms_since_start) y el ESTADO
// (pwm_base por rueda) también. NO lee millis(), NO guarda estado, NO toca pines.
// El DUEÑO de la transición parado→comando y del cronómetro es el caller
// (motors_apply_command en src/central/motors_zircon.cpp). Así se compila y testea
// en host con g++, igual que motor_power_cap.h / kinematics.h.

#pragma once

namespace iitasoccer {

// Cap de seguridad por DEFECTO del boost (= el M3 boosteado del arquero 2025, 1.8*85).
// Por debajo del ~70% (~178) que quema los motores 5V a 7,4V. El caller puede pasar
// otro cap_abs; este default existe para que el módulo sea usable suelto en test.
constexpr int MOTOR_KICKSTART_DEFAULT_CAP = 153;

// El factor se expresa como ENTERO x10 (18 = 1.8) para no meter float en el path de
// motores (el repo ya usa enteros: centideg, PWM). factor_x10 = 10 ⇒ ×1.0 ⇒ no-op.
constexpr int MOTOR_KICKSTART_FACTOR_DEN = 10;

// Devuelve el PWM a aplicar a UNA rueda, dado:
//   pwm_base       : PWM de régimen YA calculado para esta rueda (signo = sentido).
//                    En el caller es el valor que sale de apply_pwm_floor().
//   ms_since_start : ms transcurridos desde la transición parado→comando (lo lleva
//                    el caller con millis()). Es el "cronómetro" del impulso.
//   window_ms      : ventana del impulso (40 ms en 2025). Si <= 0 → kickstart OFF.
//   factor_x10     : factor del boost ×10 (18 = 1.8). <= 10 (×1.0) → sin boost.
//   cap_abs        : tope DURO del |pwm| boosteado (seguridad anti-quemado).
//                    <= 0 → usa MOTOR_KICKSTART_DEFAULT_CAP.
//
// Comportamiento:
//   • Si pwm_base == 0                       → 0 (rueda parada, no se "inventa" arranque).
//   • Si ms_since_start >= window_ms         → pwm_base SIN tocar (ya pasó el impulso).
//   • Si dentro de la ventana (0..window-1)  → boost = pwm_base * factor_x10 / 10,
//                                              recortado a ±cap_abs, signo conservado.
//   • El boost NUNCA es menor (en magnitud) que pwm_base (defensivo).
//
// Gate: con window_ms<=0 o factor_x10<=10 la función es no-op (devuelve pwm_base).
// Eso permite dejarla cableada con default OFF → binario idéntico.
int motor_kickstart_pwm(int pwm_base, int ms_since_start,
                        int window_ms, int factor_x10, int cap_abs);

}  // namespace iitasoccer
