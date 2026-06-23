# 2026-06-23 — centralmix: delantero REACTIVO estilo "Edge" (回り込み / rodeo)

## Qué se pidió (Elías)
El delantero "mix" (port 2025 sobre el robot nuevo) es **lento para posicionarse** y "si la pelota se
estuviera moviendo no creo que llegaría a hacer gol". Estudiamos cómo se pone detrás de la pelota el
**Team Edge** (campeón mundial Soccer Lightweight 2024, `github.com/shiokara0525/Edge_2025_main`) y se
pidió **portar ese rodeo al delantero de centralmix**, en una **rama nueva** (`claude/lightweight-
positioning-strategy-vb6s0s`), usando **solo los valores que ya están en la última versión de mix**,
trabajando organizado y fácil de leer/ajustar. Además: **reconstruir la cinemática** (la del repo
`kinematics.cpp` no les anda bien) y **verificar la geometría del robot con Elías** antes de implementar.

## El diagnóstico (por qué el mix es lento) vs Edge
| | mix 2025 (lo de hoy) | Edge (`Attack.cpp` A==10) |
|---|---|---|
| Estructura | estados secuenciales APUNTAR→AVANZAR→CENTRAR | **1 estado reactivo**, recalcula cada tick |
| Velocidad al rodear | fija y BAJA (centrar = 24/72 PWM) | **full** (220–240) |
| Pelota en movimiento | no usa velocidad de pelota | feedforward `ball.vec_velocity` (`go_ang+=30`) |
| Esperas | espera ≤30 cm, después orbita 4–6 s | ninguna, fluye |

El corazón de Edge es **amplificar el ángulo de la pelota**: cuanto más al costado está, más al
costado apunto → en vez de chocarla la **barro por detrás**; rodeando, el ángulo se achica solo y
termino empujándola al arco. El "mirar al arco" es un lazo aparte (su `AC`), no se mezcla con el avance.

## La geometría (reconstruida desde banco + VERIFICADA con Elías)
El `kinematics.cpp` del repo "no anda bien". Lo reconstruí **desde las primitivas ya probadas en banco**
(`avanzar`, `girar`, los `retroceder` que tuneó Elías). Resultado honesto: **la fórmula del repo en
realidad está bien**; lo que rompe es otra cosa (signo de ω / piso de motor / calibración mm/s→PWM).

Matriz R1 (M1=del-IZQ, M2=del-DER, M3=trasera), en **PWM directo**:
```
w_M1 = +0.5·vx + 0.866·vy + ω
w_M2 = +0.5·vx − 0.866·vy + ω
w_M3 = −1.0·vx +    0     + ω      (vx=+derecha, vy=+frente, ω=mismo a las 3 = giro puro)
```
Verificada contra 5 primitivas de banco (`avanzar`→[+,−,0], `girar`→[−,−,−], `retroceder2`→[−,+,0],
`retroceder1/3` dominantes). **Elías confirmó (2026-06-23):** M1=del-izq, M2=del-der, M3=trasera; y la
convención de signo por rueda (vista de frente: horario=−PWM, antihorario=+PWM, igual que `avanzar`).
Diagrama + derivación: `docs/firmware/CINEMATICA-OMNI-R1-DERIVACION.md`.

## Qué se implementó (carpeta APARTE `src/centraledge/`, pedido de Elías)
> **Update 2026-06-23 (Elías):** el rodeo Edge se separó a su propia carpeta `src/centraledge/` (copia
> autocontenida de centralmix + Edge, mismo patrón que centralmix vs arqueromix) para revisarlo sin
> mezclarlo con el mix 2025. **`src/centralmix/` quedó PRISTINO** (su FSM 2025 + el estado `TEST` de
> debug de Elías intactos, verificado: diff vacío contra el commit previo). El env
> `central_robot1_mix_edge` compila SOLO `centraledge/` + `shared/`. Las rutas de abajo que dicen
> `src/centralmix/` hoy son `src/centraledge/`.

Archivos (en `src/centraledge/`):

