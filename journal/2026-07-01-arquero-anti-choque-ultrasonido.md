# 2026-07-01 — Arquero: ANTI-CHOQUE "frenar y esperar" con el ultrasonido (gateado) — TASK-121

**Autor:** Claude (Opus 4.8), sesión con María (arquero R2 / placa CENTRAL, Incheon). Proceso: `superpowers:brainstorming`
(diseño con María, aprobado) → implementación gateada → revisión adversarial (workflow, ultracode) → fixes.

## Pedido
"Que el arquero no llegue a chocar con ningún oponente." Basado en el `EVITAR_OBSTACULO` de Elías (rama
`ultrasonido`, centralmix), adaptado: María eligió **"frenar y esperar"** (no esquivar, no retroceder), y que el
freno **también pause el despeje**.

## Qué se hizo (gateado `-DARQMIX_AVOID_OBSTACLE`, env `central_robot2_arqueromix_evita`)
- Exponer `obstacle_mm` (de `s.min_obstacle_mm` del snapshot del TOP) en `amix_io`/`amix_comm` (campo gateado →
  `_quieto`/`_kickcorto`/`_retrofreno` byte-idénticos).
- Guard arriba del switch: `if (obstaculo_cerca() && estado != frenar_patada) { millis_inicio_estado = millis(); parar(); return; }`.
- Umbral `ARQMIX_OBST_STOP_MM=150` (0 = kill-switch). NO se toca el TOP.

## Revisión adversarial (4 lentes + verify) — 4 hallazgos reales, todos atendidos
- **#1 (grave):** guard sin pausar el timer → congelar durante el golpe salteaba el despeje (timer vencido +
  `parar()` reseteaba la rampa). FIX: `millis_inicio_estado = millis()` en el guard.
- **#2 (medio):** igual con `ALINEAR_arco_opp` → despeje sin apuntar. Mismo fix.
- **#3 (grave):** el freno preemptaba `frenar_patada` (freno activo del golpe) → `parar()` coastea → la inercia
  empujaba al arquero HACIA el obstáculo. FIX: excluir `frenar_patada` del guard.
- **#4 (grave):** el comentario "en R2 los ToF están sin usar" era **falso a nivel firmware** — `top_robot2_pri`
  trae `-DTOP_ENABLE_MULTI_TOF` y `min_obstacle_mm = min(4 ToF + HC-SR04)`. Si un ToF ve la pelota, el arquero
  frena en vez de despejar. FIX: comentarios corregidos (`amix_io`/`amix_config`/`platformio.ini`); el supuesto
  ("solo ultrasonido" ⟺ ToF deshabilitados en EEPROM del TOP) queda como CHEQUEO BLOQUEANTE de banco.
- Honestidad: **yo le había minimizado a María el tema de la pelota** ("el ultrasonido no ve la pelota"); el
  supuesto sólo se cumple si los ToF están deshabilitados en el TOP — hay que verificarlo. Documentado en TASK-121.

## Verificación (host, NO reemplaza el banco)
`_evita` compila SUCCESS; `_quieto` (`8D0168…E59`), `_kickcorto` (`72F2516…9D0`), `_retrofreno` (`F41B8D1…4D2`)
byte-idénticos re-verificados tras cada cambio. NO validado en banco.

## Pendiente (TASK-121)
Banco: (1) BLOQUEANTE — que la PELOTA no dispare el freno (leer `snap_min_obstacle_mm` acercando solo la pelota;
si dispara → deshabilitar/enmascarar ToF frontales en el TOP). (2) Que frene con un robot. (3) Despeje con
obstáculo (fixes #1/#3). (4) A/B / kill-switch. Decisión de promover / combinar con `_retrofreno`.
