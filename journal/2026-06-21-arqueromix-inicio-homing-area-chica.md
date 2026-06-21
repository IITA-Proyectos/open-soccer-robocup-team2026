---
title: "arqueromix — nuevo inicio: homing al área chica (retroceder hasta la línea → avanzar a ciegas → patrullar)"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-feature
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 5ª iteración) — inicio con homing al área chica

## Reporte de Virginia
La rampa de velocidad del pateo quedó "dentro de todo bien". Ahora cambiar el INICIO del programa:
en vez de empezar a patrullar directo, el arquero tiene que:
1. ir **hacia atrás** hasta detectar el **blanco del área chica**,
2. cuando lo detecte, **avanzar un poco SIN leer los sensores**,
3. y a partir de eso, ponerse a patrullar.

## Qué se hizo
Se reemplazó el estado inicial `impulso_inicial` (strafe corto) por dos estados de HOMING en
`amix_fsm.cpp`:
- **`inicio_retroceder`** (INICIAL): `patear_atras()` (retroceso recto hacia el arco propio) hasta
  que `linea()` da true (DOWN ve el blanco del área). Safety `AMIX_T_INICIO_RETRO_SAFETY=4000 ms`
  para no retroceder contra la pared para siempre si nunca ve la línea.
- **`inicio_avanzar`**: `avanzar()` durante `AMIX_T_INICIO_AVANCE=400 ms`, **sin chequear la línea**
  a propósito (para despegarse del blanco). Después → `moverce_derecha` (patrulla).
- El gate `match_running` sigue: el homing arranca recién con el GO del árbitro.

Detalles: enum nuevo (`inicio_retroceder`/`inicio_avanzar`) en `amix_fsm.h`; constantes de tiempo
en `amix_config.h`. La velocidad del retroceso reusa `AMIX_ATRAS` (la del despeje). El primitivo
viejo `impulso_inicial_mov()` queda definido pero ya no se usa en el inicio.

## Verificación
- **Compila SUCCESS** `central_robot2_arqueromix` (FLASH ~20 KB).
- Competencia byte-idéntica (solo `src/arqueromix/`, build aislado).
- ⚠️ No validado en HW (compila ≠ anda).

## Pendiente (Virginia / banco)
- Probar el arranque: ¿retrocede y se frena al ver la línea del área? ¿avanza el poquito y patrulla?
- Tunear `AMIX_T_INICIO_AVANCE` (400 ms): cuánto se despega de la línea antes de patrullar.
- Si retrocede muy rápido y se pasa de la línea: bajar `AMIX_ATRAS` o pedir una velocidad propia
  para el homing (hoy comparte la del despeje).
- Confirmar que DOWN reporta `line_present` al tocar la línea del área chica (es el mismo blanco
  que cualquier línea; DOWN no distingue cuál — el arquero asume que la primera línea yendo atrás
  es la del área).

## Referencias
- `src/arqueromix/DOCUMENTACION.md §16` (el inicio nuevo). Iteraciones previas: §13-§15.
- Journals del día: `2026-06-21-arqueromix-*` (port, fix cámara, 180°, rampa+signo, este inicio).
