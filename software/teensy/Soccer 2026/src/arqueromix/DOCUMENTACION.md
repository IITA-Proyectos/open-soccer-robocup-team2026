---
title: "arqueromix — qué quedó hecho (estado + diseño)"
date: 2026-06-21
author: "Claude (Opus 4.8) — coach, pedido de Virginia"
status: COMPILA · NO validado en banco (prueba)
scope: software/teensy/Soccer 2026/src/arqueromix/
---

# arqueromix — qué quedó hecho

> **En una frase.** Es el **arquero 2025** (campeón Nacional BsAs: su máquina de estados
> y su manejo directo de motores) **revivido sobre el robot 2026**: en vez de leer sus
> propios sensores, lee los datos que mandan las placas **TOP y DOWN** por serie. Es el
> HERMANO ARQUERO de `centralmix` (que hizo lo mismo con el delantero el viernes). **Es
> una prueba: si anda, se sigue por acá; si no, se sigue con `src/central/` y no se perdió
> nada** (build aislado).

## 1. Estado (qué está hecho y qué no)

| | Estado |
|---|---|
| Estructura de carpeta + archivos | ✅ creada (`src/arqueromix/`) |
| Env de compilación `central_robot2_arqueromix` | ✅ en `platformio.ini` (aditivo, no toca nada) |
| **Compila** | ✅ `pio run -e central_robot2_arqueromix` → SUCCESS, FLASH ~19 KB |
| Aislamiento (no afecta lo actual) | ✅ `build_src_filter = +<arqueromix/> +<shared/>` (NO compila `src/central/`) |
| FSM 2025 del arquero portada (10 estados) | ✅ código escrito (port fiel) |
| Manejo directo de motores | ✅ código escrito (pines R1/R2) |
| Lectura de TOP/DOWN (comm propio) | ✅ código escrito (decodifica `shared/proto`) |
| **Heading por serie del TOP (no BNO local)** | ✅ (pedido de Virginia) |
| **Validado en banco** | ❌ **NO** → TASK-114 (compila ≠ anda) |

**Compilar NO prueba que ande.** Faltan: verificar el sentido de cada motor, el signo
lateral de la pelota, y re-tunear umbrales (cambiaron de píxeles a mm). Ver §7.

## 2. Objetivo y decisión de fondo

El arquero 2025 tiene una FSM que el equipo entiende y un manejo de motores simple y
directo. La idea es **reusar ese cerebro** pero darle los **ojos del robot 2026** (cámara
+ línea + heading que ya procesan TOP y DOWN), exactamente como se hizo con el delantero
el viernes (`centralmix`). Es una **rama experimental paralela**, no reemplaza nada. Cero
riesgo para el stack actual porque vive en otra carpeta y otro env.

## 3. Arquitectura y flujo de datos

```
   TOP  (Serial7) ── WorldSnapshot ──┐   (pelota + arcos + HEADING + árbitro)
                                      │   ┌──────────┐     ┌──────────┐
   DOWN (Serial1) ── LineStatusV2 ───┼──►│ amix_comm│──►  │  g_aio   │  (variables planas
                  ── Pose2D/Vel2D ────┘   │ (decode) │     │ (AmixIO) │   estilo 2025)
                                          └──────────┘     └────┬─────┘
                                                                │ lee
                                                          ┌─────▼─────┐
                                                          │ amix_fsm  │  (FSM arquero 2025: 10 estados)
                                                          └─────┬─────┘
                                                                │ llama
                                                          ┌─────▼──────┐
                                                          │ amix_motors│  (directo: INA/INB+PWM)
                                                          └─────┬──────┘
                                                                ▼  pines Zircon (R1=R2 en 2026)
```

**Clave (igual que centralmix):** NO usa `world_model`. `amix_comm` deja los datos en
**variables planas** (`g_aio`, tipo `AmixIO`) — como las globales del 2025 — y el resto
(FSM + motores) es **autocontenido estilo 2025**, leyendo `g_aio`.

