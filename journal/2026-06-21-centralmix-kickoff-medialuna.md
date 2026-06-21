# 2026-06-21 — centralmix: estado de ARRANQUE (kickoff) con impulso de medialuna

## Qué se pidió (Elías)
Un estado para el inicio de todo: al arrancar, **si ve la pelota va hacia ella**; si **no la
ve**, que se mueva hacia el centro de la cancha con una **medialuna** — específicamente, un
**impulso FUERTE y CORTO de medialuna** (elegido sobre "girar 30° con el BNO" y sobre la
órbita sostenida).

## Qué se hizo (KICKOFF_SEEK = PRIMER estado, sin flag)
Estado **`KICKOFF_SEEK`** como **primer estado** del FSM del centralmix (sin flag; reemplaza al
`AVANCE_INICIO` 2025, que se quitó). Va en TODAS las builds del centralmix. Lógica:
- **ve pelota** (`ball_visible`) → `APUNTAR_PELOTA` (la persigue, flujo de siempre).
- **línea** (`linea_s1/2/3`) → escape `DETECTA_LINEA_*` (prioridad sobre la medialuna; no se va de la cancha).
- **no ve pelota** → `kickoff_medialuna()` (impulso fuerte y corto) durante `MIX_KICKOFF_ARC_MS`,
  y después → `GIRANDO` (la búsqueda por giro de siempre; con su timeout de 9 s ya existente,
  así que no se traba).
- Entrada: `mix_fsm_init` arranca SIEMPRE en `KICKOFF_SEEK`. Como ningún otro estado transiciona
  a él, la maniobra de arranque se ejecuta una sola vez. (El viejo `AVANCE_INICIO` se eliminó.)

## La medialuna, sin inventar cinemática dudosa
El journal 2026-06-19 ya marcó que `centrar_*` decodifica a **casi-puro-strafe** (orbita mal).
Para NO caer en eso, la medialuna se arma por **superposición de las 2 bases que YA existen**
(en un omni las velocidades de rueda suman linealmente):
```
AVANCE (base avanzar(): M1=+F, M2=-F, M3=0)  +  GIRO (base girar(): M1=M2=M3=+T)
⇒  M1 = +F + dir·T ,  M2 = -F + dir·T ,  M3 = dir·T
```
`kickoff_medialuna(dir)` en `mix_motors.cpp`. Tuneables en `mix_config.h`:
`MIX_KICKOFF_ARC_FWD` (F=avance, 140), `MIX_KICKOFF_ARC_TURN` (T=curva, 70),
`MIX_KICKOFF_ARC_DIR` (lado ±1), `MIX_KICKOFF_ARC_MS` (duración corta, 250 ms).
"Fuerte" = F alto; "corto" = ARC_MS chico → térmicamente seguro aunque el PWM sea alto.

## Límite honesto (coach)
El centralmix **no tiene pose absoluta** → "hacia el centro" es dead-reckoning: el lado de la
curva es FIJO (`MIX_KICKOFF_ARC_DIR`), hay que setearlo para tu lado de saque en banco. Si curva
para el lado equivocado, se invierte ese flag (no se tocan los signos de la primitiva). Mejora
futura fácil: sesgar `dir` según el arco rival que vea la cámara, o alejarse de la línea que ve DOWN.

Semántica de "inicio": hoy el kickoff ocurre **una vez** tras el boot (cuando llega el GO del
árbitro estando en `KICKOFF_SEEK`); NO se re-dispara en cada STOP→GO (kickoff tras gol). Eso es
una mejora aparte (resetear a `KICKOFF_SEEK` en el flanco STOP→GO) — anotada, no hecha.

## Impacto
- **Cambia el arranque de TODAS las builds del centralmix** (`central_robot1_mix`,
  `central_robot1_mix_bno`): ahora empiezan en `KICKOFF_SEEK` (medialuna) en vez del
  `AVANCE_INICIO` 2025 (avance 700 ms), que se eliminó. NO afecta a `src/central/` ni a ningún
  env de competencia (el centralmix es su propio build aislado).
- Tocado: `mix_config.h` (4 constantes), `mix_motors.{h,cpp}` (`kickoff_medialuna`),
  `mix_fsm.{h,cpp}` (KICKOFF_SEEK como 1er estado + quitar AVANCE_INICIO), `platformio.ini`
  (comentario; NO hay env nuevo — el kickoff va en `central_robot1_mix_bno`).

## ⚠️ Sin verificar
- **No compilé** (shell de la máquina rota: `TEMP`/KMSpico). El equipo corre
  `pio run -e central_robot1_mix_bno` antes de flashear.
- **No validado en banco** (regla #1) → **TASK-116** (sentido de la medialuna + F/T/duración +
  que "el centro" tenga sentido para el lado de saque).

Pre-requisito: TOP de R1 con `top_robot1_pri_rt` (BNO andando) — igual que el env `_mix_bno`.
Ver [[project-iita-soccer-2026-strategy]]. Hermano del journal
`2026-06-21-centralmix-bno-del-top-snapshot.md`.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
