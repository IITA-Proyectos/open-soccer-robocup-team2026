---
title: "centralmix — el delantero apunta a goal_opp (arco rival por ROL, sin color) + cierra bug color↔rol"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: firmware-refactor-fix
toca-competencia: NO (solo src/centralmix/ + build aislado central_robot1_mix)
status: COMPILA · NO validado en banco
---

# Sesión 2026-06-21 — el delantero deja de mirar color: apunta a goal_opp (rival, resuelto por el TOP)

## Qué se pidió (Gustavo)

Misma lógica que ya se aplicó al arquero (`arqueromix`), ahora en el delantero (`centralmix`, Elías):
1. La **determinación de arco propio vs rival vive en la placa TOP** (`goal_polarity`).
2. La placa CENTRAL **nunca pregunta por color** (amarillo/azul).
3. El delantero **patea hacia el arco RIVAL (`goal_opp`)**, alineado, sin importar el color.

## Bug que se encontró y se cierra

El delantero ya se alinea al arco rival orbitando la pelota (estados `CENTRANDO_*`, gate
`|arco_rival_angle| ≤ MIX_TOL_CENTRADO`), así que la alineación ya estaba. **Pero** la FUENTE del arco
rival estaba rota: `mix_fsm.cpp` tenía `kArcoRivalEsAmarillo = false`, y con `false` los helpers leían
`goal_blue_*`; el mapeo default de `mix_comm` ponía `goal_opp → goal_yellow` y `goal_own → goal_blue`.
Encadenado: **`arco_rival` leía `goal_blue` = `goal_own` = el arco PROPIO.** O sea, el delantero se
"centraba" contra el arco equivocado. (Es la inconsistencia color↔rol que la revisión del arquero ya
había anticipado.)

## Cambios (solo `src/centralmix/`)

- **`mix_io.h`** — se eliminan los campos `goal_yellow_*/goal_blue_*` → `goal_opp_*` / `goal_own_*`
  (por ROL, sin color).
- **`mix_comm.cpp`** — se elimina el `#ifdef MIX_ATTACK_BLUE` y el mapeo a yellow/blue; se copia DIRECTO
  `goal_opp_*` / `goal_own_*` del snapshot (con sentinela `visible=0`).
- **`mix_fsm.cpp`** — se elimina `kArcoRivalEsAmarillo`; `arco_rival_visible()/arco_rival_angle_deg()`
  devuelven `g_io.goal_opp_*` directo. **El mecanismo de orbitar+alinear+patear NO se tocó** (solo cambió
  la fuente detrás de los helpers). Comentarios actualizados.
- **`mix_comm.h` / `DOCUMENTACION.md` / `README.md`** — actualizados (arcos por ROL, sin
  `-DMIX_ATTACK_BLUE`).

No hizo falta primitiva nueva (a diferencia del arquero, que kickeaba recto y necesitó `girar()` + estado
`ALINEAR_arco_opp`): el delantero ya orbita y se alinea al arco rival antes de patear.

## Verificación hecha

- **Compila:** `pio run -e central_robot1_mix` y `central_robot1_mix_bno` → SUCCESS.
- **Revisión adversarial** (subagente): sin bloqueantes ni importantes. Confirmó: color erradicado del
  código, bug color↔rol cerrado de raíz (`arco_rival = goal_opp = rival`), sentinela y unidades OK, sin
  referencias colgadas, mecanismo de pateo/fallback intacto. Hallazgos solo MENORES (superficie de
  `goal_own`/`goal_opp_dist` disponible pero no usada — igual que el 2025).

## ⚠️ NO validado en hardware — a confirmar en banco (Elías)

- El ataque ahora depende 100% de que el **TOP entregue `goal_opp` con la polaridad correcta**, lo que
  requiere que el robot **arranque mirando a la cancha** (premisa de `goal_polarity`; fail-safe del TOP =
  amarillo rival si el latch no confirma). Confirmar en banco que el delantero se centra contra el arco
  RIVAL (no el propio) y patea hacia ahí.
- Sigue pendiente el resto del TODO de banco de centralmix (sentido de motores, re-tuneo píxeles→mm,
  línea por sector). Ver TASK-113/115/116.

**Regla del repo:** compila ≠ anda. La TASK de hardware la cierra el equipo, no Claude.