**Diferencia con centralmix (pedido de Virginia):** el **HEADING viene del snapshot del
TOP** por serie (`my_heading_centideg` + bit4 `heading_valid`), **NO de un BNO local**.
Las placas CENTRAL 2026 no traen BNO propio; el rumbo se procesa en el TOP. Más simple
(sin Wire/BNO) y correcto. Con `-DARQMIX_HEADING_OTOS` usa el heading del OTOS (DOWN).

## 4. Qué hace cada archivo

| Archivo | Qué hace |
|---|---|
| `main_arqueromix.cpp` | `setup()`: init comm/motores/FSM. `loop()`: `amix_comm_tick()` → `amix_fsm_tick()`. |
| `amix_io.h` | `struct AmixIO` + `extern AmixIO g_aio`: variables planas (pelota, arcos, heading, línea, árbitro, timers, frescura). |
| `amix_comm.cpp/.h` | **Único que toca Serial.** Lee TOP (Serial7) y DOWN (Serial1) a 230400, decodifica con `shared/proto` + `line_view`/`pose_view`, y **llena `g_aio`**. Heading = snapshot del TOP. |
| `amix_fsm.cpp/.h` | La **FSM del ARQUERO 2025** portada fiel (10 estados). Lee `g_aio`, decide, llama primitivas de `amix_motors`. Agrega el gate `match_running` + un timeout de seguridad al retroceso. |
| `amix_motors.cpp/.h` | **Manejo directo 2025**: `adproporcional/aiproporcional/impulso_inicial/avanzar/avanzar_patear/patear_atras` + `amix_set_motor(idx,pwm)`. Escribe `analogWrite(PWM)`+`digitalWrite(INA/INB)`. Sin mixer, sin cinemática omni. |
| `amix_config.h` | Pines (R1/R2 2026), constantes 2025 (PWM proporcionales, impulsos, patada), tolerancias, tiempos, selector de heading. |
| `README.md` | Guía corta + comando de flasheo. |
| `DOCUMENTACION.md` | Este archivo. |

## 5. La máquina de estados del arquero (10 estados, port fiel del 2025)

Flujo del arquero: patrullar lateral siguiendo la pelota en el eje lateral, y al tenerla
cerca+centrada, despejar (pausa → patada → pausa → retroceso a la línea → reposicionar).

```
impulso_inicial (40 ms, strafe fuerte)
        ▼
moverce_derecha ◄──────────────► moverce_izquierda
   │   │   │                         │   │   │
   │   │   └ línea(borde) → impulso_izquierda (350 ms) ─┐
   │   │      línea(borde) → impulso_derecha (350 ms) ──┘  (cada impulso vuelve a su moverce)
   │   │
   │   └ pelota desviada (|lateral|≥DESVIO): elige lado por signo de ball_x_mm
   │
   └ pelota cerca+centrada (profundidad≤CERCANIA && |lateral|≤CENTRADO)
              ▼
   PATEANDO_pausa_inicial (200 ms) → PATEANDO_adelante (450 ms, avanzar_patear)
              ▼
   PATEANDO_pausa (1000 ms) → PATEANDO_atras (retroceso recto hasta ver línea + safety 4 s)
              ▼
   avanzar_despues_de_patear (1000 ms) → moverce_derecha  (retoma patrulla)
```

- **Patrulla (`moverce_*`):** `ad/aiproporcional()` hace strafe lateral CON corrección de
  rumbo en 3 bandas según el `error` (= heading − heading_inicial). Sin pelota patrulla a
  `pd=1`; con pelota desviada `pd=1.5` (corrige más fuerte).
- **Decisión por la pelota:** cerca+centrada → patea; desviada → va al lado de la pelota;
  banda muerta (entre centrado y desvío, o centrada-pero-lejos) → para.
- **Rebote en el borde:** al ver línea, impulso temporizado de 350 ms al lado OPUESTO para
  no quedarse trabado oscilando (igual que el 2025).
- Los estados DELANTERO del 2025 (girar/apuntar/centrar/patear largo) **no se portan acá**:
  eso es `centralmix`.

