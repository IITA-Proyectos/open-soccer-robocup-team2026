---
task: 206
titulo: "TOP: config persistente de sensores en EEPROM (fail-safe) — deshabilitar cámara/BNO/ToF/zonas + orientación ToF"
fecha: 2026-06-13
asignado: equipo (firmware TOP — Claude programa host-testeable; banco lo cierra el equipo)
prioridad: P1
pedido-por: Gustavo Viollaz (2026-06-13)
relacionada: TASK-205 (monitor TOP), research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md
estado: in-progress — A1 + A2.1 HECHAS (firmware) 2026-06-14; falta A2.2 (zonas/orientación ToF) + banco
---

> **A1 HECHA (2026-06-14)** — "VER cómo andan los sensores": telemetría TOP v2 con detecciones POR
> CÁMARA (`camf`/`camb`) + OTOS/línea/**vector de escape** de la base (`base`/`line`).
> `journal/2026-06-14-top-telemetria-v2-percamara-y-base-A1.md`.
>
> **A2.1 HECHA (2026-06-14)** — config persistente fail-safe: deshabilitar **cámara F/B, BNO L/R,
> ultrasonido, ToF entero** + **ubicación/bearing por ToF**, persistido en EEPROM (offset 368,
> magic+CRC) + carga al boot (defaults no-op) + comandos `CAM/BNO/US/TOF/CFG` + línea `CFG` en el
> texto humano. Módulo puro `top_config` (12 tests) + glue `top_eeprom_config` + apply-points.
> Gate host verde + `pio run -e top_robot2_pri` SUCCESS. `journal/2026-06-14-top-config-persistente-eeprom-A2-1.md`,
> `docs/firmware/EEPROM-MAP.md`. **Falta:** banco (regresión A/B + persistencia) + **A2.2** (zonas/
> rotación/flip del ToF — requiere 64 zonas, medir loop 100 Hz) + bloque `cfg` JSON para la GUI.

# TASK-206 — Config persistente de sensores de la TOP (fail-safe P1)

> **Por qué (Gustavo, 2026-06-13):** poder **apagar un sensor que manda basura** en cancha (lo
> vimos hoy: pelota fantasma de cámara dual, velocidad ±13 m/s) y que esa decisión **persista en la
> EEPROM de la TOP**. Es la Fase A del "Monitor General" (la Fase B —grillas 8×8, mapas— es 2027).

## Alcance (Fase A)

Módulo puro `top_config` + EEPROM + comandos + apply, según el spec
[`research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md`](../research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md):

- Deshabilitar **cámara F/B**, **BNO L/R**, **ultrasonido**, **ToF entero**.
- **ToF por zonas**: anular zonas (ej. filas superiores), rotar 90°, invertir eje.
- **Persistir** todo en EEPROM de la TOP; cargar al boot (defaults = no-op = competencia byte-idéntica).
- Reportar el estado en telemetría (bloque `cfg`) + línea en el texto humano.

## Comandos (contrato con la GUI — el otro agente la construye a esto)

`CAM F|B ON|OFF` · `BNO L|R ON|OFF` · `US ON|OFF` · `TOF n ON|OFF` ·
**`TOF n POS FRONT|BACK|RIGHT|LEFT`** (ubicación; futuro `POS <deg>`) · `TOF n ROT 0|90|180|270` ·
`TOF n FLIP X|Y|NONE` · `TOF n ZONE ON|OFF <0..63>` · `TOF n ZONEMASK <hex>` · `CFG SAVE|LOAD|RESET`.

## Criterio de cierre (banco — el equipo)

- [ ] Módulo puro `top_config` con tests host en verde (serialize/deserialize/crc/apply_zone_mask).
- [ ] `pio run -e top_robot2_pri` SUCCESS con la config cableada.
- [ ] Boot sin config guardada = conducta de competencia idéntica (regresión).
- [ ] `CAM B OFF` + `CFG SAVE` + power-cycle → la trasera sigue deshabilitada (persistencia).
- [ ] `TOF n ZONE OFF` en filas superiores → ese ToF deja de "ver" el techo/estructura.
- [ ] `BNO R OFF` → heading degrada con gracia al otro BNO.

## Cierre

Lo cierra el equipo en banco (regla no negociable). Claude programa el firmware host-testeable + el
glue, pero NO marca como `done` el comportamiento en hardware.
