# TASK-221 — Validar en banco la pose XY heading-free del arquero (keeper_xy_walls)

- **Placa:** TOP (+ CENTRAL para el test con motores)
- **Asignado:** equipo (banco) — Gustavo / Enzo
- **Prioridad:** P2 (mejora del arquero; el arquero por línea ya anda y no se reemplaza)
- **Estado:** abierta
- **Depende de:** firmware ya en repo (commit de esta fecha). Env TOP `top_robot2_arquero_xywalls`.

## Qué validar
La pose XY heading-free por paredes (`src/shared/keeper_xy_walls.h`, gateada
`-DTOP_KEEPER_XY_WALLS`). Ya está host-testeada (test_keeper_xy_walls 12/12) y
verificada en vivo por serie en varias posiciones (ver journal 2026-06-19). Falta el
cierre formal en banco (regla del repo: HW lo cierra el equipo).

## Cómo
1. Flashear TOP: `pio run -e top_robot2_arquero_xywalls -t upload`.
2. **Estático (sin motores), 5 posiciones marcadas con cinta** en la zona de fondo →
   comparar `snap x/y` (monitor) vs metro. ⚠️ Rutear el **cable USB del tether LEJOS de
   los ToF laterales** (se lee como pared falsa; mejor por atrás/arriba).
   - Criterio: **±5 cm con la(s) pared(es) cercana(s) visible(s)**. Centrado → X puede
     dar "centro" por diseño (ambas laterales fuera de alcance).
3. **Oclusión:** tapar una pared con la mano a 30 cm → la pose no debe pegar un salto.
4. **Rotación:** rotar el robot in situ → la X/Y NO debe seguir siendo creíble (el
   supuesto "mira al frente" deja de valer). Anotar el comportamiento.
5. **Con motores (untethered), CENTRAL `central_robot2_arquero_pingpong_trim_yhold`:**
   confirmar que el arquero (a) **mantiene profundidad** (Y-hold con la `my_y` real) y
   (b) **acota la patrulla** a la banda central (X-bound, ±35 cm del centro de arranque)
   en vez de ir de arco a arco. Sin pose confiable → rebote por línea (fallback).

## Criterio de cierre
- Estático ±5 cm en las 5 posiciones (con pared cercana). 
- El arquero con motores se queda en la banda central frente a su arco y no se va de
  arco a arco. 
- Anotar si hace falta **congelar el consumo del XY durante el escape** (lo decide la CENTRAL).

## Escape / rollback
Reflashear `top_robot2_pri` (TOP) — el flag OFF deja el binario byte-idéntico al de competencia.
