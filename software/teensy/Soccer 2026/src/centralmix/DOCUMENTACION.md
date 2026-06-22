---
title: "centralmix — qué quedó hecho (estado + diseño)"
date: 2026-06-19
author: "Claude (Opus 4.8) — coach, pedido de Gustavo"
status: COMPILA · NO validado en banco (prueba)
scope: software/teensy/Soccer 2026/src/centralmix/
---

# centralmix — qué quedó hecho

> **En una frase.** Es el **delantero 2025** (su máquina de estados y su manejo
> directo de motores) **revivido sobre el robot 2026**: en vez de leer sus propios
> sensores, lee los datos que mandan las placas **TOP y DOWN**. Sirve para probar /
> debuggear con la lógica que el equipo ya conoce. **Es una prueba: si anda, se sigue
> por acá; si no, se sigue con `src/central/` y no se perdió nada** (build aislado).

## 1. Estado (qué está hecho y qué no)

| | Estado |
|---|---|
| Estructura de carpeta + 10 archivos | ✅ creada (`src/centralmix/`) |
| Env de compilación `central_robot1_mix` | ✅ en `platformio.ini` (aditivo, no toca nada) |
| Env `central_robot1_mix_bno` (R1 con gyro, BNO del TOP) | ✅ en `platformio.ini` (aditivo) — ⏳ **compilar (no lo hizo Claude: shell rota)** + banco TASK-115 |
| **Compila** | ✅ `pio run -e central_robot1_mix` → SUCCESS, FLASH ~23,5 KB (variante base, 2026-06-19) |
| Aislamiento (no afecta lo actual) | ✅ `build_src_filter = +<centralmix/> +<shared/>` (NO compila `src/central/`) |
| FSM 2025 portada (24 estados) | ✅ código escrito (port fiel) |
| Manejo directo de motores | ✅ código escrito (pines R1) |
| Lectura de TOP/DOWN (comm propio) | ✅ código escrito (decodifica `shared/proto`) |
| **Validado en banco** | ❌ **NO** → TASK-113 (compila ≠ anda) |

**Compilar NO prueba que ande.** Faltan: verificar el sentido de cada motor,
re-tunear umbrales (cambiaron de píxeles a mm), y confirmar el heading. Ver §7.

## 2. Objetivo y decisión de fondo

El delantero 2025 (campeón Nacional BsAs) tiene una FSM que el equipo entiende y un
manejo de motores simple y directo. La idea es **reusar ese cerebro** pero darle los
**ojos del robot 2026** (cámara + línea + odometría que ya procesan TOP y DOWN), para:

1. Tener un delantero alternativo **fácil de debuggear** (lógica conocida).
2. Probar rápido si esa lógica + los datos nuevos alcanzan, sin reescribir la
   estrategia 2026 (`strategy.cpp`).

Es una **rama experimental paralela**, no reemplaza nada. Cero riesgo para el stack
actual porque vive en otra carpeta y otro env.

## 3. Arquitectura y flujo de datos

```
   TOP  (Serial7) ── WorldSnapshot ──┐
                                      │   ┌──────────┐     ┌──────────┐
   DOWN (Serial1) ── LineStatusV2 ───┼──►│ mix_comm │──►  │  g_io    │  (variables planas
                  ── Pose2D/Vel2D ────┘   │ (decode) │     │ (MixIO)  │   estilo 2025)
                                          └──────────┘     └────┬─────┘
                                                                │ lee
                                                          ┌─────▼─────┐
                                                          │  mix_fsm  │  (FSM 2025: 24 estados)
                                                          └─────┬─────┘
                                                                │ llama
                                                          ┌─────▼──────┐
                                                          │ mix_motors │  (directo: INA/INB+PWM)
                                                          └─────┬──────┘
                                                                ▼  pines Zircon R1
```

**Clave (lo que pediste):** NO usa `world_model`. `mix_comm` deja los datos en
**variables planas** (`g_io`, tipo `MixIO`) — como las globales sueltas del 2025 —
y el resto (FSM + motores) es **autocontenido estilo 2025**, leyendo `g_io`.

## 4. Qué hace cada archivo

