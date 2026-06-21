---
title: "arqueromix — avance del homing: bajar fuerza + invertir sentido (banco Virginia)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix — avance del homing: menos fuerza + sentido invertido

## Reporte de Virginia (banco, 2ª pasada sobre el avance del homing)

> "Va con mucha fuerza luego de detectar la línea y moverse sin leer los sensores. Ese movimiento
> de moverse sin leer lo hizo para el lado contrario y con demasiada fuerza: bajale y ponelo para
> el lado contrario."

Es el `inicio_avanzar` → `avanzar_inicio()` (movimiento a ciegas tras el homing). Va con demasiada
fuerza y en el sentido equivocado.

## Cambios

1. **Fuerza:** `AMIX_INICIO_AVANCE_PWM` **90 → 75** (`amix_config.h`). 75 queda apenas sobre el piso
   de las delanteras (`MOTOR_MIN_PWM=70`) → si no arranca, subir hacia 85; NO bajar a 70 (zona muerta).
2. **Sentido:** nuevo `AMIX_INICIO_AVANCE_SIGN`, default **−1** (invertido respecto del original).
   `avanzar_inicio()` ahora hace `p = PWM × SIGN; M1=+p, M2=−p, M3=0`. Flag `-DARQMIX_FLIP_INICIO_AVANCE`
   vuelve al +1 viejo (diagnóstico).

## ⚠️ Contradicción a vigilar en banco (marcada, no tapada)

Al invertir el avance (SIGN=−1), `avanzar_inicio()` queda con el **mismo patrón que `retroceder_inicio()`**
(el retroceso del homing). Es decir: el "despegue" ahora va en el MISMO sentido que el retroceso que
lo trajo a la línea. Geométricamente eso podría NO despegar (empujar de vuelta contra la línea). PERO
Virginia observó en banco que el sentido anterior iba para el lado contrario → se respeta su observación
directa (ella ve el robot; yo no). **Cierre en banco:** confirmar que con SIGN=−1 el robot SÍ se despega
de la línea antes de patrullar. Si no, es un tema geométrico (revisar qué eje produce realmente
`M1=+,M2=−` en este robot) y se replantea — el flag `-DARQMIX_FLIP_INICIO_AVANCE` deja probar ambos.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS** (FLASH ~16 KB).
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco (regla #1). No cierra TASK-114.

## Cómo verificar (Virginia)

1. Flashear `central_robot2_arqueromix`. Al GO: retrocede → detecta línea → impulso breve **al sentido
   nuevo, más suave**. ¿Va para el lado correcto y sin tanta fuerza?
2. ¿Se despega de la línea? Si re-detecta la línea al patrullar → subir `AMIX_T_INICIO_AVANCE` (200→250).
3. ¿Todavía va para el lado contrario? → flashear `central_robot2_arqueromix` + el sentido viejo:
   build con `-DARQMIX_FLIP_INICIO_AVANCE` (o pedírmelo y te armo el env).
4. ¿No arranca el avance (se queda)? → PWM corto, subir `AMIX_INICIO_AVANCE_PWM` (75→85).

Todo en `amix_config.h`. NO se tocó el despeje ni el binario de competencia (build aislado).

## Archivos tocados

- `src/arqueromix/amix_config.h` — `AMIX_INICIO_AVANCE_PWM` 90→75 + nuevo `AMIX_INICIO_AVANCE_SIGN` (−1).
- `src/arqueromix/amix_motors.cpp` — `avanzar_inicio()` aplica el signo.
- `src/arqueromix/DOCUMENTACION.md` — §16 + "Tunear" actualizados.
