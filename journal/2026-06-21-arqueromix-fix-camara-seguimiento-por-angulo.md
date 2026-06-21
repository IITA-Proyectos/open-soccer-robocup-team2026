---
title: "arqueromix — FIX: seguimiento de pelota por ÁNGULO (la cámara veía pero el robot no se movía)"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-bugfix
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco) — FIX del seguimiento de pelota del arqueromix

## Reporte de Virginia
Los motores del arqueromix andan bastante bien, pero **el seguimiento de la pelota NO**: la cámara
detecta (su LED prende) y aun así el robot no hace ningún movimiento para seguir/despejar.

## Debugging sistemático (causa raíz, comparando con centralmix como pidió Virginia)
- El LED prende en la CÁMARA → la cámara ve la pelota. Pero eso NO prueba que el dato llegue a la
  FSM. La cadena es cámara→TOP→WorldSnapshot→CENTRAL(Serial7)→`g_aio`. Como el delantero
  (`centralmix`) SÍ sigue la pelota con la MISMA capa de comm, el dato llega → el bug está en la
  **lógica/umbrales del FSM**, no en el comm.
- **Diferencia clave centralmix vs arqueromix:** `centralmix` calcula `angulo_pelota_deg =
  atan2(ball_x, ball_y)` y sigue la pelota por su **ÁNGULO** (robusto a la escala). `arqueromix`
  decidía con `ball_x_mm`/`ball_y_mm` **crudos** y umbrales en **mm** (CENTRADO=30/DESVIO=50/
  CERCANIA=140).
- **Causa raíz:** la escala del snapshot está SIN CALIBRAR (`CAMERA_UNIT_TO_MM=10` en el TOP). Con
  esos umbrales en mm, una pelota más o menos al frente caía en la **banda muerta** (ni
  cerca+centrada ni desviada) → el FSM hacía `parar()`, que pisaba la patrulla → **el robot se
  CONGELABA justo al ver la pelota.** Exactamente el síntoma reportado.

## Fix (siguiendo cómo usa la cámara el delantero)
- `amix_io.h`: campo nuevo `angulo_pelota_deg`.
- `amix_comm.cpp`: lo calcula `atan2f(ball_x, ball_y)*180/PI` (igual que `mix_comm`).
- `amix_config.h`: umbrales mm → **ángulo**: `AMIX_TOL_CENTRADO_DEG=8°` (banda muerta angular
  angosta), `AMIX_TOL_KICK_DEG=30°` (despeje), `AMIX_TOL_CERCANIA_MM=250` (distancia euclídea para
  despejar; knob de tuning porque la escala está sin calibrar).
- `amix_fsm.cpp`: `moverce_der/izq` ahora siguen por ÁNGULO: `|áng|>8°` → strafe al lado de la
  pelota (la SIGUE); alineada y lejos → mantiene posición; `dist≤CERCANIA_MM && |áng|≤30°` →
  despeja. Se elimina la banda muerta en mm que causaba el freeze.

## Verificación
- **Compila SUCCESS** (`pio run -e central_robot2_arqueromix`, FLASH ~19,6 KB).
- ⚠️ **No validado en hardware** (compila ≠ anda). El plan de banco está en `DOCUMENTACION.md §13`
  y en TASK-114 (mover la pelota izq/der y ver que el arquero la sigue; subir `CERCANIA_MM` si
  nunca despeja). El signo del strafe (`ball_a_la_derecha`, ang>0=derecha) lo cierra el banco.
- Competencia byte-idéntica (solo `src/arqueromix/`, build aislado).

## Pendiente (Virginia / banco)
- Probar el seguimiento: pelota a derecha → strafe derecha; a izquierda → izquierda.
- Tunear `AMIX_TOL_CERCANIA_MM` (250) para que dispare el despeje a la distancia justa.
- Si el strafe va al lado contrario de la pelota → invertir `ball_a_la_derecha()`.

## Referencias
- `src/arqueromix/DOCUMENTACION.md §13` (diagnóstico + plan de banco del fix).
- Cómo usa la cámara el delantero: `src/centralmix/mix_comm.cpp` (`angulo_pelota_deg`) + `mix_fsm.cpp`.
- Journal del port base: `journal/2026-06-21-arqueromix-port-arquero-2025.md`.
