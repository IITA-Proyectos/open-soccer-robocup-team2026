---
task: 106
titulo: "Banco — validar Fase 1 del monitor USB de CENTRAL (4 pruebas; la 'S' es crítica)"
fecha: 2026-06-16
asignado: Gustavo + equipo (banco con robot real)
prioridad: P1
pedido-por: Gustavo Viollaz (2026-06-16, antes de dormir — PRIORIDAD de la próxima sesión)
relacionada: TASK-105 (telemetría CENTRAL, queda 2027), docs/firmware/PLAN-MONITOR-Y-CALIBRACION-CENTRAL.md, monitor-base
estado: pendiente de banco (firmware + app IMPLEMENTADOS y verificados host 2026-06-16, main 8fbb4dc; banco lo cierra el equipo)
criterio-cierre: Test 1-4 PASAN. Si Test 1 FALLA, el monitor NO va a competencia hasta corregir el ruteo de líneas.
---

# TASK-106 — Banco: validar Fase 1 del monitor USB de CENTRAL

Lo implementado (host-verificado, byte-neutro en competencia): la CENTRAL habla JSON
Lines por USB (monitor DORMIDO, gateado `-DCENTRAL_USB_MONITOR`) y la app `monitor-base`
la muestra como **3ª placa** (vistas **Cerebro** + **Salud CENTRAL**). Falta el banco.
**Regla dura: esto lo cierra el equipo, no Claude.** Plan completo en
`docs/firmware/PLAN-MONITOR-Y-CALIBRACION-CENTRAL.md`.

## Setup (una vez)
- Robot armado + **batería cargada** (no fuente de banco). CENTRAL cableada a TOP y DOWN.
- Flashear la CENTRAL con el binario que trae el monitor:
  - Delantero (R2): `pio run -e central_robot2_demo_bb -t upload`
  - Arquero (R1): `pio run -e central_robot1_arquero_demo_bb -t upload`
- **USB de la CENTRAL → la PC** (TOP/DOWN hablan con CENTRAL por UART, no necesitan USB).
- App: `cd tools/monitor-base` → `python -m monitor_base`

---

## ✅ Test 1 — LA 'S' NO FRENA EL ROBOT *(crítico — make or break)*
La app manda `STREAM ON` + `PING` cada 1 s; si la 'S' disparara STOP, el robot se
frenaría solo cada segundo. Es lo que se arregló (ruteo de líneas al parser ANTES del
handler `g/s/d/x`) y NO se pudo probar host.

1. CENTRAL sola alcanza. Monitor crudo: `pio device monitor -b 115200 -p COMxx`.
2. Tipeá **`g`** (GO) → robot arranca / motores activos.
3. **Cerrá el monitor crudo** y abrí **la app**. Dejá ~10 s con la app conectada.

- **PASA si:** `match` queda en **1** y `state` sigue evolucionando / motores siguen
  — el robot **NO** cae a STOP con la app conectada.
- **Regresión:** monitor crudo, **`s`** → SÍ frena; **`g`** → arranca; **`d`** → vuelca caja negra.
- **Si FALLA:** el ruteo quedó mal → reportar; **NO usar el monitor en partido** hasta corregir.

## ✅ Test 2 — Las 2 vistas muestran datos REALES y vivos
Con el robot andando (jugando o empujándolo a mano) + app conectada:
- **Cerebro:** `role` (ATK/GK), `state` cambia con la jugada, `match`, y **barras de PWM
  por rueda** que se mueven al acelerar/girar.
- **Salud CENTRAL:** TOP **verde** (fresh) con `fr` subiendo, DOWN verde con `valid`,
  `hdg` del OTOS, `loop_us`.
- **PASA si:** datos no-cero, vivos, coherentes con lo que hace el robot; `seq` ~20 Hz.

## ✅ Test 3 — La salud refleja la realidad (no es cosmética)
1. Andando, **desenchufá TOP→CENTRAL** (o apagá TOP) mirando **Salud CENTRAL**. Reconectá.
2. Repetí con **DOWN→CENTRAL**.
- **PASA si:** al desconectar TOP, `top.fresh` se pone **ROJO** en ~1 s y `age` sube; al
  reconectar, **verde**. Ídem DOWN (`line_fresh`). Prueba que los semáforos miden el enlace real.

## ✅ Test 4 — Competencia SIN cambios (byte-neutralidad)
1. Flasheá competencia (sin monitor): `pio run -e central_robot2 -t upload` (o `central_robot1`).
2. Corrida normal.
- **PASA si:** el robot se comporta **idéntico a antes** de este cambio (misma FSM, mismo
  movimiento). El monitor OFF no debe cambiar nada. (T1 byte-a-byte del `.hex` = opcional;
  el gateo `#ifdef` ya lo garantiza estructuralmente.)

---

## Opcionales (no bloquean)
- **Test 5 — loop-time:** con la app streameando, `loop_us` (max/ema) en Salud CENTRAL **no
  debe dispararse** vs sin la app. Si el `max_us` salta feo, reportar.
- **Test 6 — registrar las 2 CENTRAL:** conectar cada una y
  `python registrar_placa.py --robot 1 --board central` / `--robot 2 --board central`;
  verificar con `--list` (quedan las 6 placas).

---

## Documentación esperada (journal)
`journal/2026-06-1X-banco-monitor-central.md` con: resultado del **Test 1** (sí/no frena),
captura/video de las 2 vistas con datos reales, y qué falló si algo falló. Cerrar esta TASK
solo si Test 1-4 pasan.
