---
title: "Pack de optimización de VISIÓN (cámaras OpenMV N6) — base de trabajo del agente"
date: 2026-06-03
status: vigente
audiencia: "el agente (IA) que optimiza el reconocimiento de imágenes + humanos de banco"
fuente-canonica: "docs/FUENTES-DE-VERDAD.md"
---

# Pack de optimización de VISIÓN — cámaras OpenMV N6

## Para qué existe este directorio

Es la **base de trabajo de UN agente** dedicado a **optimizar el reconocimiento
de imágenes** (pelota naranja + arco amarillo + arco azul) en las **2 cámaras
OpenMV N6** del robot. Es el equivalente, para visión, de los packs de placa
(`down-board-pack/`, `central-board-pack/`, `top-board-pack/`) que usan los otros
3 agentes.

## Regla de oro (importante — leer antes de tocar nada)

> **Este pack NO duplica el código.** Los scripts de cámara y el parser del TOP
> viven en UN solo lugar canónico cada uno (ver `01-mapa-de-programas.md`). El
> agente **edita esos archivos EN SU LUGAR**, no copias acá. Este pack es la
> **misión + el conocimiento + el backlog + un sandbox de experimentos**.
>
> Se decidió así a propósito: una 4ª copia de los scripts se desincroniza y
> termina "pisando" con datos viejos. Si algo de este pack contradice al código
> vivo, **gana el código vivo**.

## Cómo arranca el agente

**Primera lectura obligatoria:** [`SESION-INICIAL-CLAUDE.md`](SESION-INICIAL-CLAUDE.md)
(la misión, las reglas duras de la N6, qué puede hacer sin banco y qué necesita banco).

## Índice — pregunta → doc

| Pregunta | Doc |
|---|---|
| ¿Cuál es la misión y cómo trabajo? | `SESION-INICIAL-CLAUDE.md` |
| ¿Qué programas hay y DÓNDE está el canónico de cada uno? | `01-mapa-de-programas.md` |
| ¿Cómo funciona la detección de punta a punta? | `02-pipeline-de-deteccion.md` |
| ¿Cómo calibro/tuneo (LAB, exposición, flip, homografía)? | `03-calibracion-y-tuning.md` |
| ¿Qué hay que optimizar, en qué orden, y cómo lo mido? | `04-objetivos-de-optimizacion.md` |
| ¿Dónde dejo variantes/resultados sin romper lo canónico? | `experiments/` |

## Estado del subsistema (al 2026-06-03)

- Las 2 N6 **muestran color y transmiten** los 9 bytes (migración H7→N6 + bugs P0 resueltos).
- **Bloqueante #1 de todo el robot: el color LAB NO está calibrado → no ve la pelota** (TASK-022).
- Procedimiento de banco listo: [`docs/firmware/CALIBRACION-VISION-N6.md`](../../../docs/firmware/CALIBRACION-VISION-N6.md).