## 6. El mapeo 2025 → 2026 (la traducción que hace el adaptador)

| Dato | El arquero 2025 lo leía de… | En arqueromix viene de… | Adaptación |
|---|---|---|---|
| ¿Ve pelota? | `Xp != 0` (cámara local) | `ball_visible` (snapshot TOP) | directo |
| Profundidad pelota | `Xp` (cámara, píxeles) | `ball_y_mm` (snapshot, mm) | `cerca = ball_y_mm ≤ CERCANIA`. ⚠️ unidades distintas → RE-TUNEAR |
| Lateral pelota | `Yp` (cámara, píxeles) | `ball_x_mm` (snapshot, mm) | `centrada = |ball_x_mm| ≤ CENTRADO`. ⚠️ RE-TUNEAR |
| ¿A qué lado? | signo de `Yp` (`Yp<0`→der) | signo de `ball_x_mm` | `ball_x_mm>0`→derecha. ⚠️ RE-VERIFICAR SIGNO |
| Rumbo (`error`) | **BNO local** del arquero | **heading del snapshot TOP** | `error = heading − heading_inicial`; sin BNO local |
| Línea (3 sensores) | `s1/s2/s3` analógicos locales | **DOWN** (`line_present/depth`) | 3 sensores → una señal de DOWN. ⚠️ pierde el "qué lado", RE-TUNEAR |
| Árbitro | (no tenía) | `match_running` (snapshot) | **nuevo** gate GO/STOP |
| Motores (salida) | `analogWrite/digitalWrite` inline | primitivas `amix_motors` | mismos valores, pines 2026 |

## 7. Lo que falta validar en banco (TASK-114) — ⚠️ compila ≠ anda

1. **Sentido de cada motor / primitiva.** El 2025 arquero era ROBOT1 con un layout de
   pines; arqueromix usa los pines 2026 (R1=R2). Una primitiva `adproporcional` (strafe
   derecha) podría salir a la izquierda o invertida. **Verificar cada primitiva con las
   ruedas al aire** antes de la FSM (`amix_set_motor` suelto por índice).
2. **Signo lateral de la pelota.** `ball_x_mm>0`→derecha es la elección intuitiva; si el
   arquero va para el lado contrario de la pelota, invertir en `ball_a_la_derecha()`.
3. **Re-tuneo píxeles→mm.** `AMIX_TOL_CERCANIA_MM` (140), `_CENTRADO_MM` (30), `_DESVIO_MM`
   (50) eran coordenadas de cámara; ahora son mm. **Re-tunear los tres en banco** mirando
   la telemetría de la pelota.
4. **Heading del TOP.** Confirmar que `g_aio.heading_deg`/`heading_valid` llegan sanos del
   snapshot (mirar el monitor). Si el heading no es confiable, la patrulla usa la banda
   centrada (no corrige rumbo) — degrada, no rompe.
5. **Línea desde DOWN.** El 2025 distinguía borde (s1|s2) vs vuelta-a-línea (s1|s2|s3); acá
   ambos son `line_present`. Confirmar que DOWN reporta la línea lateral del arco a tiempo.
6. **Comm.** Confirmar que `g_aio` se puebla con datos reales de TOP/DOWN (telemetría).

## 8. Decisiones de diseño (y por qué)

- **Heading por SNAPSHOT del TOP (no BNO local).** Pedido explícito de Virginia y correcto:
  las CENTRAL 2026 no traen BNO; el rumbo viene de arriba. (centralmix había dejado un BNO
  local como default — acá se corrige.)
- **`match_running` agregado.** El 2025 arrancaba solo; en RCJ no se mueve hasta el START.
- **Timeout de seguridad en el retroceso.** El 2025 `PATEANDO_atras` no tenía timeout
  (retrocedía hasta ver blanco; si no llegaba, se colgaba). Se agrega un tope de 4 s.
