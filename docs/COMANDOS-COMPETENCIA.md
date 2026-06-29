---
title: "Hoja de comandos — día de competencia (offline)"
date: 2026-06-25
status: vivo
tipo: referencia-rapida
audiencia: "equipo en la cancha (Virginia, Elías, Enzo, Gustavo)"
verificado-contra: "docs/pruebas-banco/QUE-FLASHEO-HOY.md (CANÓNICO) + platformio.ini + ESTADO-ACTUAL (2026-06-25)"
complementa: [docs/pruebas-banco/QUE-FLASHEO-HOY.md, docs/RUNBOOK-BANCO-INCHEON.md]
---

# Hoja de comandos — día de competencia

> **Autoridad de los ENVs = [`docs/pruebas-banco/QUE-FLASHEO-HOY.md`](pruebas-banco/QUE-FLASHEO-HOY.md)** (la
> tabla canónica robot×placa + lista negra). Este doc **la replica al 2026-06-25** y le SUMA lo que ella no
> tiene (app, IMU ZERO, gotchas, utilidades). Si discrepan, manda QUE-FLASHEO-HOY.
>
> **Carpeta de TODOS los `pio`:** `C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026` (abrí la
> terminal ahí). App del monitor: `…\Soccer 2026\tools\monitor-base`. Comando base: `pio run -e <ENV> -t upload`.
>
> ⚠️ **El hardware es role-agnostic:** cualquiera de los 2 robots se flashea como arquero O delantero. El
> "robot1/robot2" del nombre del env es **la PLACA/cableado**, no el rol. **Ignorá el rol que sugiera el nombre.**
> ⚠️ **Un USB = una placa.** Varias Teensy → `--upload-port COMx` (§4). Power-cycle tras flashear ToF/cámaras/OTOS (§6).

---

## 1. FLASHEO DE COMPETENCIA (envs del canónico QUE-FLASHEO-HOY)

**⚠️ ANTES de cualquier `pio`, parate en la carpeta del FIRMWARE** (NO en la del monitor). Copiá y pegá:
```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
```
> 📌 Dos carpetas: **flashear (`pio`)** → `…\Soccer 2026` · **app (`python -m monitor_base`)** → `…\Soccer 2026\tools\monitor-base`.
> Si te tira *"Not a PlatformIO project / platformio.ini not found"*, estás en la carpeta equivocada → corré el `cd` de arriba.

### TOP (las DOS placas TOP usan el MISMO env)
```
pio run -e top_robot2_pri -t upload
```
> ⚠️ **`top_robot1`, `top_robot1_pri*`, `top_robot1_oscint`, `top_robot1_bno_wire2` están en LISTA NEGRA**
> (cableado VIEJO pre-recableado 06-11). Para CUALQUIER TOP va `top_robot2_pri`.

### 🥅 El robot que juega de ARQUERO  (programa DEFINITIVO)
| Placa | Comando |
|---|---|
| TOP | `pio run -e top_robot2_pri -t upload` |
| **CENTRAL** | `pio run -e central_robot2_arqueromix_quieto -t upload` |
| DOWN | `pio run -e down_robot2 -t upload` |

> **Qué hace el arquero al iniciar:** va al arco — **se mueve de costado hasta la línea** → **avanza** un poco
> → se queda **QUIETO buscando la pelota**. Si la pelota se va a un costado la **sigue de costado** (consciente
> de la línea, no se sale de la cancha); si está cerca **despeja**; después de patear se **orienta de frente al
> arco contrario** (por giroscopio) y **retrocede hasta la línea**.
>
> Definitivo elegido por el equipo (2026-06-29) = `central_robot2_arqueromix_quieto` (candidato 06-22).
> **Fallbacks del arquero:** `central_robot2_arquero` (viejo canónico, .hex `CENTRAL_R2_arquero_competencia.hex`)
> o `central_robot2_arqueromix` (patrulla). DOWN: si `down_robot2` falla, probar `down_robot2_rt`.

### ⚽ El robot que juega de DELANTERO
| Placa | Comando | Nota |
|---|---|---|
| TOP | `pio run -e top_robot2_pri -t upload` | (igual) |
| **CENTRAL** | `pio run -e central_robot2 -t upload` | delantero de partido (si va en la placa R2) |
| DOWN | `pio run -e down -t upload` | **con OTOS** (la placa con OTOS = la del delantero) |

> ⚠️ Si el delantero va en la PLACA R1: su CENTRAL de partido está **en decisión pendiente** (QUE-FLASHEO-HOY:
> "hoy usar `central_robot1_arquero_demo`"; `central_robot1_delantero_practica_bb` es **fallback, NO competencia**).
> **Confirmá con Gustavo qué placa juega qué rol y con qué CENTRAL.**

### Cámaras (ambas, ambos robots)
Cargar `hardware/electronics/camaras-openmv/main.py` desde OpenMV IDE. (NO los `cam-*-n6.py` de los packs, deprecados.)

---

## 2. APP DE MONITOREO  (comprobar sensores)