| Archivo | Qué hace |
|---|---|
| `main_centralmix.cpp` | `setup()`: init comm/motores/FSM. `loop()`: `mix_comm_tick()` → `mix_fsm_tick()`. + **print de debug** USB Serial 115200 throttleado (`if(Serial)`, apagable con `-DMIX_NO_DEBUG_SERIAL`): pelota / heading (`hdg`/`hvalid`/`herr`) / OTOS (`otos_hdg`/`otos_conf`) / arco / línea / árbitro / enlaces. |
| `mix_io.h` | Define `struct MixIO` + `extern MixIO g_io`: las variables planas (pelota, arcos, heading, línea, árbitro, timers, frescura de enlaces). Es "lo disponible". |
| `mix_comm.cpp/.h` | **Único que toca Serial.** Lee TOP (Serial7) y DOWN (Serial1) a 230400, decodifica con `shared/proto` + `line_view`/`pose_view`, y **llena `g_io`**. Calcula `angulo_pelota_deg` y `heading_error_deg`. |
| `mix_fsm.cpp/.h` | La **FSM 2025** portada fiel (24 estados). Lee `g_io`, decide, llama primitivas de `mix_motors`. Agrega el gate `match_running`. |
| `mix_motors.cpp/.h` | **Manejo directo 2025**: `girar/avanzar/centrar/patear/...` y `mix_set_motor(idx,pwm)`. Escribe `analogWrite(PWM)`+`digitalWrite(INA/INB)` sobre los pines R1. Sin mixer, sin pisos, sin cinemática omni. |
| `mix_config.h` | Pines R1, constantes 2025 (`MIX_G/A/C/IC`, tolerancias, kicker), umbrales de línea, selector de heading. |
| `README.md` | Guía corta + comando de flasheo. |
| `DOCUMENTACION.md` | Este archivo. |

## 5. La máquina de estados (arranque KICKOFF_SEEK 2026 + estados del 2025)

Flujo del delantero (arranque → buscar → apuntar → acercar → orbitar → patear):

```
                 (!match_running ⇒ parar)        ◄── regla nueva (árbitro RCJ), va ANTES de todo
   KICKOFF_SEEK ──(no ve pelota: medialuna fuerte y corta)──→ GIRANDO ──9s & |error|≤50→ AVANZANDO_POR_TIEMPO
        │ (ve pelota; PRIMER estado, se ejecuta 1 sola vez)       │ (ve pelota)
        └──────────────► APUNTAR_PELOTA ◄────────────────────────┘
                              │ |áng|<15
                              ▼
                          AVANZANDO ──pelota & cerca→ CENTRANDO_horario/antihorario  (orbita la pelota)
                              ▲ |áng|≥15                         │
                              └─────────────────────────────────┤ arco alineado / timeout
                                                                 ▼
                                                         PATEANDO_pausa_inicial → _adelante → _pausa → _atras
                                                                 │ (patada larga, empuje por inercia)
                                                                 ▼  vuelve a IMPULSO_INICIAL_GIRANDO
   (si pisa línea durante la órbita: alineado ⇒ PATEANDO_corto_*; desalineado ⇒ invierte sentido)
   (DETECTA_LINEA_1/2/3 ⇒ retroceder → IMPULSO_INICIAL_GIRANDO)
```

- **Patada larga** (`PATEANDO_*`): pausa 1000ms → avanzar 500ms → pausa 500ms →
  retroceder 200ms. Empuja "a ciegas" (cuando la pelota está pegada, la cámara no la ve).
  El empuje (`avanzar_patear`) ya NO es la rampa lenta lazo-abierto del 2025: ahora es **fuerte/rápido
  + RECTO con heading-hold del OTOS** (ancla `otos_heading_deg` al iniciar y corrige el giro; ver
  `mix_config.h` `MIX_KICK_*` y journal `2026-06-21-centralmix-patada-recta-otos.md`). TASK-117.
- **Patada corta** (`PATEANDO_corto_*`): cuando pisa línea bien alineado al arco.
- `PRIMER_IMPULSO_INICIAL_GIRANDO`: está en el enum pero **sin `case`** (estado muerto
  ya en el 2025; se conservó por fidelidad).
- **24 estados, no 28**: el enum real del 2025 tiene 24 nombrados. No se inventaron 4
  para llegar a 28 (se marcó en `mix_fsm.h`). Si el equipo quiere separar estados
  extra, es una decisión explícita.

## 6. El mapeo 2025 → 2026 (la traducción que hace el adaptador)

| Dato | El 2025 lo leía de… | En centralmix viene de… | Adaptación |
|---|---|---|---|
| Pelota posición | cámara UART, **píxeles** (`Xp/Yp`) | snapshot TOP, **mm** (`ball_x/y_mm`) | unidades distintas; `angulo_pelota_deg = atan2(x,y)·180/π` |
| ¿Ve pelota? | `Xp != 0` | `ball_visible` (snapshot) | directo |
| Arcos | cámara (`Xam/Yam`…) | snapshot (`goal_opp_*`/`goal_own_*`) | **por ROL, sin color**: el delantero apunta a `goal_opp` (rival, ya resuelto por el TOP) |
| Rumbo (`error`) | **BNO local** del delantero | **BNO del TOP** (snapshot) o OTOS | `heading_error_deg`; ⚠️ ver §8 |
| Línea | **3 sensores analógicos** locales | **DOWN** (`line_present/angle/depth`) | 3 sensores → 1 ángulo por **sector ±30°** |
| Árbitro | (no tenía) | `match_running` (snapshot) | **nuevo** gate GO/STOP |
| Motores (salida) | `analogWrite/digitalWrite` inline | primitivas `mix_motors` | mismos valores, **pines R1** |

## 7. Lo que falta validar en banco (TASK-113) — ⚠️ compila ≠ anda

