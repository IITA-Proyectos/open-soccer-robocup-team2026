# centraledge — delantero REACTIVO estilo "Edge" (rodeo) — CARPETA APARTE de centralmix

> **Qué es.** El delantero del robot pero con la estrategia de **posicionamiento del Team Edge**
> (campeón mundial Soccer Lightweight 2024): en vez de "apuntar → avanzar → orbitar" en estados
> separados (lento), se pone **detrás de la pelota con UNA fórmula reactiva** a full velocidad, mira
> al arco mientras rodea, **anticipa la pelota en movimiento** con su velocidad, y empuja al arco por
> inercia (**sin pateador**).
>
> ⚠️ **Compila, pero NO está validado en banco.** Compilar ≠ andar. Banco → **TASK-119**.

## Por qué es una carpeta separada (pedido de Elías)
Para revisar el rodeo Edge **sin mezclarlo** con el mix 2025. `src/centralmix/` queda **PRISTINO**
(su FSM 2025 + el estado `TEST` de debug intactos). `centraledge/` es una **copia autocontenida** de
centralmix + el rodeo Edge — el mismo patrón que el repo ya usa con `centralmix` vs `arqueromix`.
Cada uno compila en su propio env (`build_src_filter`), nunca juntos → no se pisan.

## Cómo compilar
```
pio run -e central_robot1_mix_edge -t upload
```
Ese env compila **solo** `centraledge/` + `shared/` con `-DMIX_ATTACK_EDGE`. Para volver al mix 2025:
`central_robot1_mix_bno` (mismo robot, otra carpeta) o cualquier env de competencia.

## Qué cambia vs centralmix (lo que ESTE folder agrega)
Todo lo demás (`mix_io` / `mix_comm` / `mix_motors` 2025 / `mix_fsm` 2025 con su estado `TEST` / el
árbitro / el kickoff / la patada por inercia) es **igual a centralmix**. Lo nuevo:

- **`mix_edge.{h,cpp}`** — núcleo PURO (host-testeable, `test/test_mix_edge/`): la **curva de rodeo**
  (ángulo de pelota → ángulo de avance amplificado, "回り込み") + la decisión de empuje + el
  **feedforward de velocidad** (anticipa a dónde VA la pelota).
- **`mix_fsm_edge.{h,cpp}`** — la FSM del rodeo: `KICKOFF → BUSCAR → RODEAR → EMPUJAR → RETROCEDER`
  + escape de línea (`DETECTA_LINEA_1/2/3`, igual al 2025). El estado **RODEAR** es el corazón.
- **`mix_mover_vector()`** (en `mix_motors`) — primitiva HOLONÓMICA (moverse en cualquier ángulo +
  girar), con la cinemática R1 verificada con Elías. Ver `docs/firmware/CINEMATICA-OMNI-R1-DERIVACION.md`.
- **`mix_io` / `mix_comm`** — agregan `ball_vx/vy_cm_s` (la velocidad de pelota del WorldSnapshot del TOP).
- **`mix_config.h`** — bloque `MIX_EDGE_*`: TODAS las perillas del rodeo, con notas de banco.

## Flujo de datos (igual que centralmix, SIN world_model)
```
TOP (Serial7) ─┐                       ┌─ mix_fsm_edge (RODEAR reactivo + feedforward)  ◄── -DMIX_ATTACK_EDGE
DOWN (Serial1) ┴─ mix_comm ─► g_io ───►┤  (ó mix_fsm 2025 si se compila sin el flag)
                  (decode proto)        └─ mix_motors (DIRECTO 2025 + mix_mover_vector → Zircon R1)
```

## El estado RODEAR en una frase
Cada tick: mira el ángulo (y la velocidad) de la pelota → calcula UN ángulo de avance amplificado
(cuanto más al costado la pelota, más al costado apunto → la barro por detrás) → se mueve ahí a full
velocidad mientras gira para mirar al arco. Cuando queda detrás + cerca + alineado → empuja al arco.

## TODO de banco — TASK-119
Sentido de `mix_mover_vector`, signo de `MIX_EDGE_FACE_KP` (giro al arco), `MIX_EDGE_PUSH_DIST_CM` en
cm reales, y el A-B del feedforward de velocidad. Orden y criterios: `team-tasks/2026-06-23-task-119-*`.
Diseño completo: `journal/2026-06-23-centralmix-rodeo-estilo-edge.md`.