- **Manejo directo de motores (no el mixer 2026).** Que sea como el 2025 para debuggear fácil.
- **Sin `world_model`.** Variables planas (`g_aio`) como las globales del 2025.
- **Línea de DOWN, sin "qué lado".** El DOWN agrega los 32 sensores en una señal; el 2025
  distinguía s1/s2/s3. Hoy ambos branches (borde / vuelta) usan `line_present`. Se puede
  refinar por `line_angle_deg` a futuro (como hizo centralmix por sectores).

## 12. Límite honesto: arqueromix es MODO NO-REGRESIÓN, no reemplazo del arquero R2

El contrato plano `AmixIO` **recorta** la línea a 3 campos (`line_present`/`line_angle_deg`/
`line_depth`), pero el `LineStatusV2` del DOWN trae 10+ campos. En particular **NO expone**:
- `cross_track_mm` — el arquero R2 que YA anda en banco lo usa para hacer **strafe paralelo**
  a la línea por error lateral real; arqueromix no lo tiene (patrulla por signo de pelota, como 2025).
- `IMMINENT_EXIT` (event flag) — freno anticipado de borde; arqueromix solo ve `line_present`.
- `data_valid` — con `data_valid=0`, `line_angle_deg` devuelve 0 (que también es "línea al
  frente"). Para el arquero el riesgo es bajo (no usa el ángulo, solo present/depth), pero
  conviene saberlo. ⚠️ OJO: `line_depth` acá = **conteo de sensores** (0..32), NO `penetration_mm`
  como en `world_model_get_line_depth()` del CENTRAL clásico (mismo nombre, dos semánticas).

➡️ **arqueromix reproduce el arquero CAMPEÓN 2025 (simple, robusto) como modo de comparación /
no-regresión, NO reemplaza al arquero R2** (que además tiene Y-hold de profundidad, pose XY por
paredes y escape acotado). Si en banco se quiere paridad, el camino es **ampliar `AmixIO` +
`apply_down_line`** con `cross_track_mm`/`cross_track_valid` (helper `lsv2_cross_track_mm` ya
existe en `line_view.h`) e `imminent_exit` — trabajo concreto, no opcional, para esa paridad.
Decisión del equipo tras validar el port base (TASK-114).

## 9. Cómo compilar, flashear y volver atrás

```bash
# Compilar:
pio run -e central_robot2_arqueromix
# Flashear a la CENTRAL de R2 (la de Virginia):
pio run -e central_robot2_arqueromix -t upload
# Heading por OTOS en vez del TOP:
#   agregar -DARQMIX_HEADING_OTOS al build_flags del env
# VOLVER al arquero de competencia (descarta arqueromix):
pio run -e central_robot2_arquero -t upload
```
El env `central_robot2_arqueromix` **extiende** `central_robot2` (board Teensy 4.1) y solo
cambia `build_src_filter` para compilar `arqueromix/ + shared/`. **TOP y DOWN no se tocan:**
seguí con `top_robot2_pri` y `down_robot2`.

## 10. Cómo continuar (orden sugerido)

1. **Banco — primitivas de motor** una por una, ruedas al aire (TASK-114 paso 1).
2. **Banco — comm**: ver `g_aio` poblado en vivo (heading, pelota, línea).
3. **Banco — FSM completa** + re-tuneo de umbrales y signo lateral (§7).
4. Decisión: si anda → seguir mejorando acá; si no → volver a `src/central/` (nada perdido).

## 11. Referencias

- Código: `src/arqueromix/` (este directorio).
- Hermano delantero (el del viernes): `src/centralmix/` + `journal/2026-06-19-centralmix-port-delantero-2025.md`.
- Base 2025: `software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026`.
- Análisis fiel 2025: `docs/internal/ANALISIS-FIEL-ARQUERO-2025.md` (la fuente del port).
- Validación de banco: `team-tasks/2026-06-21-task-114-validar-arqueromix-banco.md`.
- Journal: `journal/2026-06-21-arqueromix-port-arquero-2025.md`.
- Arquero 2026 actual (para comparar): `src/central/strategy.cpp` (FSM GK).
