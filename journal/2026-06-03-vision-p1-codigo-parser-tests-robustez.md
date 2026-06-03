---
title: "Visión P1 [CÓDIGO]: tests del parser de cámara + robustez de detección + kit calib + análisis eje X"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: final
tags: [vision, camaras, openmv, n6, tests, host-native, robustez, P1, TASK-022, TASK-202]
robot: ambos
area: vision
tipo: resultado
---

# Visión P1 [CÓDIGO] — todo lo que NO necesita banco

## Contexto

Sesión del **agente de optimización de visión** (cámaras OpenMV N6 + parser/fusión
del TOP). Objetivo acordado con Gustavo: atacar los ítems **[CÓDIGO] P0/P1** del
backlog de visión (`hardware/electronics/vision-optimization-pack/04-objetivos-de-optimizacion.md`),
es decir todo lo que se puede endurecer/testear **sin la cámara en el banco**.

Hallazgo de entrada: los **bugs P0 de higiene** (sentinel=0, clamp `[0,255]`, autos
off con `BRING_UP=False`, `NARANJA_PIXELS_MIN=20`) **ya estaban aplicados** en los
`cam-*-n6.py` (sesión previa del 2026-06-03). Lo que quedaba era P1.

**Nota de proceso:** se intentó primero un workflow de 5 agentes en paralelo
(particionados por archivo). El servidor devolvió **rate-limit** y abortó el batch
(2 de 5 archivos quedaron a medio editar). Se completó todo **secuencialmente en la
sesión principal**, revisando y terminando las ediciones parciales.

## Qué se hizo

**A — Suite host-native del parser de cámara (Gap 7 del contrato).** `src/top/cameras.cpp`
(la máquina de estados de 9 bytes, donde vive la heurística de pelota-fantasma) no
tenía **ningún** test. Nuevo `test/test_cameras_parser/test_main.cpp` (10 tests):
decode normal, sentinel correcto, **caracterización del bug fantasma** (Y_coded=100 →
visible), arco amarillo/azul solo, basura pre-header, resync por HEADER2 ausente,
header-como-dato en stream alineado (R6), `reset()`, dos packets seguidos. Técnica:
unity-build (`#include "../../src/top/cameras.cpp"`) para que el harness lo compile
sin tocar `run-host-tests.sh` ni `platformio.ini` ni mover el archivo a `src/shared`.

**B + C — Robustez de detección y frame-rate en los scripts N6.** En
`cam-frontal-n6.py` y `cam-trasera-n6.py`:
- Filtro de **forma** aplicado **solo a la pelota** (es redonda): helper `is_ball_like`
  por aspect ratio (`w/h`) + densidad (`density()`), **fail-open** (si la API no existe
  en esta N6, NO filtra — prioridad: ver la pelota antes que crashear). Constantes
  tuneables con default conservador (`BALL_SHAPE_FILTER=True`, `BALL_MIN_DENSITY=0.45`,
  `BALL_ASPECT_MIN/MAX=0.5/2.0`). Los **arcos NO** se filtran por forma (solo área).
- ROI opcional de pelota `BALL_ROI=None` (frame completo) con comentario de que recortar
  el horizonte/público depende del montaje → **verificar en banco** antes de activar.
- fps agregado al print de bring-up (`BRING_UP=True`); en competencia sigue sin imprimir.

**D — Kit de calibración `calib-lab-n6.py`** (front del agente + copia a back
sincronizada, hashes idénticos): cicla los **3 colores** sin re-Run (consola reporta
los 3), texto sobre la imagen (`draw_string`), márgenes por canal (L más holgado que
A/B), y **try/except** en `get_statistics`/`draw_*` para degradar sin crashear si algún
feature no está en la N6 (misma trampa que csi/machine.UART/pyb.LED).

**E — Tests borde de fusión/velocidad.** `test_cameras_fusion` 16→**19** (ángulo a la
**izquierda** → negativo —conecta con F—, promedio real de puntos distintos, confianzas
exactas 80/95). `test_ball_velocity` 16→**17** (velocidad diagonal vx+vy).

**F — Análisis del eje X (sin tocar código/contrato).**
`research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md`: X se codifica
**sin offset** mientras Y sí lo tiene (`Y_coded=Y+100`), y el clamp uint8 del script
**aplasta a 0 toda la izquierda** → una pelota a la izquierda se transmite como "al
frente". Más profundo que TASK-202 (es **representabilidad**, no solo signo). Toca el
contrato de 9 bytes → **decisión pendiente** + coordinación con el agente TOP.

## Qué se midió/observó

- **GATE host-native completo:** `bash scripts/run-host-tests.sh` → **403 tests / 33
  envs / 0 fallos** (incluye `test_cameras_parser` 10, `test_cameras_fusion` 19,
  `test_ball_velocity` 17). Antes de esta sesión: 32 envs.
- **Sintaxis Python:** `python -m py_compile` OK en los 4 scripts editados
  (`cam-frontal-n6.py`, `cam-trasera-n6.py`, ambos `calib-lab-n6.py`).
- **No se midió nada en hardware** — esta sesión es 100% host/código. Toda la
  detección real (LAB, exposición, homografía, signo de X) sigue dependiendo del banco.

## Conclusión

Los ítems [CÓDIGO] P1 de visión que no necesitan banco quedaron **hechos y verificados
host-native**: el parser más crítico ahora tiene red de seguridad (10 tests), los
scripts de competencia descartan falsos positivos del fondo de forma segura y
reversible (flag), el kit de calibración es más rápido y a prueba de crashes para
Incheon, y se documentó un problema de codificación del eje X que conviene resolver
**antes** del banco. El contrato de 9 bytes **no se tocó**.

## Próximos pasos

**Para el banco / humano (NO cerrado por Claude):**
- **TASK-022** sigue abierta: calibración real LAB + exposición + homografía con la
  cámara montada y luz real. El kit `calib-lab-n6.py` está listo para esa sesión.
- **TASK-202** (signo eje X): validar izquierda/centro/derecha; ver el plan de banco
  en el research doc F. Si se confirma el síntoma, decidir opción A (offset simétrico
  en el contrato) vs B (restar 100 en el TOP) **con el agente TOP**.
- Verificar en la N6 real que `is_ball_like` (density/aspect) y el `draw_string` del
  kit funcionan; el filtro de forma es fail-open por las dudas.
- Confirmar `UART_PORT` y `HMIRROR/VFLIP` por montaje de cada cámara.

**Decisión de Gustavo:** commit/push de este changeset (ver resumen de sesión) y cómo
manejar la rama `agente/vision` (hoy no existe; el trabajo está en el working tree de
`main`).
