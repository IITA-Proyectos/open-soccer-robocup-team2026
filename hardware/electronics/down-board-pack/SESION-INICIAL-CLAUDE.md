---
title: "Sesión inicial Claude — programar la placa DOWN"
date: 2026-05-24
status: vigente
audiencia: "Humano que abre una sesión Claude nueva para trabajar sobre DOWN"
---

# Cómo iniciar una sesión Claude para programar la placa DOWN

## 1. Carpeta a abrir

Abrir Claude (o cualquier asistente de código) **en la raíz del repo**:

```
C:\Users\violl\iitasoccer\open-soccer-robocup-team2026
```

> ⚠️ **CONFIRMAR PATH EXACTO**: en la máquina de Gustavo puede haber más
> de un clone del repo (por ejemplo `C:\Users\violl\futbol2026\...` además
> del de arriba). **El clone canónico es el de `iitasoccer/`**. Si abrís
> Claude en otro clone, va a ver una versión vieja sin los packs (porque
> los `git push` van a GitHub pero NO se replican automáticamente entre
> clones locales).
>
> **Antes de empezar, en la terminal verificá** que estás en el clone
> correcto y al día:
> ```bash
> pwd                                  # debe ser /c/Users/violl/iitasoccer/open-soccer-robocup-team2026
> git status                           # debe decir: On branch main, up to date with origin/main
> ls hardware/electronics/down-board-pack/README.md   # debe existir
> ```
> Si la carpeta `down-board-pack/` NO existe → estás en el clone
> equivocado o no hiciste `git pull origin main`.

> ⚠️ **No abrir Claude dentro de la carpeta del pack** (`hardware/electronics/down-board-pack/`).
> Razón: el código que se va a **modificar** vive en `software/teensy/Soccer 2026/src/down/`,
> NO dentro del pack. El pack es solo un snapshot curado para entender el
> subsistema; cualquier cambio de código se hace en el repo vivo.

## 2. Prompt para copiar al iniciar la sesión

Copiar exactamente este texto al chat:

```
Vamos a trabajar SOLO en la placa DOWN del robot Soccer 2026.

Tu primera lectura obligatoria es:
hardware/electronics/down-board-pack/README.md

Ese pack tiene TODO lo necesario para entender, programar, testear y
diagnosticar la placa DOWN en un solo lugar. Tiene un índice "pregunta →
doc" que te dice exactamente qué archivo abrir para cada cosa.

Reglas de esta sesión:

1. NO leer docs de las otras placas (CENTRAL, TOP, cámaras) a menos que
   sea estrictamente necesario para programar la DOWN. Si necesitás
   entender el otro lado de una comunicación, mirá puntualmente
   src/central/comm_down.{h,cpp} o src/top/comm_down.{h,cpp}, pero
   no te metas con sus packs completos.

2. NO leer el journal ni team-tasks ni research a menos que te lo pida
   explícitamente. Toda la info útil para programar está en el pack.

3. El código compilable y los tests viven en:
   - software/teensy/Soccer 2026/src/down/         (firmware vivo)
   - software/teensy/Soccer 2026/src/shared/       (módulos compartidos
                                                     que usa DOWN)
   - software/teensy/Soccer 2026/test/test_down_*/ (tests host-native)

   El pack tiene COPIAS snapshot de estos archivos para lectura. Si vas a
   modificar código, hacelo en el repo vivo (no en las copias del pack).

4. Regla de oro: si el pack contradice al código vivo, gana el código vivo.

5. Atribución de commits: ver AI-INSTRUCTIONS.md. Formato:
   Author: Claude Opus 4.7 (Anthropic)
   Requested-by: Gustavo Viollaz (@gviollaz)

6. NO modificar firmware en producción sin que yo lo apruebe. Vos
   proponés, yo apruebo. Tampoco cerrás TASKs de hardware como done —
   eso lo hace el equipo humano que tiene la placa en la mano.

7. Antes de proponer cualquier cambio, leé también:
   - docs/ESTADO-ACTUAL.md  (qué módulos están vivos, qué bloquea)
   - docs/FUENTES-DE-VERDAD.md (qué doc/módulo es canónico)

Empezá leyendo hardware/electronics/down-board-pack/README.md, decime qué
entendiste del estado actual de la placa DOWN, y esperá mi siguiente
instrucción antes de hacer cualquier cambio.
```

