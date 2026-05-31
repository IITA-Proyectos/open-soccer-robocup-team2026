---
id: TASK-201
title: "Multímetro: continuidad LP de cada ToF → pin Teensy (bodge bus único)"
date_created: 2026-05-30
date_due: 2026-06-03
assigned: [Enzo]
priority: P1
status: pending
estimated_hours: 1
blocks: [enumeracion-4-tof, task-035-validacion-hardware]
blocked_by: []
tags: [hardware, top-board, tof, lp, xshut, bodge, multimetro, i2c]
---

# TASK-201 — Multímetro: continuidad LP de cada ToF → pin Teensy

## Por qué

El 2026-05-30 recableaste los 4 ToF al bus I2C principal (`Wire`, 18/19) y
cableaste la pata **LP** de cada uno a un pin del Teensy (reusando la traza de
INT). Diagnóstico de banco con `diag_top_tof_census`
(ver `journal/2026-05-30-top-tof-bus-unico-lp-no-controlable.md`):

- Bus sano (BNO en 0x28 ✅), hay 1 ToF vivo (0x29 ✅).
- **Pero el Teensy NO controla el LP de NINGÚN ToF**: probé los 8 pines libres
  (2,9,10,11,12,13,22,23) en **ambas** polaridades → todos "sin efecto".
- **Solo 1 ToF es usable** (el frontal U2, que responde fijo en 0x29).

Sin control LP individual es **imposible** enumerar 4 ToF idénticos en un bus
(todos nacen en 0x29 y al cambiar dirección se mueven juntos). El software ya
hizo todo lo que podía. Ahora necesito los datos físicos.

## Qué necesito (medir con multímetro, placa alimentada)

Para **cada** ToF — U2 (frontal), U3 (trasero), U5 (izq), U17 (der):

| ToF | LP→pin Teensy | ¿Bodge en pad LP (pin1) o INT (pin2)? | 3V3 en Vin? | SDA→18? | SCL→19? |
|-----|---------------|----------------------------------------|-------------|---------|---------|
| U2  |               |                                        |             |         |         |
| U3  |               |                                        |             |         |         |
| U5  |               |                                        |             |         |         |
| U17 |               |                                        |             |         |         |

### Cómo medir cada columna
1. **LP→pin Teensy**: punta en el pad **LP (pin 1)** del módulo ToF, otra punta
   recorriendo los pines del Teensy hasta que pite continuidad. Anotá el número
   (o "ninguno" si no pita con ninguno).
2. **Pad correcto**: confirmá que el cable del bodge esté soldado al pad **LP
   (pin 1)**, **NO** al pad **GPIO/INT (pin 2)**. Son adyacentes y es fácil
   haberlos confundido. (En el símbolo: pin1=Xshut/LP, pin2=GPIO/INT.) El INT
   es justo el que el PCB tenía como NC — si el cable quedó ahí, no controla nada.
3. **3V3 en Vin**: que el ToF esté alimentado (Vin ~3.3V respecto a GND).
4. **SDA→18 / SCL→19**: continuidad del bus de datos desde cada ToF al Teensy.

## Posibles hallazgos y qué significan
- **Cable LP en pad INT (no LP)** → por eso no controla. Resoldar al pad LP.
- **Sin continuidad** → cable flojo / soldadura fría. Rehacer.
- **LP no cableado (NC)** → falta el jumper. Agregarlo.
- **ToF sin 3V3 o sin SDA/SCL** → ese sensor ni comunica. Revisar.

## Criterio de cierre
- Tabla de arriba completa (4 filas).
- Decisión: ¿se puede dejar ≥3 LP controlables con re-trabajo? (sí/no + qué falta)
- Si quedan pines LP reales distintos a la hipótesis → pasármelos para re-correr
  `diag_top_tof_census` apuntando a ellos.
- Journal entry con los números medidos.

## Decisión de arquitectura que destraba (cruzar con TASK-033/034)
Si el bodge LP no se puede hacer andar con ≥3 sensores, hay 2 caminos:
- (A) Insistir con el bodge hasta tener ≥3 LP controlables.
- (B) Volver a **2+2 en dos buses** (1 XSHUT por bus, más simple) y comunicar
  con DOWN por **UART (Serial1)** en vez de I2C — que es lo que el firmware ya
  usa hoy. Esto liberaría la presión sobre el bodge.

## Cómo re-testear después del re-trabajo
```
pio run -e diag_top_tof_census -t upload
pio device monitor -b 115200
```
(power-ciclar la placa antes para partir limpio). Objetivo: que el veredicto
diga "LP control FUNCIONA en >=3 ToF".

## Cambios de estado
- 2026-05-30: creada por Claude (Opus 4.8) tras el diagnóstico de banco que
  determinó que ningún LP llega al Teensy, a pedido de Gustavo Viollaz.
