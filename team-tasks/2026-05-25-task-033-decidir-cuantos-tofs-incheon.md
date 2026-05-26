---
id: TASK-033
title: "Decidir cuántos TOFs llevar a Incheon (2 sin rework vs 4 con bodge)"
date_created: 2026-05-25
date_due: 2026-05-29
assigned: [gviollaz]
priority: P1
status: pending
estimated_hours: 0.5  # decisión + journal con plan
blocks: [scope-firmware-tof-incheon]
blocked_by: []
tags: [decision, top-board, tof, hardware, incheon]
---

# TASK-033 — Decidir cuántos TOFs llevar a Incheon

## Resumen

Definir si para Incheon llevamos 2 ToFs (1 por bus I²C, sin rework
hardware) o 4 ToFs (requiere bodge: soldar 4 jumpers Xshut → GPIOs
libres del Teensy). Decisión define scope del firmware ToF de los
próximos días.

## Contexto

Hallazgo forense del 2026-05-25 (ver `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`):
los 4 pines XSHUT/LPn de los slots ToF en TOP rev 1.0 están
intencionalmente sin conectar (NC flags explícitos en SCH, 0 nets en
PCB). Sin XSHUT individual no se pueden enumerar 2 ToFs en el mismo
bus I²C (ambos arrancan en 0x29 → colisión).

Como TOP rev 1.0 tiene 2 buses (Wire I²C0 y Wire1 I²C1), el máximo
soportado SIN REWORK es:
- 1 ToF en bus Wire → U2 (frontal), ya validado el 2026-05-24.
- 1 ToF en bus Wire1 → U5 (¿lateral / trasero? confirmar slot físico
  con Enzo).

Para los 4 ToFs originalmente planeados (U2, U3 en Wire + U5, U17 en
Wire1) hay que rework. La decisión de fondo (TOP rev 1.1 con XSHUT
ruteados) ya está capturada en
`research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md` para
post-Incheon. Hoy se decide qué hacemos para Incheon.

## Decisión a tomar

¿Para Incheon vamos con **2 ToFs sin rework** o **4 ToFs con bodge**?

## Opciones con trade-offs

### Opción A — 2 ToFs sin rework

**Qué hay que hacer.**
- Activar U2 (frontal, ya funcionando) en Wire.
- Activar U5 en Wire1 (asumiendo módulo soldado).
- Confirmar visualmente con Enzo qué slot físico está soldado en U5.
- Firmware: extender `sensors_tof.cpp` para manejar 2 instancias (1 por
  bus). Lib Adafruit_VL53L7CX ya lo soporta out of the box.

**Pros.**
- **Cero rework hardware.** No tocamos pads delicados del módulo Pololu.
- Riesgo de rotura del módulo: cero.
- Tiempo firmware: ~1-2 horas (extender lo que ya funciona).
- Encaja con la estrategia coach Incheon = "robot honesto, aprender, no
  podio" (CLAUDE.md).

**Contras.**
- Perdemos cobertura angular ToF (vs 4 sensores planeados).
- 2 slots quedan vacíos físicamente.

**Tiempo total estimado:** 2-3 horas (firmware + validación banco).

---

### Opción B — 4 ToFs con bodge

**Qué hay que hacer.**
- Identificar 4 GPIOs libres del Teensy 4.0 (candidatos: pines no
  usados por UARTs ni I²C; revisar `config_top.h` y schematic).
- Enzo soldar 4 jumpers desde los pads Xshut de U2/U3/U5/U17 hasta los
  GPIOs elegidos. Pads del módulo Pololu son ~0.5 mm — frágiles.
- Firmware: implementar la enumeración estándar VL53L7CX (encender uno
  por bus, cambiar address con `setAddress(0x52)`, prender el siguiente).
- Validar en banco que los 4 sensores responden con sus 4 addresses
  custom.

**Pros.**
- Cobertura ToF completa (4 cuadrantes / lo que originalmente se planeó).
- Capitaliza el hardware ya soldado.

**Contras.**
- **Rework delicado** sobre pads Pololu chicos → riesgo real de romper
  módulos (cada VL53L7CX cuesta ~USD 25, 4 = USD 100).
- Si un jumper se zafa en torneo (vibración, choque), perdemos ese ToF
  hasta poder resolderar (que no podemos en cancha).