## 3. Pendientes humanos antes de programar firmware

Antes de tocar firmware de DOWN, conviene tener resueltos estos puntos
(la sesión Claude lo va a recordar al leer el pack, pero útil saberlos
de antemano):

| # | Pendiente | Quién | Bloquea |
|---|---|---|---|
| 1 | **Validación física del pinout** (TASK-026) | Enzo + Virginia/Elías con multímetro | Aplicar el pinout nuevo a `config_down.h` con confianza |
| 2 | Confirmar **orientación del montaje** del PCB (¿+Y = adelante?) | Enzo cuando arme el robot | Asegurar que la LUT `SENSOR_POS[]` no esté rotada |
| 3 | Confirmar **ambos OTOS U5/U6** poblados | Enzo o cualquiera mirando la placa | Saber si `DOWN_NUM_OTOS_CONNECTED=1` o `=2` |
| 4 | **Vendorear lib SparkFun OTOS** (TASK-012) | Cualquiera | Activar bloque `TODO_OTOS_LIB` en `otos.cpp` para test físico |

Si Claude propone trabajar en algo que depende de estos pendientes, va a
marcar el bloqueante y proponer alternativas que no dependan (por ejemplo,
tests host-native que no necesitan hardware).

## 4. Tareas típicas para esta sesión (ejemplos)

Cuando Claude termine de leer el pack, podés pedirle cosas como:

- "Actualizá `config_down.h` con el pinout validado del doc del pack §10
  (asume que TASK-026 ya quedó cerrada y los valores son los del schematic)."
- "Reescribí `sample_all_sensors_hardware()` en `line_ring.cpp` para
  usar los 12 selectores independientes (no 1 SEL compartido)."
- "Agregá la LUT `SENSOR_POS[32]` a `config_down.h` con las posiciones
  XY del pack §5b. Después modificá el cálculo de ángulo del centroide
  para usar posiciones reales en vez de asumir 11.25° entre sensores."
- "Vendoreá la lib SparkFun OTOS en `lib/` y activá el bloque
  TODO_OTOS_LIB de `otos.cpp`."
- "Agregá un test host-native en `test_down_geometry` que verifique el
  cálculo de ángulo con la nueva LUT de posiciones reales."

Cada tarea de estas implica:
1. Claude lee el código vivo afectado.
2. Propone un diff.
3. Vos lo aprobás (o pedís ajustes).
4. Aplica el cambio.
5. Corre los tests host-native (`pio test -e native`).
6. Si pasan: commit + push (con tu OK).

## 5. Cómo terminar bien la sesión

Cuando termines:

1. Confirmar que **todos los tests host-native pasan** (`pio test -e native`).
2. Confirmar que el commit final está pusheado (`git status` debe decir
   "Your branch is up to date with 'origin/main'").
3. **Actualizar `docs/ESTADO-ACTUAL.md`** si algún módulo vivo cambió de
   estado (de stub a activo, de buggy a probado, etc.).
4. **Actualizar `docs/FUENTES-DE-VERDAD.md`** si algún doc canónico cambió.
5. **Re-sincronizar el pack DOWN** si el snapshot quedó desactualizado:
   copiar de nuevo los `.cpp/.h` de `src/down/` a
   `hardware/electronics/down-board-pack/firmware/down/` y commitear.

> 📌 La regeneración del pack es manual hoy. Si querés un script
> `regenerate-down-pack.sh` que lo haga automático, pedíselo a Claude en
> esa misma sesión o en la siguiente.
