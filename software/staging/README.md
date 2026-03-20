---
title: "Staging — Pendiente de prueba en robots"
date: 2026-03-20
status: active
---

# Staging — Pendiente de prueba en robots

> **¿Qué es esto?** Acá van los programas nuevos y mejoras listos para probar en el robot físico.
>
> **Flujo completo**: ver `docs/internal/flujo-de-trabajo-software.md`

Última actualización: 2026-03-20

---

## Próxima sesión de prueba: viernes 28 de marzo 2026

---

## Prioridad 1 — Test del sensor BNO055 (PROBAR PRIMERO)

### [TEST] BNO055 en modo IMUPLUS — test standalone
- **Archivo**: `shared/test-bno055-imuplus.ino`
- **Qué es**: Programa pequeño que SOLO testea el giróscopo. No mueve motores.
- **Qué cambió**: Usa `OPERATION_MODE_IMUPLUS` (sin magnetómetro) + init mejorado con espera de calibración + promedio de lecturas.
- **Cómo probar**:
  1. Subir a cualquiera de los dos robots
  2. Abrir Serial Monitor a 19200 baud
  3. **NO MOVER** el robot durante 5 segundos (calibración)
  4. Esperar mensaje "LISTO!"
  5. Girar el robot 90° a la derecha → debe mostrar ~-90°
  6. Girar 90° a la izquierda → debe mostrar ~+90°
  7. Volver al frente → debe volver a ~0°
  8. **Test de drift**: dejar 5 minutos quieto y anotar cuánto se desvía
  9. **Test con motores**: encender motores (otro programa) y ver si el heading salta
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Prioridad 2 — Programas con TODOS los fixes combinados

La idea es aplicar TODOS los fixes juntos en una copia del programa de cada robot. Son cambios independientes entre sí, así que si algo falla, se puede aislar cuál fue.

### [DELANTERO] Init BNO055 + fix UART + fix rampa de pateo
- **Cambios a aplicar** (en este orden):
  1. `shared/cambios-bno055-init.md` — Cambio 1 (IMUPLUS + init mejorado) + Cambio 3 (eliminar START_BYTE)
  2. `shared/cambios-uart-sincronizacion.md` — Cambio 1 (lectura UART robusta con while-peek)
  3. `shared/cambios-rampa-pateo.md` — Reset de velocidadActualPateo en transiciones de pateo
- **Qué cambió en resumen**:
  - Giróscopo en IMUPLUS (sin magnetómetro interferido por motores)
  - Init con espera de calibración + promedio de 10 lecturas
  - UART busca header descartando basura (no se desincroniza)
  - Valida los 3 headers (201+202+203) antes de decodificar
  - Rampa de pateo funciona en CADA patada (no solo la primera)
- **Cómo probar**:
  1. Aplicar los 3 documentos de cambios a una COPIA del definitivo-delantero
  2. Subir al robot delantero
  3. Serial Monitor: verificar mensajes de calibración BNO055
  4. **Test giróscopo**: encender apuntando a distintas direcciones, verificar que funciona
  5. **Test UART**: con pelota visible, verificar detección estable (LED no parpadea)
  6. **Test pateo**: hacer patear 3 veces seguidas, verificar que las 3 tienen rampa
  7. **Test completo**: dejar jugar 2-3 minutos y observar comportamiento general
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

### [ARQUERO] Init BNO055 + fix currentYaw + fix UART
- **Cambios a aplicar** (en este orden):
  1. `shared/cambios-bno055-init.md` — Cambio 1 + Cambio 2 (currentYaw→error) + Cambio 3
  2. `shared/cambios-uart-sincronizacion.md` — Cambio 1 (lectura UART robusta)
- **Qué cambió en resumen**:
  - Todo lo del giróscopo (IMUPLUS + init + calibración)
  - Fix currentYaw: todas las comparaciones usan `error` normalizado
  - UART robusto con while-peek
- **Cómo probar**:
  1. Aplicar los 2 documentos de cambios a una COPIA del definitivo-arquero
  2. Subir al robot arquero
  3. **Test orientación**: encender apuntando a DISTINTA dirección que el norte (varias)
  4. **Test centrando**: verificar que el orbitado y decisiones de pateo funcionan
  5. **Test UART**: detección estable de pelota y arcos
  6. **Test completo**: dejar oscilar 2-3 minutos y observar comportamiento
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Prioridad 3 — Cambios en OpenMV

### [VISION] Clampeo de coordenadas + quitar print()
- **Cambios**: Ver `shared/cambios-uart-sincronizacion.md` — Cambio 2 + Cambio 3
- **Qué cambió**:
  - Coordenadas clampeadas a 1-200 para que nunca coincidan con headers (201/202/203)
  - `print("Enviando:", packet)` comentado (reduce FPS innecesariamente)
- **Cómo probar**:
  1. Subir el programa modificado a la OpenMV
  2. Verificar que los LEDs de la OpenMV siguen indicando detección
  3. En el IDE de OpenMV: verificar que el FPS sube (debería ser más fluido)
  4. Probar en combinación con el robot: ¿detección estable?
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Prioridad 4 — Evaluación de librería alternativa (OPCIONAL)

### [EVALUACIÓN] Librería BohleBots_BNO055
- **Documento**: `shared/evaluar-bohlebots-bno055.md`
- **Qué es**: Librería de un equipo alemán de RoboCup Junior, optimizada para competencia.
- **Riesgo**: Alto (requiere cambiar muchas líneas de código)
- **Recomendación**: Solo evaluar si todo lo anterior funciona y sobra tiempo.
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Orden de prueba recomendado (viernes 28/3)

```
1°  Test standalone BNO055       (10 min)  → ¿funciona IMUPLUS?
2°  Delantero con todos los fixes (20 min)  → gyro + UART + rampa
3°  OpenMV con clampeo           (10 min)  → coordenadas seguras + FPS
4°  Arquero con todos los fixes   (20 min)  → gyro + currentYaw + UART
5°  (Opcional) BohleBots          (30 min)  → solo si sobra tiempo
```

**Regla**: Si el test standalone BNO055 falla, NO seguir con 2-4.
**Regla**: Si la detección parpadea, probar primero el fix de OpenMV (clampeo) antes de culpar al Teensy.

---

## Documentos de referencia

| Documento | Descripción |
|-----------|-------------|
| `shared/cambios-bno055-init.md` | Parches exactos para init del giróscopo |
| `shared/cambios-uart-sincronizacion.md` | Parches exactos para protocolo UART |
| `shared/cambios-rampa-pateo.md` | Parches exactos para fix de rampa |
| `shared/evaluar-bohlebots-bno055.md` | Evaluación de librería alternativa |
| `shared/test-bno055-imuplus.ino` | Programa de test standalone |
| `docs/internal/giroscopo-bno055-analisis-tecnico.md` | Análisis completo del BNO055 |
| `docs/internal/analisis-definitivo-delantero.md` | Análisis del programa del delantero |
| `docs/internal/analisis-definitivo-arquero.md` | Análisis del programa del arquero |

---

## Resultados de sesiones anteriores

| Fecha | Link | Items probados | Promovidos |
|-------|------|---------------|------------|
| _(ninguna aún)_ | | | |
