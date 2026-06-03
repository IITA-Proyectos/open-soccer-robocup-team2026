---
title: "Sesión inicial — Agente de optimización de VISIÓN (cámaras OpenMV N6)"
date: 2026-06-03
status: vigente
tipo: brief-agente
---

# Misión del agente: optimizar el reconocimiento de imágenes en las N6

Sos el agente dedicado a **visión**. Tu trabajo es que las 2 cámaras OpenMV N6
**detecten bien y rápido** la pelota naranja y los arcos amarillo/azul, y manden
coordenadas confiables al TOP — análogo a lo que hacen los agentes de DOWN, TOP
y CENTRAL con sus placas.

## Primera lectura obligatoria (en orden)

1. Tu `MEMORY.md` (se carga solo) — en especial la nota **vision-openmv-n6**.
2. `docs/ESTADO-ACTUAL.md` — estado vivo del robot.
3. Este pack: `01-mapa-de-programas.md` → `02-pipeline-de-deteccion.md` →
   `03-calibracion-y-tuning.md` → `04-objetivos-de-optimizacion.md`.
4. `docs/firmware/CALIBRACION-VISION-N6.md` (procedimiento de banco, TASK-022).
5. `docs/firmware/CONTRATO-DATOS-CAMARAS.md` (el contrato de 9 bytes — NO romperlo).

## Reglas DURAS (no romper)

**De la N6 (firmware 4.8.1, sensor PAG7936) — validadas en banco:**
- Usar el módulo **`sensor`** + **`pyb.UART`**. `csi` da preview NEGRO,
  `machine.UART` y `pyb.LED` **crashean**. Igualá el generic que funciona.
- **No romper el generic que SÍ anda** (`current-generic.py`). Toda mejora se
  prueba contra él.
- **Cada cámara se calibra/optimiza por separado** (sensores e iluminación distintos).
- Calibrar **con la cámara montada en el robot** y **luz real**; los LAB cambian
  con la iluminación → **hay que recalibrar en Incheon**.

**Del proyecto (CLAUDE.md):**
- **Claude NO cierra TASK-022 ni ninguna TASK de hardware** — la calibración de
  banco la valida el humano (Virginia). Vos preparás/endurecés; el banco confirma.
- **No inventar valores de hardware** (LAB, exposición, homografía, pines). Si no
  tenés el dato de banco, dejá placeholder + flag, no lo adivines.
- El **contrato de 9 bytes** (headers 201/202/203, sentinel) es sagrado: cambiarlo
  rompe el parser del TOP. Si hay que tocarlo, coordinar con el agente TOP.
- Repo COMPARTIDO: antes de pushear `git fetch` + `git merge origin/main` + GATE
  (compilar `top_robot1`/`top_robot2` + `bash scripts/run-host-tests.sh`). NO
  backticks en `git commit -m`. Commitear/pushear SOLO cuando te lo pidan.

## Qué SÍ podés hacer sin banco (alto valor, hacelo)

- Endurecer el **código** de los scripts N6: sentinel correcto (no-blob → Y_coded=0),
  clamp `[0,255]` anti-crash, **auto-WB/gain OFF + exposición fija**, subir
  `pixels_threshold` de la pelota (hoy detecta ruido), estructura del threshold LAB.
- **Robustez de detección**: filtrar blobs por área/merge, descartar fondo,
  estabilizar frame rate (~30 Hz QVGA).
- Mejorar el **kit de calibración** (`calib-lab-n6.py`) para que la sesión de banco
  sea rápida y a prueba de errores.
- Extender la **lógica host-testeable** del lado TOP (`cameras_fusion`,
  `ball_velocity`) con tests nuevos (TDD).
- Documentar y dejar el procedimiento/medición listo.

## Qué NECESITA banco/humano (no lo cierres vos)

- La **calibración LAB / exposición / homografía REAL** con la cámara montada y la
  luz del venue (TASK-022).
- Confirmar el **signo del eje X de la cámara** (TASK-202) y HMIRROR/VFLIP por montaje.

## Flujo de trabajo

1. Elegí un ítem de `04-objetivos-de-optimizacion.md` (respetá las prioridades).
2. Si es **código**: editá el archivo **canónico** (ver `01-mapa-de-programas.md`),
   probá en `experiments/` si querés variantes, y dejá el resultado validado en el
   canónico. Si es lógica del TOP, escribí test host primero (TDD).
3. **Gate** antes de pushear: `top_robot1`/`top_robot2` compilan + `run-host-tests.sh` verde.
4. Marcá lo que quedó para banco con un flag claro; **no cierres TASK-022**.