1. **`mix_edge.{h,cpp}`** — núcleo **PURO** (sin Arduino, host-testeable): la **curva de rodeo**
   (|ángulo pelota|→ángulo avance, piecewise lineal CONTINUA en 3 tramos) + la decisión de **empuje**.
2. **`mix_mover_vector()`** (en `mix_motors`) — primitiva **holonómica** (traslación en cualquier
   ángulo + giro), con la matriz de arriba, en PWM directo y saturación por escalado (no hereda la
   calibración del `kinematics.cpp`).
3. **`mix_fsm_edge.{h,cpp}`** — FSM autocontenida: `KICKOFF → BUSCAR → **RODEAR** → EMPUJAR →
   RETROCEDER` + escape de línea. Reusa el árbitro RCJ, el kickoff-medialuna, `avanzar_patear`
   (empuje por inercia, **sin pateador**) y `retroceder1/2/3` (bench-tuneados de Elías).
4. **`mix_config.h`** — bloque `MIX_EDGE_*` (todas las perillas, con notas de banco).
5. **`main_centralmix.cpp`** + **`platformio.ini`** — selección por flag + env `central_robot1_mix_edge`.

### Feedforward de velocidad — SÍ implementado (update mismo día, pedido Elías)
Elías marcó que "el mix trae todos los datos del mundo del TOP, capaz sirve la velocidad de la pelota".
**Tenía razón:** el `WorldSnapshot` del TOP YA trae `ball_vx_mm_s`/`ball_vy_mm_s` (el TOP los calcula con
el módulo `ball_velocity` y los manda en `main_top.cpp:231`). `mix_comm` los recibía pero NO los pasaba a
`g_io`. Se cableó: `mix_io.ball_vx_cm_s/vy` + `mix_comm` (÷10 a cm/s) → `EdgeIn` → núcleo puro.
- En vez del `go_ang += 30` crudo de Edge, se hace **proyección predictiva**: apuntar a la pelota
  `posición + velocidad·LEAD_S` (dónde VA a estar), gateado por `MIX_EDGE_VEL_MIN_CM_S` (ignora ruido) y
  topeado por `MIX_EDGE_LEAD_MAX_CM` (acota el ruido de la velocidad de cámara).
- El **empuje** se sigue decidiendo con la pelota ACTUAL (no la predicha): no comprometerse a empujar
  contra una posición que todavía no existe.
- ⚠️ Caveat honesto: la velocidad es RELATIVA al robot (incluye ego-movimiento) y de cámara (ruidosa).
  Por eso el gate + tope + el kill-switch (`MIX_EDGE_VEL_MIN_CM_S = 9999` → rodeo por posición pura, A-B
  en banco). 3 tests host nuevos (anticipa / gate ignora lento / tope).

### Diferencia honesta vs Edge
- **Sin pateador:** el "gol" es empuje por inercia (`avanzar_patear`), no solenoide.
- El feedforward usa proyección predictiva (mejor que el `+30` fijo de Edge), pero la velocidad RELATIVA
  con ego-movimiento es un límite conocido → si molesta en banco, kill-switch y queda el rodeo por posición.

## Verificación host (sin hardware)
`test/test_mix_edge/` (Unity) — **11/11 verde** (`bash scripts/run-host-tests.sh test_mix_edge`):
curva (cero, simetría, monotonía, **continuidad** en los quiebres, valores clave, tope) + decisión de
empuje (lejos/alineado/arco-desalineado/sin-arco/costado). Suite completa: sin regresiones (no toqué
`src/shared`).

## Lo que NO puede cerrar Claude (regla de hardware)
Nada de esto está probado en placa. El sentido de `mix_mover_vector`, el **signo** de `MIX_EDGE_FACE_KP`
(giro al arco) y las distancias en cm reales se titulan en banco → **TASK-119**.

## Estado
- Rama `claude/lightweight-positioning-strategy-vb6s0s`. Compila host (núcleo puro); el firmware Teensy
  lo compila el equipo (`pio run -e central_robot1_mix_edge`).
- Pendiente humano: TASK-119 (banco). El mix 2025 sigue intacto como fallback (sin el flag).
