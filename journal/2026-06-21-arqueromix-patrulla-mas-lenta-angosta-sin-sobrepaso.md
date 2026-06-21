---
title: "arqueromix — patrulla más lenta + angosta + sin sobrepaso a la izquierda (tuneo de constantes)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco) + workflow paralelo"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: tuneo-banco
---

# arqueromix — patrulla más lenta, angosta, sin sobrepaso izquierdo

## Pedido (Virginia)

"Funciona, pero se pasa a veces en lo lateral, sobre todo a la IZQUIERDA. Hacelo más lento y más
centrado; patrulla lenta y corta mirando al arco rival. Que tenga estado de centrar pelota (lejos) y
patear (cerca). Mínimos cambios. Solo bajar velocidad y amplitud lateral y revisar esos 2 estados."

## Workflow paralelo (2 verificadores + síntesis)

**Confirmado: los 2 estados YA EXISTEN** (no se programó nada nuevo):
- **Centrar pelota (lejos):** en `moverce_*`, si `|angulo_pelota| > 8°` (AMIX_TOL_CENTRADO_DEG) strafe
  hacia su lado para centrarla en la cámara delantera (`amix_fsm.cpp:163-256`).
- **Patear (cerca):** `ball_para_despejar()` = `dist ≤ 250mm && |áng| ≤ 30°` → secuencia de patada
  (`amix_fsm.cpp:61-65` + `280-348`).
- **Mirar al arco rival:** ya se cumple por el rumbo (heading sellado al GO + corrección en el strafe
  + ALINEAR_arco_opp antes de patear). No hace falta nada.

**Causa del "se pasa a la izquierda" (hallazgo):** la corrección de rumbo a la izquierda era **2.5×**
la de la derecha (`AMIX_AI_REAR_ENEG=100` vs `AMIX_AD_REAR_ENEG=40`).

## Cambios (SOLO constantes en amix_config.h — cero lógica nueva)

| Constante | Viejo → Nuevo | Qué hace |
|---|---|---|
| `AMIX_PD_BASE` | 1.0 → **0.85** | Patrulla -15% más lenta (sin caer en zona muerta; NO bajar de 0.80) |
| `AMIX_AI_REAR_ENEG` | 100 → **75** | Mata la asimetría que sobrepasaba a la IZQUIERDA (2.5×→1.875×) |
| `AMIX_T_SALIR_LINEA` | 450 → **350** | Rebote más corto → menos sobrepaso |
| `AMIX_TOL_ARCO_OWN_DEG` | 20 → **15** | Patrulla más angosta/centrada al arco |

**NO se bajó `AMIX_PD_SALIR` (1.9)** — el análisis avisó que bajarlo JUNTO con `T_SALIR_LINEA` puede
dejar al arquero pegado a la línea. Se deja para banco si todavía sobrepasa.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.
- Checkpoint guardado: tag `arqueromix-funciona-angosta-profundidad-2026-06-21`.

## Plan de banco (Virginia)

1. Flashear, GO, mirar la patrulla: ¿más lenta, más angosta, **dejó de pasarse a la izquierda**?
2. ¿Se traba / espasmódica? → la velocidad quedó muy baja: subir `AMIX_PD_BASE` 0.85→0.90.
3. ¿Se queda pegado a la línea al rebotar? → `AMIX_T_SALIR_LINEA` 350→400.
4. ¿Ahora se pasa a la DERECHA? → `AMIX_AI_REAR_ENEG` 75→85.
5. ¿Todavía se pasa (cualquier lado)? → recién ahí bajar `AMIX_PD_SALIR` 1.9→1.5.
6. Pelota: lejos descentrada → ¿strafe a centrarla? cerca al frente → ¿patea? (si no patea, `AMIX_TOL_CERCANIA_MM` 250→300).

## Archivos

- `amix_config.h` (4 constantes). Cero lógica nueva. Los 2 estados de pelota ya existían.