1. **Sentido de cada motor.** El 2025 (ROBOT2) usaba OTRO mapeo de pines
   (`M1=8/7/6, M2=11/12/4, M3=2/5/3`); centralmix usa los R1 actuales
   (`M1=2/5/3, M2=8/7/6, M3=11/12/4`). Por eso una primitiva `avanzar` podría salir
   lateral o invertida. **Verificar cada primitiva con las ruedas al aire** antes de la FSM.
2. **Re-tuneo píxeles→mm.** `MIX_TOL_CERCANIA` (50) era coordenada lateral en píxeles;
   ahora es mm de distancia. `MIX_TOL_CENTRADO` (30) era diferencia de coordenada Y;
   ahora es grados del ángulo al arco. **Re-tunear ambos en banco.**
3. **Línea 3-sensores→1-ángulo.** Los `DETECTA_LINEA_1/2/3` se eligen por sector angular
   (±30°) del dato único de DOWN. Re-tunear el semiancho.
4. **Arco rival = `goal_opp`** (POR ROL, resuelto por el TOP con `goal_polarity`). El delantero
   ya NO mira color ni necesita `-DMIX_ATTACK_BLUE`: apunta al arco que el TOP marca como rival.
   En banco, confirmar que el TOP entrega `goal_opp` correcto (arranca mirando a la cancha).
5. **Comm.** Confirmar que `g_io` se puebla con datos reales de TOP/DOWN (telemetría).

## 8. Decisiones de diseño (y por qué)

- **Heading: 3 modos** (✅ resuelto 2026-06-21). El default es **BNO LOCAL del CENTRAL**
  (Wire@0x28) — pero **R1 NO tiene BNO local**, así que NO sirve para R1. El rumbo "oficial"
  2026 viene del **BNO del TOP por el snapshot**: modo nuevo `-DMIX_HEADING_SNAPSHOT` (env
  **`central_robot1_mix_bno`**), que NO toca ningún BNO local. Tercer modo: `-DMIX_HEADING_OTOS`
  (OTOS, sin gyro). Para R1 con gyro → `central_robot1_mix_bno`. El BNO del TOP de R1 quedó
  andando + validado en banco 2026-06-21 (fix del flag `bno_left_en`, ver journal
  `2026-06-21-bno-heading-fix-config-flag-no-era-tof.md`). Falta validar el delantero completo
  con ese heading en banco → TASK-115.
- **`match_running` agregado.** El 2025 arrancaba solo; en RCJ el robot **no se mueve
  hasta el START del árbitro**. Es la única regla nueva sobre la lógica 2025.
- **Manejo directo de motores (no el mixer 2026).** Pedido explícito: que sea como el
  2025 para que debuggear sea fácil. Por eso `mix_motors` NO usa `motors_zircon` ni
  cinemática omni; escribe los 3 motores a mano, igual que el 2025.
- **Sin `world_model`.** Variables planas (`g_io`) como las globales del 2025.

## 9. Cómo compilar, flashear y volver atrás

```bash
# Compilar (R1 CON gyro — BNO del TOP por snapshot; recomendado para R1):
pio run -e central_robot1_mix_bno
# Flashear a la CENTRAL de R1 (con gyro):
pio run -e central_robot1_mix_bno -t upload
# Variante BNO LOCAL en la CENTRAL (NO aplica a R1, no tiene BNO local):
pio run -e central_robot1_mix -t upload
# Variante SIN gyro (heading por OTOS):
#   agregar -DMIX_HEADING_OTOS al build_flags del env (o PLATFORMIO_BUILD_FLAGS)
# VOLVER al delantero/arquero actual (descarta centralmix):
pio run -e central_robot1_delantero_practica -t upload    # delantero actual
pio run -e central_robot1 -t upload                        # arquero actual
```
El env `central_robot1_mix` **extiende** `central_robot1` (board Teensy 4.1, `-DROBOT1`,
flags) y solo cambia `build_src_filter` para compilar `centralmix/ + shared/`.

## 10. Cómo continuar (orden sugerido)

1. Resolver el **heading** (§8) — 1 línea en `mix_comm`.
2. **Banco — primitivas de motor** una por una (TASK-113 paso 2).
3. **Banco — comm**: ver `g_io` poblado en vivo.
4. **Banco — FSM completa** + re-tuneo de umbrales (§7).
5. Decisión: si anda → promover; si no → descartar y seguir con `src/central/`.

## 11. Referencias

- Código: `src/centralmix/` (este directorio).
- Base 2025: `software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp`.
- Análisis 2025: `docs/internal/ANALISIS-FIEL-DELANTERO-2025.md`.
- Validación de banco: `team-tasks/2026-06-19-task-113-validar-centralmix-banco.md`.
- Journal: `journal/2026-06-19-centralmix-port-delantero-2025.md`.
- FSM 2026 actual (para comparar): `docs/firmware/STRATEGY-CPP-COMO-FUNCIONA.md`.
