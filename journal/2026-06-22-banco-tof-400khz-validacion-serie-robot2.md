---
title: "Banco — validación del bus ToF a 400 kHz en robot2 (lectura por serie, sin GUI)"
date: 2026-06-22
author: "Claude Opus 4.8 (Anthropic) + Gustavo Viollaz (placa)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: banco-validacion
toca-competencia: NO (no se tocó firmware; solo lectura de telemetría por USB)
status: T1–T5 ✅ EFECTUADOS Y PASADOS (robot2, Gustavo en la placa) · T6/T7 opcionales
---

# Validación en banco del bus ToF a 400 kHz — robot2

Robot2 ya flasheado con `top_robot2_pri` (bus de ToF a 400 kHz). Sesión de banco con la TOP
conectada por USB (COM22). Claude leyó la telemetría **por serie, headless** (sin la GUI del
monitor): modo MÁQUINA (`STREAM ON`+`PING`) para el JSONL por sensor y modo HUMANO (ENTER) para
el dump de config. **Solo lectura** — no se mandó ningún comando de config (ni IMU ZERO, ni ZONEMASK,
ni CFG SAVE). Herramienta: `tools/monitor-base/probe_top_serial.py` (agregada en este commit).

## Mapeo índice → posición física (robot2, validado)

De la config del firmware (`CFG ... tof[0:@0 1:@180 2:@270 3:@90]`, leída en vivo) **+** confirmación
física poniendo un objetivo frente a cada sensor:

| Índice | Bearing (EEPROM) | Posición | Confirmación física |
|---|---|---|---|
| **T0** | 0°   | **FRENTE**    | placa plana → 177 mm, **16/16 zonas**, rock-solid ✅ |
| **T1** | 180° | **ATRÁS**     | por config (el target de atrás cayó fuera de su FoV; lee mid-range) |
| **T2** | 270° | **DERECHA**   | obstáculo → 197 mm, **16/16 zonas** ✅ |
| **T3** | 90°  | **IZQUIERDA** | objetivo → 221 mm, **16/16 zonas** ✅ |

> Es la config de la EEPROM de **robot2**; robot1 tiene la suya (verificar aparte si se necesita).

## Resultados de los tests (plan T1–T7 de `docs/firmware/TOF-BUS-400KHZ-PLAN-PRUEBAS.md`)

- **T1 — arranca/vivo: ✅** stream sano ~21 fps, todos los subsistemas reportando, `resync=0`.
- **T2 — distancias por sensor: ✅** los 4 ToF leen y responden a 400 kHz. 3 de 4 (frente/derecha/
  izquierda) clavaron objetivos puestos a mano con **16/16 zonas, ±1 mm**. El sensor que antes "no daba
  lecturas" (T2/derecha) **estaba SANO**: daba `--` porque miraba al vacío (nada en rango); con un objeto
  enfrente lee perfecto. **Despejada la duda histórica.**
- **T3 — sin caídas: ✅** a lo largo de ~varios minutos de lecturas, ningún sensor entró en stale ni
  desapareció del round-robin. Sin resets.
- **T4 — heading sano: ✅** girando el robot ~90° a la izquierda, el heading siguió el giro **suave y
  continuo** (149.7° → −132.1°, ~78° efectivos para un giro a ojo de "90°"), manejó bien el wrap de
  ±180°, y **se asentó estable −132.1° por 12 s sin deriva**. `valid=True` todo el tiempo. **NO está
  congelado.** Convención confirmada: **izquierda = heading sube (CCW+)**.
- **T5 — loop 400 vs 100: ✅** mismo método/escena: **400 kHz = 150.511 pasadas/s** vs
  **100 kHz = 71.947 pasadas/s** → el bus rápido da **~2,1× más throughput de loop**. O sea el
  `getRangingData()` de los ToF era el **costo dominante** del lazo (a 100 kHz pasaba ~70% del tiempo
  bloqueado leyendo ToF); a 400 kHz se libera. Es la justificación cuantificada del cambio. (Son
  *pasadas de loop*, no la tasa de control —el snapshot va por timer @100 Hz— pero mide el ahogo por I/O.)
- **T6 (soak 3–5 min) y T7 (con motores): no corridos** (opcionales).

## Veredicto

**El bus de ToF a 400 kHz está VALIDADO en banco para robot2** (T1–T4 ✅). El cambio anda: los 4 ToF
leen correcto y estable, sin caídas, y el heading del primario (en Wire2) no se ve afectado. (Robot1 se
validó antes a nivel "anda"; su detalle por-sensor queda pendiente si se quiere.)

## Items abiertos (NO son del bus de ToF)

1. **Centinela (2º BNO) = 0.00** en TODAS las lecturas (`cent:@1Hz 0.00`). El backup IMU se lee pero no
   da un heading real. El **primario está perfecto** (trackea, sin drift). Es del dominio BNO/centinela
   → mirar con la skill `bno055-imu-heading-robocup` si se quiere la redundancia operativa.
2. **Cámaras `N/N` + enlace DOWN `STALE`** en las lecturas del final (al principio estaban vivos `Y/Y`).
   Probable: quedó **solo el USB alimentando la TOP** (batería apagada → cámaras y DOWN sin alimentación).
   Confirmar que es eso y no una caída real.

## Qué sigue
- ✅ T5 hecho (400=150.511 vs 100=71.947 pasadas/s, ~2,1×). Robot2 vuelto a `top_robot2_pri` (400, competencia).
- Opcional: T6 soak, T7 con motores, validación por-sensor de robot1, y el centinela del 2º BNO.
