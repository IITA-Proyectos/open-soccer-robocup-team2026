---
title: "Evaluación: Librería BohleBots_BNO055 para RoboCup"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: staging
tags: [giroscopo, bno055, bohlebots, libreria, evaluacion]
---

# Evaluación: Librería BohleBots_BNO055

## Qué es

Librería Arduino creada por el equipo **BohleBots de Alemania** específicamente para RoboCup Junior. Reemplaza la librería Adafruit_BNO055 con funcionalidad optimizada para competencia.

- **Repo**: https://github.com/zischknall/BohleBots_BNO055
- **Arduino Library Manager**: Buscar "BohleBots_BNO055"
- **Compatibilidad**: Todas las arquitecturas Arduino

## Funcionalidades clave

| Función | Descripción | ¿La necesitamos? |
|---------|-------------|------------------|
| `startBNO(impact, forward)` | Inicia en NDOF con detección de impactos | ✅ Sí (pero queremos IMUPLUS) |
| `getHeadingAuto(address)` | Heading + recarga automática de offsets post-impacto | ✅ Sí, muy útil |
| `getRLHeadingAuto(address)` | Heading relativo a referencia | ✅ Exactamente lo que necesitamos |
| `setReference()` | Guarda orientación actual como referencia | ✅ Reemplaza nuestro `initialYaw` |
| `saveOffsets(address)` | Guarda calibración en EEPROM | ✅ Acelera arranques |
| `loadOffsets(address)` | Carga calibración desde EEPROM | ✅ Acelera arranques |
| `isCalibrated()` | Verifica calibración completa | ✅ Mejor que nuestro chequeo manual |

## Ventajas sobre Adafruit_BNO055

1. **Diseñada para robots de competencia** (no para proyectos generales)
2. **Detección de impactos**: Cuando el robot choca, recarga automáticamente los offsets de calibración desde EEPROM. Esto evita que un golpe corrompa la orientación.
3. **Más rápida**: Lee registros directamente sin la capa de abstracción de Adafruit.
4. **`setReference()` + `getRLHeading()`**: Heading relativo integrado, no hay que calcular offset a mano.

## Desventajas / Riesgos

1. **Usa modo NDOF** por defecto. Para nuestro caso necesitaríamos verificar si podemos configurarla en IMUPLUS, o si la detección de impactos compensa el problema del magnetómetro.
2. **Repo archivado** (no tiene mantenimiento activo). Funciona pero no habrá updates.
3. **Cambio grande**: Requiere reemplazar todas las llamadas a `bno.getEvent()` por `getHeading()` / `getRLHeading()`. Es un cambio en todo el código.

## Recomendación

**Fase 1 (viernes 28/3)**: Probar primero los cambios mínimos con Adafruit (IMUPLUS + init mejorado). Es menos riesgoso y resuelve el 80% del problema.

**Fase 2 (semana siguiente)**: Si IMUPLUS + Adafruit funciona bien, evaluar si BohleBots agrega valor suficiente para justificar el refactoring. El feature más valioso es la recarga automática post-impacto.

## Cómo probar (si se decide evaluar)

1. Instalar desde Arduino IDE: Sketch → Include Library → Manage Libraries → buscar "BohleBots"
2. Subir el ejemplo básico que viene con la librería
3. Verificar heading y calibración por Serial Monitor
4. Probar golpeando el robot y ver si recarga offsets
