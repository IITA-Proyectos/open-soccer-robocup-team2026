---
title: "Delantero R1: LINE_AVOID con 'despegue real con margen' replicado del arquero"
date: 2026-06-14
author: Claude (Opus 4.8)
requested-by: Elías Cordero
status: SIN VALIDAR EN BANCO — solo compila SUCCESS
tipo: cambio-firmware
env: central_robot1_delantero_practica_bb
---

# Delantero R1 — evasión de línea robusta (patrón del arquero)

## Problema reportado (Elías, banco 2026-06-14)

El delantero, al encontrar una línea blanca, **quedaba trabado** y "constantemente
intentaba salir"; a veces hacía **curvas aleatorias** al sacarlo a mano.

## Diagnóstico

El LINE_AVOID viejo retrocedía un **tiempo CIEGO fijo** (`ATK_LINE_AVOID_DURATION_MS = 4000`)
y salía a SEARCH aunque siguiera sobre la línea → re-disparaba en loop. Además, si entraba
rápido y se pasaba al **MEDIO** de una línea gruesa, **todos los sensores ven blanco
(saturación) → `line_detected` da false** → ni siquiera podía disparar el escape (queda
trabado en el blanco haciendo SEARCH/APPROACH = las "curvas").

## Qué se replicó del arquero (GK_ADVANCE, banco 2026-06-09)

El arquero ya resolvió esto con **"despegue real con margen"**: no sale por tiempo ciego,
sino que avanza hasta que la línea DEJA de verse y se mantiene limpia un margen (si la re-ve,
reinicia el margen → evita el loop escape↔toque). Se trajo el mismo patrón al delantero, en
`src/central/strategy.cpp` (estado `ATK_LINE_AVOID`):

- **Escape un MÍNIMO** `ATK_LINE_AVOID_MIN_MS = 800` ms → atraviesa la banda saturada
  "todo blanco" del medio (donde `line_detected` da false) en vez de creer que ya está limpio.
- **Despegue real:** después del mínimo, sigue escapando hasta que `!line_detected` se
  mantenga `ATK_LINE_CLEAR_MARGIN_MS = 300` ms; si re-ve la línea, reinicia el margen.
- **Tope de seguridad** `ATK_LINE_AVOID_MAX_MS = 3000` ms (no cruza toda la cancha).
- La dirección de escape sigue siendo `line_angle + 180` congelada al entrar (sin re-leer).
- Variable de estado nueva `g_atk_line_clear_ms`; se re-arma al entrar a LINE_AVOID.

## Lo que NO resuelve (a vigilar en banco)

⚠️ **Si la dirección de escape tiene el SIGNO invertido en R1** (nunca se validó en hardware,
igual que el yaw OTOS), el robot escaparía hacia la línea y saldría recién por el tope de 3 s.
En banco: con el monitor, al tocar la línea el estado debe ir a `ATK_LINE_AVOID` y el robot
**alejarse** de la línea. Si se acerca → invertir el signo (`+180` ↔ vector, o el mapeo vx/vy).

## Verificación hecha

- `pio run -e central_robot1_delantero_practica_bb` → **SUCCESS** (solo compila).
- **NO validado en hardware** (regla no negociable: lo cierra el equipo en banco).

## Plan de prueba (banco — pendiente)

1. Flashear `central_robot1_delantero_practica_bb` + DOWN con `down`. Monitor 115200.
2. Empujar el robot hacia una línea: el estado debe ir a `ATK_LINE_AVOID` y **alejarse**.
3. Meterlo al MEDIO de una línea gruesa (saturación): debe escapar igual (el MIN lo atraviesa),
   no quedarse trabado.
4. Confirmar que NO re-dispara en loop ni cruza toda la cancha.
5. Si escapa hacia la línea → invertir el signo del escape y re-probar.

## Tuning expuesto (strategy.cpp)

- `ATK_LINE_AVOID_MIN_MS` (800) · `ATK_LINE_CLEAR_MARGIN_MS` (300) · `ATK_LINE_AVOID_MAX_MS` (3000)
- `ATK_LINE_RETREAT_SPEED` (600 mm/s) — velocidad del escape.
