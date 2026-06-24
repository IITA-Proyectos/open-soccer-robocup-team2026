# centralconduce — CONDUCIR la pelota al arco (NO es el rodeo de Edge)

> **Qué es.** Una estrategia de delantero SIMPLE, distinta del rodeo: el robot **busca la pelota,
> la centra al frente, y la lleva al arco** empujándola, hasta quedar cerca; ahí apunta y patea.
> NO se posiciona detrás de la pelota respecto al arco (eso es `centraledge`). Solo la encuentra,
> la mantiene adelante, y la conduce.
>
> ⚠️ **Compila, pero NO está validado en banco.** Banco → **TASK-119**.

## La estrategia (pedido de Elías, 2026-06-23)
1. **Busca** la pelota (gira en el lugar).
2. **La centra al frente** (`CENTRAR`): gira hasta tener la pelota justo adelante, así al avanzar
   no se le escapa de costado.
3. **La lleva al arco** (`CONDUCIR`): avanza hacia la pelota (la mantiene adelante) y, cuando ya la
   tiene cerca, gira el frente hacia el **arco rival** para escoltarla hacia ahí.
   - Si **NO ve el arco**, la lleva hacia el **heading 0** (la dirección inicial del robot) nomás.
4. **A <40 cm del arco** (`APUNTAR_ARCO` → `PATEAR`): apunta al arco y patea (empuje por inercia).

```
KICKOFF → BUSCAR → CENTRAR → CONDUCIR → (a 40cm) APUNTAR_ARCO → PATEAR → RETROCEDER → BUSCAR
                                         (+ escape de línea DETECTA_LINEA_1/2/3)
```

## Diferencia con las otras carpetas
| Carpeta | Estrategia |
|---|---|
| `centralmix` | delantero 2025 (apuntar→avanzar→orbitar→patear). **Pristino.** |
| `centraledge` | rodeo estilo Edge **con** feedforward de velocidad (anticipa pelota en movimiento). |
| `centraledgefijo` | rodeo estilo Edge **sin** velocidad (posición pura). |
| **`centralconduce`** (esta) | **conducir** la pelota al arco — NO rodea, la lleva. |

Esta carpeta NO usa `mix_edge` (la curva de rodeo) — la borré, no aplica. Usa la primitiva
holonómica `mix_mover_vector` para avanzar hacia la pelota mientras gira el frente al arco.

## Cómo compilar
```
pio run -e central_robot1_mix_conduce -t upload
```
Compila **solo** `centralconduce/` + `shared/` con `-DMIX_ATTACK_EDGE`. Las demás carpetas y el
mix 2025 quedan intactos (cada una en su env).

## Perillas (mix_config.h, bloque CONDUCIR)
- `MIX_CONDUCE_CENTER_TOL_DEG` (12°) — cuándo la pelota está "al frente" (centrada).
- `MIX_CONDUCE_RECAPTURE_TOL_DEG` (35°) — si se va al costado, volver a centrar.
- `MIX_CONDUCE_HAVE_DIST_CM` (20) — "ya la tengo cerca" → recién ahí escolto al arco.
- `MIX_CONDUCE_KICK_GOAL_DIST_CM` (40) — a esa distancia del arco, apuntar y patear.
- `MIX_CONDUCE_AIM_TOL_DEG` (12°) — arco apuntado → patear.
- `MIX_EDGE_SPEED` / `MIX_EDGE_FACE_KP` / `MIX_EDGE_OMEGA_MAX` — avance y giro (signo de FACE_KP a banco).

## TODO de banco — TASK-119
Sentido de `mix_mover_vector`, signo de `MIX_EDGE_FACE_KP` (giro hacia arco/pelota/heading 0),
`MIX_CONDUCE_*` en cm/grados reales, y que la pelota no se escape al conducir. Diseño:
`journal/2026-06-23-centralmix-rodeo-estilo-edge.md`.
