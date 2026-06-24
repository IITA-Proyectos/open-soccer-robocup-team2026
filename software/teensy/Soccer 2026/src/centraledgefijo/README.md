# centraledgefijo — rodeo estilo "Edge" POR POSICIÓN PURA (sin velocidad de pelota)

> **Qué es.** Igual que `centraledge` (el delantero reactivo estilo Team Edge: se pone detrás de la
> pelota con UNA fórmula de rodeo amplificado a full velocidad, mira al arco y empuja por inercia)
> **PERO sin usar la velocidad de la pelota**. El rodeo se calcula **solo con la POSICIÓN actual**
> de la pelota — la curva fija de amplificación de ángulo, como el rodeo SIMPLE de Edge.
>
> ⚠️ **Compila, pero NO está validado en banco.** Banco → **TASK-119** (mismas perillas, sin las de velocidad).

## Diferencia con `centraledge` (la única)
| | `centraledge` | `centraledgefijo` (esta) |
|---|---|---|
| Curva de rodeo (posición) | ✅ | ✅ (idéntica) |
| Anticipa la pelota en movimiento | ✅ con **velocidad** (proyección predictiva) | ❌ **no usa la velocidad** |
| Robustez al ruido de cámara / ego-movimiento | depende del tuneo del feedforward | más simple/robusta (no hay velocidad que ensucie) |

Es decir: misma forma de rodear y de empujar; lo único que cambia es que **acá NO hay feedforward
de velocidad**. `mix_edge.cpp` calcula el ángulo de avance con `mix_edge_wrap_angle(ángulo ACTUAL)`
y listo. (Los datos `ball_vx/vy` igual llegan del TOP y se ven en el debug USB, pero la estrategia
NO los usa.)

## Por qué existe (pedido de Elías)
Para tener la versión simple de Edge (sin velocidad) separada y poder compararla en banco contra
`centraledge` (con velocidad). `centralmix` (el mix 2025) queda PRISTINO; cada carpeta compila en su
propio env, nunca juntas.

## Cómo compilar
```
pio run -e central_robot1_mix_edge_fijo -t upload
```
Compila **solo** `centraledgefijo/` + `shared/` con `-DMIX_ATTACK_EDGE`. Versión con velocidad:
`central_robot1_mix_edge` (carpeta `centraledge`). Mix 2025: `central_robot1_mix_bno`.

## Archivos (igual que centraledge, salvo mix_edge sin velocidad)
- **`mix_edge.{h,cpp}`** — núcleo PURO: SOLO la curva de rodeo por posición + la decisión de empuje.
  Sin `ball_vx/vy`, sin params de velocidad. Host-test: `test/test_mix_edge_fijo/`.
- **`mix_fsm_edge.{h,cpp}`** — la FSM del rodeo (`KICKOFF → BUSCAR → RODEAR → EMPUJAR → RETROCEDER`
  + `DETECTA_LINEA_1/2/3`). Idéntica a centraledge salvo que arma el `EdgeIn` sin velocidad.
- **`mix_mover_vector()`** (en `mix_motors`) — primitiva holonómica (cinemática R1 verificada con Elías).
- **`mix_config.h`** — bloque `MIX_EDGE_*` SIN las constantes de velocidad (no hay `VEL_MIN/LEAD_*`).

## TODO de banco — TASK-119
Sentido de `mix_mover_vector`, signo de `MIX_EDGE_FACE_KP` (giro al arco), `MIX_EDGE_PUSH_DIST_CM` en
cm reales, y rodeo completo. (Sin el paso del feedforward de velocidad: acá no aplica.)
Diseño: `journal/2026-06-23-centralmix-rodeo-estilo-edge.md`.
