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

## Prioridad 2 — Aplicar cambios al programa del delantero

### [DELANTERO] Init BNO055 mejorado (IMUPLUS + calibración)
- **Cambios**: Ver `shared/cambios-bno055-init.md` (Cambio 1 + Cambio 3)
- **Qué cambió**:
  - `bno.begin()` → `bno.begin(OPERATION_MODE_IMUPLUS)` (desactiva magnetómetro)
  - Timeout de 3 segundos si BNO055 no responde (en vez de bloquear)
  - Espera 1000ms de estabilización post-init
  - Espera calibración del giróscopo (robot quieto)
  - Promedia 10 lecturas para heading inicial (en vez de 1)
  - Elimina `#define START_BYTE 0xAA;` (no usado, tiene punto y coma extra)
- **Cómo probar**:
  1. Aplicar los cambios del documento a una COPIA del definitivo-delantero
  2. Subir al robot delantero
  3. Verificar por Serial Monitor que muestra calibración
  4. Probar comportamiento normal (buscar pelota, avanzar, patear)
  5. Comparar con programa original: ¿se desvía menos al avanzar recto?
  6. Probar encendido apuntando a distintas direcciones (no solo al norte)
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Prioridad 3 — Aplicar cambios al programa del arquero

### [ARQUERO] Init BNO055 mejorado + fix currentYaw
- **Cambios**: Ver `shared/cambios-bno055-init.md` (Cambio 1 + Cambio 2 + Cambio 3)
- **Qué cambió**: Todo lo del delantero MÁS:
  - Todas las comparaciones `currentYaw <= 10 or currentYaw >= 350` cambiadas a `abs(error) <= 10`
  - Todas las comparaciones `currentYaw <= 90 or currentYaw >= 270` cambiadas a `abs(error) <= 90`
  - Esto hace que las decisiones de centrando funcionen sin importar la orientación al encender
- **Cómo probar**:
  1. Aplicar todos los cambios a una COPIA del definitivo-arquero
  2. Subir al robot arquero
  3. Encender apuntando a DISTINTA dirección que el norte (probar varias)
  4. Verificar que el orbitado (centrando) y las decisiones de pateo funcionan
  5. Comparar con programa original: ¿el centrando funciona mejor cuando no apunta al norte?
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Prioridad 4 — Evaluación de librería alternativa (OPCIONAL)

### [EVALUACIÓN] Librería BohleBots_BNO055
- **Documento**: `shared/evaluar-bohlebots-bno055.md`
- **Qué es**: Librería de un equipo alemán de RoboCup Junior, optimizada para competencia. Tiene detección de impactos con recarga automática de calibración y guardado en EEPROM.
- **Riesgo**: Alto (requiere cambiar muchas líneas de código)
- **Recomendación**: Solo evaluar si los cambios de Prioridad 1-3 funcionan bien y hay tiempo.
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar después de probar)_

---

## Orden de prueba recomendado

```
1°  Test standalone BNO055 (10 min) → verifica que el sensor funciona en IMUPLUS
2°  Delantero con init mejorado (20 min) → verifica comportamiento completo  
3°  Arquero con init + fix currentYaw (20 min) → verifica centrando/pateo
4°  (Opcional) Evaluar BohleBots (30 min) → solo si sobra tiempo
```

**Regla**: Si el test standalone falla, NO seguir con prioridades 2-3. Resolver primero.

---

## Resultados de sesiones anteriores

| Fecha | Link | Items probados | Promovidos |
|-------|------|---------------|------------|
| _(ninguna aún)_ | | | |
