# 2026-07-01 — Arquero: freno ACTIVO del retroceso sobre la línea (gateado) — TASK-120

**Autor:** Claude (Opus 4.8), sesión con María (arquero R2 / placa CENTRAL). Proceso: `superpowers:systematic-debugging` + skill `control-pid-zona-muerta` (obligatoria por ser cambio de control de movimiento).

## Síntoma (María)
Al VOLVER hacia atrás tras patear, el arquero "se pasa por arriba de la línea y no frena" → cruza / se mete al área.
Premisa de María: "no está leyendo la línea".

## Causa raíz (Fase 1 — verificada en código, NO de memoria)
La premisa era **parcialmente falsa** y lo marqué con evidencia:
- `PATEANDO_atras` **SÍ lee la línea** (`amix_fsm.cpp:358`, `if (linea()) → parar()`).
- Pero `parar()` (`amix_motors.cpp:64`) = INA=INB=0 con PWM 0 = **RUEDA LIBRE (coast)**, no freno. El robot venía
  en reversa a PWM 80 → al soltar, la **inercia lo cruza**; + latencia de la línea de DOWN.
- **No estaba ciego: la ve, suelta el motor y patina.** El golpe ya resolvía esto (al detectar línea va a
  `frenar_patada` = contra-empuje activo), pero el retroceso solo coasteaba.

## Fix (gateado `-DARQMIX_RETRO_BRAKE_ON_LINE`, env `central_robot2_arqueromix_retrofreno`)
Espejo exacto de `frenar_patada`, al revés: al detectar línea en `PATEANDO_atras` → estado nuevo
`frenar_retroceso` → primitiva nueva `frenar_adelante()` (contra-empuje ADELANTE ~200 PWM × 250 ms, plugging)
para matar la inercia de la reversa y parar SOBRE la línea → sigue a `acomodar_linea`. Valores de arranque =
los del freno de patada (probados). Por safety/enlace DOWN caído sigue derecho a acomodar (sin línea que frenar).

Archivos: `amix_config.h` (constantes), `amix_motors.{h,cpp}` (`frenar_adelante`), `amix_fsm.h` (estado),
`amix_fsm.cpp` (transición + case), `platformio.ini` (env). **TODO bajo `#ifdef`.**

## Verificación (Fase 4 — host, NO reemplaza el banco)
- `central_robot2_arqueromix_retrofreno` → compila **SUCCESS** (FLASH code 16776).
- `central_robot2_arqueromix_quieto` (validado) → **byte-idéntico**, md5 `8D0168CF81F7CDAF347B0AB89E030E59`
  (sin cambio) → el fix está 100% aislado en el env de banco.
- ⚠️ NO hay test host para esta FSM (Arduino: `millis`/`analogWrite`) → **la validación del EFECTO es 100% de
  banco, la cierra el equipo** (regla #1). Compila ≠ frena de verdad.

## Iteración de banco (fix #2, mismo día) — "se sigue metiendo" → retroceso HASTA la línea
Banco (María): con `_retrofreno` se sigue metiendo "no siempre pero muchas veces". Re-análisis (systematic-
debugging, nueva evidencia): el freno sólo actúa si el retroceso PISA la línea, pero `PATEANDO_atras` tenía 2
salidas EXTRA (gate de frescura de DOWN + safety corto de 4 s) que lo cortaban ANTES → el freno no se disparaba.
El homing (`inicio_retroceder`) NO se mete porque sale SÓLO por línea (safety 50 s = nunca corta antes).
**Fix #2 (María):** post-pateo = incondicional como el homing → `retroceder_quieto()` hasta `linea()` → freno.
Sacados el gate de frescura y el safety de 4 s; queda una red anti-cuelgue grande (`AMIX_T_ATRAS_SAFETY_RETRO`
=50 s) SÓLO para no retroceder infinito si DOWN muere. Trade-off marcado en la TASK. `_quieto` byte-idéntico
re-verificado (md5 sin cambio); `_retrofreno` compila SUCCESS. Sigue SIN banco del efecto (lo cierra el equipo).

## Fix #3 (mismo día) — "avanzar hasta despegarse" (como el homing) + revisión adversarial
Pedido María (aprobado por diseño, brainstorming): al pisar la línea en el retroceso, en vez de FRENAR por tiempo,
AVANZAR al frente HASTA despegarse (`!linea()`), espejo del homing (`inicio_avanzar`). Se reemplazó el estado
`frenar_retroceso` por `escapar_adelante` (avanza `avanzar_inicio` PWM 75 hasta `!linea()`, reusa constantes del
homing) y se eliminaron `frenar_adelante` + `AMIX_FRENO_RETRO_*`. Todo gateado.

**Revisión adversarial (workflow, 4 lentes + verificación, ultracode)** — 2 hallazgos confirmados, 0 falsos:
- **#1 GRAVE (corregido):** el fix #2 (sacar el gate de frescura del retroceso) creó una regresión: `apply_down_line`
  (`amix_comm.cpp:139`) nunca resetea `line_present` al perder el enlace → si DOWN muere a mitad del retroceso,
  `line_present` congela en false → retroceso CIEGO hasta 50 s → se sale de la cancha (la rama sin flag cortaba en
  ~4 s). **Yo le había minimizado este riesgo a María** ("no dispara en juego"); el review mostró que una caída de
  UART (EMI de motores, falla conocida) sí lo dispara. FIX: re-agregado el corte `!down_link_fresh → acomodar_linea`
  en el retroceso gateado (solo actúa si el sensor muere; en juego sigue "hasta la línea").
- **#2 LEVE (P2):** con `line_present` congelado en true, `escapar_adelante` avanza ~1200 ms al campo sin
  realimentación (igual que el homing, dirección segura, acotado). Documentado en TASK-120 para titrar; no se tocó.

Verificación final: `_retrofreno` compila SUCCESS; `_quieto` (`8D0168…E59`) y `_kickcorto` (`72F2516…9D0`)
byte-idénticos re-verificados tras cada cambio. Sigue SIN banco del efecto (lo cierra el equipo).

## Pendiente
TASK-120: probar en banco que PARA sobre la línea al volver del pateo (no cruza, no se va para adelante),
titrar `AMIX_FRENO_RETRO_PWM`/`_T_`, decidir promover o no. Ataca el momento #2 de las notas de TASK-119
("el escape no alcanza al volver del pateo"). Probar junto a `_kickcorto` y `_orientesc`.