- Tiempo Enzo: 2-4 horas + tiempo de validación. Enzo está cargado con
  TASK-001/002/006/011/013 P0 — agregar esto le come tiempo de las P0.
- Tiempo firmware: 4-6 horas (enumeración + manejo de 4 instancias +
  testing).
- **Si rompemos un módulo en el bodge, perdemos un ToF para Incheon.**

**Tiempo total estimado:** 8-12 horas (Enzo + firmware + validación
banco + recuperación si algo falla).

---

### Opción C — 0 ToFs (deferir todo)

**Qué hay que hacer.**
- Dejar el firmware ToF como stub (`TOF_NO_READING` para los 4 slots).
- Recuperar U2 funcionando para validar lo aprendido pero no integrar al
  WorldSnapshot.

**Pros.**
- Cero distracción del foco principal (cámaras + IMU + line ring).

**Contras.**
- Desperdiciamos las 3 horas del 2026-05-24 que invertimos en levantar
  U2 con Adafruit.
- ToF era parte del valor diferencial del diseño TOP.

**No recomendado** salvo que A también nos coma tiempo de subsistemas
P0.

---

### Opción D — 2 ToFs sin rework + bodge SOLO si sobra tiempo

Variante hibrida: empezar con Opción A. Si para el 2026-06-15 (15 días
antes de viajar) todo lo P0 está cerrado y queda margen, evaluar Opción
B como upgrade. **Decisión real: A primero, B opcional.**

---

## Recomendación del coach

**Opción B (2 ToFs sin rework).**

Justificación:
1. **Strategy fit con Incheon = aprendizaje, no podio** (CLAUDE.md). 2
   ToFs jugando bien > 4 ToFs riesgosos.
2. **Cero riesgo hardware** sobre módulos ya probados.
3. **Tiempo del equipo** (Enzo en particular) saturado en P0 pendientes
   — no agregar carga si no es bloqueante.
4. **Aprendizaje capitalizable**: los 2 ToFs validados nos dan baseline
   firmware sólida que reusamos en rev 1.1 con 4 ToFs ruteados (donde
   el bodge ya no será necesario).

Si la decisión es A, abrir TASK separada para enumeración bodge como
opcional / post-confirmación de los P0 (Opción D).

## Pasos concretos

1. Gustavo lee este TASK + el journal forense + el wishlist rev 1.1.
2. Gustavo decide A / B / D (no recomiendo C salvo que aparezca conflicto
   nuevo).
3. Gustavo escribe **journal entry corto** con:
   - Cuál opción eligió.
   - Por qué (1 párrafo).
   - Plan de implementación (qué pasos siguen, quién, cuándo).
4. Si A: crear sub-task para "extender `sensors_tof.cpp` a 2 instancias"
   con asignado (Virginia o Claude session siguiente).
5. Si B: crear sub-task hardware para Enzo (bodge Xshut) + sub-task
   firmware (enumeración).
6. Marcar este TASK como `done` con link al journal de la decisión.

## Criterio de cierre

- [ ] Decisión escrita en journal `journal/2026-05-2X-decision-tofs-incheon.md`.
- [ ] Sub-task(s) de implementación creada(s) con asignado.
- [ ] `docs/ESTADO-ACTUAL.md` actualizado con la decisión.
- [ ] Esta TASK marcada como `done` en este archivo + en la tabla del
      `team-tasks/README.md`.

## Plazo

**2026-05-29** (5 días) — antes de que sea tarde para reorganizar el
firmware si se elige A. Si se elige B, el plazo de Enzo es separado
(probablemente ~1 semana para el bodge + validación).

## Referencias

- Journal forense del hallazgo XSHUT: `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`
- Journal hardware-up U2: `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`
- Wishlist rev 1.1 (decisión de fondo, post-Incheon): `research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md`
- Firmware actual: `software/teensy/Soccer 2026/src/top/sensors_tof.cpp`
- Config: `software/teensy/Soccer 2026/src/top/config_top.h` (con banner aclaratorio agregado en esta sesión)
- TASK acoplada (arquitectura algorítmica): `team-tasks/2026-05-25-task-034-decidir-arquitectura-localizacion-incheon.md`

## Cambios de estado

- 2026-05-25: creada por Claude Opus 4.7 (Anthropic) al cerrar el
  hallazgo forense XSHUT. Asignada a Gustavo (decisión de coach +
  budget de tiempo del equipo).
