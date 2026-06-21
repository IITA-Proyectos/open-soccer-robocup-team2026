---
title: "arqueromix — el avance del homing pasa a ser un IMPULSO momentáneo (no PWM sostenido)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix — avance del homing = impulso breve (banco Virginia 2026-06-21)

## Reporte de Virginia (banco)

> "Al dar el GO sí va para atrás, pero lo que sucede es que el impulso luego de ir hacia adelante
> provoca que el robot vaya chueco y detecta la línea del área chica. Necesito que le cambies el
> impulso ya que esa potencia la mantiene siempre y provoca que el robot no vaya prolijo — es solo
> un momento que lo debe de tener."

- El **retroceso** del homing (`inicio_retroceder`) anda: va para atrás y detecta la línea del área. ✅
- El **avance** posterior (`inicio_avanzar`) era el problema.

## Diagnóstico (verificado contra el código, no de memoria)

`inicio_avanzar` llamaba `avanzar()` = **M1=+100, M2=−100, M3=0 SOSTENIDO durante 400 ms**
(`AMIX_T_INICIO_AVANCE=400`, `amix_fsm.cpp:114` + `amix_motors.cpp:126`).

En el omni-3, un empuje recto con la trasera apagada (sin corrección de rumbo — el avance es a
ciegas a propósito) **acumula deriva de yaw**: cuanto más tiempo se mantiene la potencia, más se
tuerce. A los 400 ms ya se va chueco y termina rozando de nuevo la línea del área. Exactamente lo
que describe Virginia: "esa potencia la mantiene siempre" → debe ser "solo un momento".

## Fix

El avance del homing pasa de PWM sostenido a **IMPULSO MOMENTÁNEO**, con primitiva y PWM dedicados
(mismo patrón que ya tiene el retroceso `retroceder_inicio()` con su `AMIX_INICIO_RETRO_PWM`):

1. **`AMIX_T_INICIO_AVANCE` 400 → 200 ms** (`amix_config.h`). El cambio central: la potencia se tiene
   un toque, no sostenida → no le da tiempo a torcerse.
2. **Primitiva dedicada `avanzar_inicio()`** (`amix_motors.cpp/.h`) con PWM propio
   **`AMIX_INICIO_AVANCE_PWM=90`** — desacoplada del `avanzar()=100` del despeje (que NO se toca).
3. `inicio_avanzar` ahora llama `avanzar_inicio()` (`amix_fsm.cpp`).

**Decisión de diseño marcada:** el PWM se deja en **90**, NO en 70. 70 es el piso de las ruedas
delanteras (`MOTOR_MIN_PWM={70,70,107}`); ahí andan en zona muerta y se irían MÁS chuecas. Lo que
mata la deriva es que sea BREVE, no flojo. 90 = suave con margen sobre el piso.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS** (FLASH code 16392, data 4040).
- ⚠️ **Compila ≠ anda.** Lo cierra el equipo en banco (regla #1). Esto NO cierra TASK-114.

## Cómo verificar en banco (Virginia)

1. Flashear `central_robot2_arqueromix`. Al GO: retrocede → detecta línea → **breve impulso al frente**
   → patrulla. ¿Va más prolijo (menos chueco)?
2. Si **NO se despega** de la línea y la re-detecta al empezar a patrullar → subí `AMIX_T_INICIO_AVANCE`
   (200→250…) — más impulso, sigue siendo breve.
3. Si todavía va chueco → es la deriva del omni; achicá un poco más el tiempo o bajá apenas
   `AMIX_INICIO_AVANCE_PWM` (90→85), nunca a 70.
4. Si **no arranca** el avance (se queda) → el PWM quedó corto, subí `AMIX_INICIO_AVANCE_PWM` hacia 100.

Todo en `amix_config.h`. NO se tocó la secuencia de despeje ni el binario de competencia (build aislado).

## Archivos tocados

- `src/arqueromix/amix_config.h` — `AMIX_T_INICIO_AVANCE` 400→200 + nuevo `AMIX_INICIO_AVANCE_PWM=90`.
- `src/arqueromix/amix_motors.{h,cpp}` — primitiva `avanzar_inicio()`.
- `src/arqueromix/amix_fsm.cpp` — `inicio_avanzar` usa `avanzar_inicio()`.
- `src/arqueromix/DOCUMENTACION.md` — §16 + diagrama + "Tunear" actualizados.
