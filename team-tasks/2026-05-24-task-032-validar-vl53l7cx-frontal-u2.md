---
id: TASK-032
title: "Validar VL53L7CX frontal U2 en hardware real (placa TOP) — sketch diag_top_tof"
date_created: 2026-05-24
assigned: [a-asignar]
priority: P2
status: pending
estimated_hours: 0.5
blocks: [integracion-vl53l7cx-firmware-top]
blocked_by: [TASK-025 (excepción Avast PlatformIO)]
tags: [hardware, top-board, tof, vl53l7cx, diagnostico]
---

# TASK-032 — Validar VL53L7CX frontal U2 en hardware real (placa TOP)

## Resumen

Flashear el sketch de diagnóstico `diag_top_tof` en la Teensy 4.0 de la
placa TOP, abrir Serial Monitor, y verificar los 7 criterios de
aceptación del spec sobre el VL53L7CX frontal (U2 del schematic).

## Contexto

Antes de integrar el driver real del VL53L7CX en `src/top/sensors_tof.cpp`
hace falta confirmar que el sensor físico que se soldó en la placa TOP
responde correctamente vía I²C (bus Wire, I²C0 — pines SDA=18 / SCL=19),
carga firmware y entrega una
grilla de distancias coherente. El sketch `diag_top_tof` (env homónimo
en `platformio.ini`) es una herramienta de banco aislada del resto del
firmware de competencia para hacer exactamente esta validación sin
contaminar el código de producción.

Los TOFs son Nivel 3+ de la roadmap (no bloquean Incheon), por eso P2.
Pero validar el sensor que ya está comprado y soldado es barato y deja
el camino libre para integrarlo en cuanto los Niveles 1 y 2 estén
estables.

## Prerequisitos

1. Placa TOP montada, Teensy 4.0 alimentada, LED de power ON.
2. VL53L7CX soldado en posición U2 del schematic (frontal).
3. **TASK-025** cerrada o con excepción Avast aplicada en la máquina que
   vaya a flashear (sin esto PlatformIO no descarga toolchain por SSL MITM).

## Pasos concretos

1. Posicionarse en el proyecto firmware:
   ```bash
   cd "software/teensy/Soccer 2026"
   ```
2. Compilar y flashear el sketch de diagnóstico:
   ```bash
   pio run -e diag_top_tof -t upload
   ```
3. Abrir Serial Monitor:
   ```bash
   pio device monitor -b 115200
   ```
   (alternativa: botón Serial Monitor de la extensión PlatformIO en VSCode)
4. Recorrer los 7 escenarios del criterio de cierre (abajo). Anotar
   qué pasa en cada uno — capturar pantalla o copiar al journal.

## Criterio de cierre (los 7 del spec §4.3)

- [ ] (1) Banner `=== diag_top_tof ===` visible al arrancar.
- [ ] (2) Línea `loading FW... OK` (NO `init FAILED`).
- [ ] (3) Grilla nueva cada ~67 ms durante 30 s sin freeze.
- [ ] (4) Apuntar a pared a 1 m: zona central lee 950–1050 mm.
- [ ] (5) Mano pegada al sensor: zona central baja a 30–50 mm.
- [ ] (6) Cielo abierto (>4 m): valores ~4000 mm o status XXXX (OOR).
- [ ] (7) Mover obstáculo solo al cuadrante sup-izq a 50 cm: solo esas
      4 zonas (modo 4x4) bajan, las otras 12 quedan en lectura de fondo.
- [ ] Journal nuevo `journal/2026-05-2X-validacion-tof-frontal.md` con
      verdict de los 7 escenarios + capturas.

## Si algún criterio falla

1. **Si `init FAILED`:** recompilar con `-DDIAG_TOF_SKIP_XSHUT` agregado
   al `build_flags` del env `diag_top_tof` (descarta problema de pin XSHUT).
2. **Si I2C scan no muestra `0x29`:** problema físico (alimentación,
   soldadura, pull-ups). Avisar a Enzo, no insistir por software.
3. **Si la grilla aparece pero todas las zonas leen 0 o XXXX:** probar
   `Wire.setClock(100000)` (slow mode) en el sketch — el bus puede estar
   con ringing por trazas largas.
4. **Documentar TODOS los hallazgos** en
   `journal/2026-05-2X-validacion-tof-frontal.md` (incluido lo que NO
   funcionó — sirve para la próxima sesión).

## Qué NO hacer

- NO modificar `src/top/sensors_tof.cpp` con código del driver real
  hasta que este sketch pase los 7 criterios. El objetivo del sketch
  es justamente desacoplar la validación física del firmware de
  competencia.
- NO marcar este TASK como `done` sin que pasen los 7 criterios.
- NO bajar prioridad ni cerrar el TASK si los criterios 4-7 no se
  pueden ejecutar por falta de banco/superficies — re-asignar `blocked`
  con la razón.

## Referencias

- Spec: `docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md`
- Plan: `docs/superpowers/plans/2026-05-24-diag-top-tof-vl53l7cx.md`
- Pack TOP: `hardware/electronics/top-board-pack/`

## Cambios de estado

- 2026-05-24: creada al cierre del plan diag_top_tof (Tasks 1-4 ya
  commiteadas: vendored lib, env+skeleton, setup(), loop()+formatters).
  Asignación pendiente — necesita ojos humanos en la placa TOP.
