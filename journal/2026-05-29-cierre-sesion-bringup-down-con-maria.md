---
title: "Cierre de sesión: bring-up DOWN en banco con María (OTOS revividos, línea TX, y hallazgo de worktree atrasado)"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "María Viollaz (en la compu de Gustavo @gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7, Anthropic)"
status: final
tags: [down, central, bringup, otos, linea, uart, protocolo, cierre-sesion, leccion-proceso, sync-main]
robot: ambos
area: integracion
tipo: resultado
related-journals: [2026-05-29-down-task-302-otos-vendoreado-offline.md, 2026-05-29-down-central-bringup-debug-serial.md, 2026-05-29-otos-revividos-power-bateria.md]
---

# Cierre de sesión — bring-up DOWN en banco con María

Sesión larga e interactiva con María (en la compu de Gustavo), probando la placa
DOWN en el banco y empezando la integración con CENTRAL. Resumen honesto: se
avanzó bastante en hardware real, pero se descubrió al final que **este worktree
(`agente/down`) está 13 commits atrás de `main`**, lo que hizo que parte del
trabajo y de la información dada fuera sobre base vieja.

## Qué se hizo (commiteado a `agente/down`)

| commit | qué |
|---|---|
| `4eabbb9` | **TASK-302**: vendorear OTOS + SparkFun_Toolkit en `lib/` → `[env:down]`/`[env:diag_down]` compilan 100% offline (Avast). |
| `a69fce5` | `[env:down_debug]` + print de bring-up por USB. |
| `8848fe7` | regla de encendido OTOS (batería/power-cycle) elevada a checklist visible en ESTADO-ACTUAL + journal. |
| `1725c39` | `down_debug` muestra **LÍNEA=SÍ/NO + pose OTOS** (en vez del volcado de 32 sensores). |
| `3ee4bf5` | par de test de enlace mínimo DOWN↔CENTRAL (`diag_down_send1` / `diag_central_recv1`, "mandar un 1"). |

Working tree limpio, todo pusheado (`HEAD == origin/agente/down == 3ee4bf5`).

## Qué se midió / observó (hardware real, con María)

- **OTOS muertos → revividos.** Daban `[L=-- R=--]`, un bus I²C vacío (U5) y el
  otro en `0x64` (la dirección del OTOS es `0x17`). **Causa real: la batería
  estaba conectada pero SIN entregar corriente** → el riel 3.3 V del MP1584 (que
  alimenta los OTOS) quedó hambriento → brownout. **Fix: batería entregando + power
  cycle completo → ambos OTOS en `0x17`, funcionando.** Recurrencia (refinada) del
  bug del 2026-05-24. Detalle: los 32 sensores de luz seguían leyendo con el riel
  flojo, pero los OTOS no.
- **Línea TX (lado DOWN):** DOWN transmite `LineStatusV2` por Serial1 (verificado
  por compilación + diseño; la validación HW del enlace queda pendiente). Los 32
  sensores de luz leen perfecto.
- **Protocolo:** se documentó para María el formato de trama (`0xAA | LEN | TYPE |
  SEQ | PAYLOAD | CRC16 | 0x55`), los TYPE (0x10 línea, 0x11/0x12 OTOS) y ejemplos
  de paquetes byte-por-byte con CRC real.

## Hallazgo crítico + lección de proceso (lo más importante)

Al verificar la pregunta de María ("¿el receptor decodifica bien lo que mando?")
encontré que, **en este worktree**, `central/comm_down.cpp` decodificaba el
`LineStatus` viejo (5 B) mientras DOWN manda `LineStatusV2` (16 B) → habría
descartado toda la línea. **PERO** antes de escribir un fix, chequeé `git log
origin/main` y descubrí que **ya estaba resuelto en `main`** (`e2ca3af fix(central):
ingest de linea DOWN->CENTRAL a LineStatusV2 (P0)`). **No se escribió un fix
duplicado.**

Eso destapó que `agente/down` está **13 commits atrás de `main`**, incluyendo:
- `e2ca3af` / `b67f984` — fix de la línea a LineStatusV2 (el "bug" ya resuelto).
- `695e7b0 fix(hw-truth): UART TOP→CENTRAL es Serial5 (20/21), no Serial2 (7/8)` —
  **mi cuadrito de UARTs dado a María estaba mal en esa fila** (info de código viejo).
- `20976f0 docs(coach): runbook sesión prueba DOWN+CENTRAL` — runbook para esta
  misma prueba, que no se consultó.
- `4b56894` — mi propio trabajo anterior (P0.2/P1.5/P1.6 + vendoring) ya mergeado a main.

**Lecciones (las marcó María, dos veces):**
1. Ante una falla, **grepear `journal/` + `ESTADO-ACTUAL` + `team-tasks/` PRIMERO**
   (el bug de power-cycle de los OTOS estaba documentado desde 2026-05-24; se perdió
   tiempo rediagnosticando).
2. **Sincronizar con `main` al INICIO de la sesión** (`git pull` / rebase). `main`
   avanzó —otros agentes mergearon— y se trabajó/aconsejó sobre base vieja (el fix
   de la línea y la corrección de UARTs ya estaban en main).
3. Lo que sí se hizo bien: chequear `git log` antes de escribir el fix → no se
   duplicó (`e2ca3af` ya lo tenía).

## Pendientes (próxima sesión / equipo)

- **SINCRONIZAR `agente/down` con `main`.** Lo hace Gustavo desde el repo principal
  (`git merge --no-ff agente/down` y/o traer main acá). Las reglas multi-agente
  prohíben rebase desde worktree (regla 4). Para flashear CENTRAL **correcto**,
  construir desde `main` (que ya tiene `e2ca3af`), no desde este worktree atrasado.
- **Re-derivar el cuadrito de UARTs desde `main`** (la fila TOP↔CENTRAL es Serial5
  por `695e7b0`, no Serial2 como se documentó en chat).
- **Validaciones HW (Claude NO cierra HW):**
  - **TASK-301** — P0.2/P1.5/P1.6 en banco.
  - **TASK-029** — precisión OTOS sobre superficie texturada (no hoja A4).
  - **Enlace DOWN→CENTRAL end-to-end** — correr `diag_down_send1`/`diag_central_recv1`
    ("mandar un 1") y después el firmware real desde `main`; confirmar que CENTRAL
    reacciona a la línea (fail-safe de borde).
  - **TASK-028** — medir 3.3 V del MP1584 con multímetro (P0.3) + doc operativo formal.
- **Conflicto pines 7/8 (TASK-036)** sigue bloqueando recibir DOWN en CENTRAL
  mientras se manejan motores.

## Estado del hardware-up (regla 8)

| Condición | Estado |
|---|---|
| Robot encendido | ✅ |
| OTOS leyendo pose | ✅ (revividos hoy) |
| 32 sensores de línea | ✅ |
| DOWN reportando línea por UART **real** a otra placa | ⚠️ TX lista; falta validar recepción end-to-end |
| COMM flasheada (TASK-006) | ❌ |

Moratoria sigue vigente, pero hubo avance real de bring-up. La próxima sesión DOWN
debe **arrancar sincronizando con `main`**.