**Paso 1 — entrá a la carpeta** (copiá y pegá tal cual):
```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026\tools\monitor-base"
```
**Paso 2 — abrí la app** (y andá al panel **"Salud"** para ver los sensores):
```
python -m monitor_base --monitor                 # unificado (auto-detecta la placa)
python -m monitor_base --monitor --port COM15     # si no auto-detecta, fijá el puerto
python -m monitor_base --field                   # cancha / pose XY
python -m monitor_base --top-salud                # salud + botón IMU ZERO
python -m monitor_base --list-ports               # ver qué COM es la Teensy
```
> El COM puede cambiar al cambiar de cable/puerto (fue COM15 / COM17). Si dice **"esperando datos"**, casi
> siempre es **puerto equivocado** → corré `--list-ports` y usá `--port` con el COM que diga "probable Teensy".

---

## 3. FALLBACKS 🛟 (la EEPROM —calib línea + heading— persiste al reflashear)

| Caso | Comando |
|---|---|
| Arquero CENTRAL falla → patrulla arqueromix | `pio run -e central_robot2_arqueromix -t upload` |
| TOP se porta raro → TOP anterior (sin timer de snapshot) | `pio run -e top_robot2_pri_anterior -t upload` |
| DOWN R2 RT se porta mal → base sin RT | `pio run -e down_robot2 -t upload` |
| Reflasheo limpio (binario quedó raro) | `pio run -t clean -e <ENV>` y luego el `-t upload` |

---

## 4. UTILIDADES

| Qué | Comando |
|---|---|
| Listar puertos serie | `pio device list` |
| Elegir puerto al subir | `pio run -e <ENV> -t upload --upload-port COM5` |
| Compilar SIN subir | `pio run -e <ENV>` |
| Limpiar y recompilar | `pio run -t clean -e <ENV>` |

---

## 5. IMU ZERO — anclar el heading al arco rival
1. Apuntá el robot **al arco rival**, quieto.
2. `python -m monitor_base --top-salud --port COM15`
3. Botón **IMU ZERO** (heading → ~0) → botón **IMU SAVE** (persiste; sin esto se pierde al rebootear).
> El BNO (modo IMU, sin magnetómetro) **deriva de a poco** → si se corre, re-zerá.

---

## 6. GOTCHAS DEL DÍA ⚠️
1. **Power-cycle SIEMPRE tras flashear ToF/cámaras/OTOS** (cortar batería **Y** USB ~10 s). El reset por software NO limpia el I²C de los ToF.
2. **`L=-- R=--` en OTOS = ALIMENTACIÓN** (no firmware): batería **>7,6 V** + switch ON + power-cycle. `0x64` en scan = brownout.
3. **IMU ZERO mirando al arco ANTES de jugar** (§5).
4. **Calib de línea NO persiste sin "Guardar EEPROM"** en el monitor de DOWN.
5. **Batería ~7,6 V degrada TODA la telemetría.** Medila con tester antes de debuguear.
6. **Boot de la TOP ~40 s** (4 ToF + 2 BNO). No conectes la CENTRAL hasta que termine.
7. **Exposición de cámaras LOCKEADA** (autos OFF; ver RUNBOOK §1.3).
8. **Usá el env EXACTO, no `pio run -t upload` pelado** (`default_envs=top_robot2_pri`).

---

## 7. DIAGNÓSTICO / BANCO — ❌ NO competencia
| Qué | Comando |
|---|---|
| Pose XY ToF+BNO (arquero, en validación TASK-227) | `pio run -e top_robot2_pri_xypose -t upload` |
| Paredes negras máx rango ToF | `pio run -e top_robot2_pri_tofmaxrange -t upload` |
| Sanity 3 placas (comms en CENTRAL) | `pio run -e diag_central_rx_all -t upload` |

---

## ⚠️ CONFLICTOS A RESOLVER CON GUSTAVO (antes de Incheon)
1. **Rol ↔ placa:** FUENTES-DE-VERDAD L47 dice "R1 arquero / R2 delantero"; la memoria/sesión dice Virginia=arquera con R2. **Definir qué placa juega qué rol.**
2. ~~**CENTRAL arquero**~~ ✅ **RESUELTO (equipo, 2026-06-29):** el arquero usa `central_robot2_arqueromix_quieto`. (Falta actualizar QUE-FLASHEO-HOY → ver #4.)
3. **DOWN R2:** `down_robot2` (canónico) vs `down_robot2_rt` (completo RT, ESTADO-ACTUAL 06-16). **Elegir.**
4. **QUE-FLASHEO-HOY (06-11) quedó stale** vs el trabajo posterior (arqueromix_quieto, RT down/central) → **reconciliar** la tabla canónica.
5. **Refuerzo del conflicto #1:** el comentario de `[env:down_robot2]` dice "(delantero)" y FUENTES L47 también (R1 arquero / R2 delantero) — esto **CONTRADICE** la memoria/sesión (Virginia = arquera con R2). NO está claro cuál es el correcto; es el mismo nudo del #1 y resolverlo define todo el cableado. **No lo decido yo.**
