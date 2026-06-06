---
id: TASK-305
title: "App PC (GUI) de monitoreo y calibración de la BASE — 32 sensores de luz + línea"
date_created: 2026-06-06
assigned: [mariaviollaz, gviollaz]
priority: P0
status: pending
estimated_hours: 14
blocked_by: [TASK-304]
tags: [tooling, pc-app, down, telemetria, calibracion, prioritaria]
---

# TASK-305 — App PC de monitoreo + calibración de la BASE

## 1. Resumen
Aplicación de PC con **interfaz gráfica SIMPLE e intuitiva** que lee el stream USB del modo debug de DOWN (TASK-304) y muestra en vivo: el **anillo de 32 sensores de luz**, **qué líneas ve**, **la interpretación que DOWN envía a CENTRAL** (LineStatusV2), y permite **calibrar** los valores de luz de forma asistida.

## 2. Contexto
Es la mitad "host" del desarrollo prioritario (la otra mitad es TASK-304, el firmware). Objetivo: poner el robot en la cancha, conectarlo por USB a la Teensy de la base, **moverlo sobre las líneas** y diagnosticar/calibrar de un vistazo, sin leer texto crudo. Diseño: `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md`.

## 3. Pasos concretos
1. **Stack:** Python (pyserial + GUI: PyQt / Dear PyGui / Tkinter, o web local). Cross-platform, offline. Vive en `tools/monitor-base/` del repo.
2. **Lectura del stream** de USB (protocolo v1 de TASK-304) + parser robusto + **log a archivo** (para análisis posterior, estilo telemetría).
3. **Vista principal:**
   - **Anillo de 32 sensores** en su geometría real (LUT del PCB DOWN): color por valor crudo, resaltado si ve blanco.
   - **Línea detectada**: flecha de ángulo + profundidad + flag de salida inminente.
   - **Interpretación a CENTRAL** (LineStatusV2) en claro: cross_track (mm), penetration, line_present, quality.
4. **Diagnóstico de sensores**: marcar en rojo los sensores **muertos/pegados** (sin rango al pasar la línea). Indicador "X/32 OK".
5. **Calibración asistida**: botones carpet / blanco / **auto-calib** (capturar min/máx por sensor mientras se pasa el robot) + barra por sensor + **guardar a EEPROM** (envía el comando a DOWN). Guía en pantalla.
6. **Desarrollo sin robot**: soportar un archivo de telemetría grabado/simulado para iterar la GUI sin hardware.
7. README de uso (instalar deps, elegir puerto, flujo de calibración).

## 4. Criterio de cierre
- [ ] La app levanta, detecta el puerto USB y muestra el stream a tasa estable.
- [ ] Anillo de 32 sensores + línea + LineStatusV2 visibles y claros.
- [ ] Detecta y marca sensores muertos/pegados; muestra "X/32 OK".
- [ ] Flujo de calibración asistida funciona y guarda a EEPROM (vía comando a DOWN).
- [ ] Funciona en modo "replay" con un stream grabado (sin robot).
- [ ] README + corre cross-platform (al menos Windows del equipo).

## 5. Notas / decisiones
- (vacío — completar al ejecutar)

## 6. Cambios de estado
- 2026-06-06 — creada (pending). Bloqueada por TASK-304 (necesita el stream). Prioritaria.
