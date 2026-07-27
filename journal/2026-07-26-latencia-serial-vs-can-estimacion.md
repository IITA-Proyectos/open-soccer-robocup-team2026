---
title: "Estimación de latencia peor-caso DOWN→TOP→CENTRAL: serial actual vs CAN"
date: 2026-07-26
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: final
tags: [journal, comunicacion, latencia, can, uart, tiempo-real, analisis]
---

# Journal — latencia serial vs CAN (estimación)

## Qué se hizo

Gustavo pidió estimar la latencia peor-caso del camino *dato leído en DOWN → se
manda al TOP → TOP procesa → lo manda a CENTRAL*, hoy (UART) y con CAN.

Se extrajeron los parámetros **reales** del firmware (commit `9a56923`) con un
workflow multi-agente (5 lectores paralelos + 1 verificador adversarial), y sobre
esos números se hizo la aritmética del peor caso. Resultado en
[`docs/decisions/2026-07-26-latencia-serial-vs-can-estimacion.md`](../docs/decisions/2026-07-26-latencia-serial-vs-can-estimacion.md).

## Resultado principal (contra-intuitivo)

| Escenario | Peor caso | vs hoy |
|---|---|---|
| HOY (UART, con relay por TOP) | **≈ 41 ms** | — |
| CAN, dato que **necesita** al TOP | ≈ 38 ms | **−6 %** |
| CAN sin relay | ≈ 25 ms | −39 % |
| CAN + publicación por evento | ≈ 15 ms | −63 % |
| CAN + evento + consumo por evento | ≈ 5 ms | −88 % |
| **UART actual + sin relay + por evento (SIN CAN)** | **≈ 6 ms** | **−85 %** |

**Descomposición del peor caso de hoy:** gates de reloj **30 ms (73 %)** ·
I²C del OTOS 4 ms (10 %) · proceso 4 ms (10 %) · **cable 2,85 ms (7 %)**.

**Conclusión:** el cuello de botella **no es el bus**, son los *gates* fixed-rate
(3 × 10 ms en serie) y el relay por el TOP. Casi toda la mejora posible se logra
**sin cambiar de hardware**. CAN sigue justificándose por pines/EMI/escala/
determinismo — **nunca por latencia**, coherente con el análisis previo.

## Hallazgos de la extracción (verificados archivo:línea)

- **Baud único 230400** en los 3 enlaces inter-placa; overhead `proto.h` = 7 B.
- `WorldSnapshot` = **31 B** (v3, `static_assert` en `types.h:139`) → 38 B en
  cable → 1,65 ms. `LineStatusV2` = 16 B → 23 B → 1,00 ms.
- **Todas las cadencias son fixed-rate por gate; CERO event-driven.** Línea
  200 Hz, pose/vel 100 Hz, snapshot 100 Hz (ISR `IntervalTimer`), strategy
  100 Hz (`>=10 ms`, con jitter).
- **Corrección de topología importante:** la **línea NO pasa por el TOP** para
  llegar a CENTRAL — `down_tx` **difunde a los 2 enlaces** (`Serial1`→CENTRAL
  directo + `Serial5`→TOP). Por eso el freno de borde tarda **~8 ms** y no 41.
  **Es la prueba interna de la tesis**: el camino que ya eliminó el relay es
  5× más rápido, y ahí CAN solo aportaría ~1 ms.
- **Determinante del jitter:** `otos_tick` I²C **bloqueante ~3–4 ms cada 10 ms**
  en el binario de competencia (100 kHz, sin burst). La mitigación
  (`-DDOWN_OTOS_FAST_I2C`, → ~0,5 ms) **ya está escrita pero gateada a un env de
  banco**, no al de partido.
- Carga estimada de un bus CAN 1 Mbps con TODO el tráfico: **~13,5 %**.

## Lo que NO está hecho (importante)

**Nada de esto está medido en hardware.** Es aritmética sobre constantes del
código. El WCET real del loop nunca se midió. Marcado como **P0 de aprendizaje**
(T4 del doc) con plan de banco en §7: `LoopMonitor` para p99 de período,
`stamp_ms`/`sample_age_ms` para latencia end-to-end, y GPIO + osciloscopio para
el peor caso fino — **con los motores girando** (EMI real).

## Recomendación registrada

1. Medir primero (§7). 2. Atacar relay → evento → I²C del OTOS (las tres,
gratis en hardware, llevan de ~41 ms a ~6 ms). 3. CAN sí, pero por sus razones
reales (pines, EMI, escala, determinismo), no por velocidad.

## Nota de proceso

El workflow de extracción se lanzó el 2026-06-23 y quedó cortado al cerrarse la
sesión; los 6 agentes **habían completado** y sus resultados estaban cacheados en
`journal.jsonl` del run — se recuperaron de ahí en vez de re-ejecutar (evitó
repetir ~2,6 M tokens de lectura de firmware).
