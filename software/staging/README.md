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

## Prioridad 1 — Tests de sensores y primitivas (PROBAR PRIMERO)

### [TEST 1] BNO055 en modo IMUPLUS — test standalone
- **Archivo**: `shared/test-bno055-imuplus.ino`
- **Qué es**: Programa que SOLO testea el giróscopo. No mueve motores.
- **Cómo probar**:
  1. Subir a cualquiera de los robots, Serial Monitor 19200 baud
  2. **NO MOVER** 5 segundos (calibración)
  3. Girar 90° derecha → ~-90° | Girar 90° izquierda → ~+90°
  4. **Test drift**: 5 min quieto, anotar desviación
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar)_

### [TEST 2] Movimiento omnidireccional con PD heading
- **Archivo**: `shared/test-movimiento-omnidireccional.ino`
- **Referencia**: `docs/internal/cinematica-omnidireccional-movimientos.md`
- **Qué es**: Test interactivo de la función `moverRobot(velocidad, dirección, headingObjetivo)` que combina traslación omnidireccional + control PD de heading con giróscopo. 9 tests secuenciales controlados por botón.
- **Qué prueba** (3 seg cada test, avanzar con botón 1):
  1. Quieto mirando al frente (heading 0) → no debe moverse
  2. Avanzar recto mirando al frente → debe ir DERECHO (PD corrige)
  3. Retroceder mirando al frente → atrás sin girar
  4. Lateral DERECHA mirando al frente → cangrejo a la derecha
  5. Lateral IZQUIERDA mirando al frente → cangrejo a la izquierda
  6. Diagonal adelante-derecha → 45° sin girar
  7. Solo girar a heading 90° → gira y para en 90°
  8. Solo girar a heading -90° → gira al otro lado
  9. Orbitar (lateral + heading fijo) → simula orbitar pelota
- **Qué calibrar si no funciona bien**:
  - `Kp_heading` (2.0): subir si corrige lento, bajar si oscila
  - `Kd_heading` (0.05): subir si oscila mucho
  - `L_ROTACION` (0.6): bajar si gira demasiado y no traslada
  - `SIGNO_M1/M2/M3`: cambiar a -1 si un motor va al revés
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar, anotar valores calibrados)_

---

## Prioridad 2 — Fixes combinados en programas existentes

### [DELANTERO] Init BNO055 + fix UART + fix rampa de pateo
- **Cambios a aplicar** (en orden):
  1. `shared/cambios-bno055-init.md` — Cambio 1 + Cambio 3
  2. `shared/cambios-uart-sincronizacion.md` — Cambio 1
  3. `shared/cambios-rampa-pateo.md` — Reset de velocidadActualPateo
- **Cómo probar**:
  1. Aplicar a COPIA del definitivo-delantero, subir al robot
  2. Verificar calibración BNO055 por Serial Monitor
  3. Test giróscopo: encender apuntando a distintas direcciones
  4. Test UART: detección estable de pelota (LED no parpadea)
  5. Test pateo: 3 patadas seguidas, verificar rampa en cada una
  6. Test completo: jugar 2-3 minutos
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar)_

### [ARQUERO] Init BNO055 + fix currentYaw + fix UART
- **Cambios a aplicar** (en orden):
  1. `shared/cambios-bno055-init.md` — Cambio 1 + Cambio 2 + Cambio 3
  2. `shared/cambios-uart-sincronizacion.md` — Cambio 1
- **Cómo probar**:
  1. Aplicar a COPIA del definitivo-arquero, subir al robot
  2. Encender apuntando a DISTINTA dirección que el norte
  3. Test centrando/orbitado y decisiones de pateo
  4. Test UART: detección estable
  5. Test completo: oscilar 2-3 minutos
- **Resultado**: ⬜ Pendiente
- **Observaciones**: _(completar)_

---

## Prioridad 3 — OpenMV

### [VISION] Clampeo de coordenadas + quitar print()
- **Cambios**: `shared/cambios-uart-sincronizacion.md` — Cambio 2 + Cambio 3
- **Cómo probar**: Subir a OpenMV, verificar LEDs, FPS, detección estable con robot
- **Resultado**: ⬜ Pendiente

---

## Prioridad 4 — Evaluaciones opcionales

### [EVALUACIÓN] Librería BohleBots_BNO055
- **Documento**: `shared/evaluar-bohlebots-bno055.md`
- **Recomendación**: Solo si sobra tiempo.
- **Resultado**: ⬜ Pendiente

---

## Orden de prueba (viernes 28/3)

```
1°  Test BNO055 standalone        (10 min)  → ¿funciona IMUPLUS?
2°  Test movimiento omnidirec.    (20 min)  → calibrar Kp/Kd/L/signos
3°  Delantero con 3 fixes         (20 min)  → gyro + UART + rampa
4°  OpenMV con clampeo            (10 min)  → coordenadas seguras
5°  Arquero con 2 fixes           (20 min)  → gyro + currentYaw + UART
6°  (Opcional) BohleBots          (30 min)  → librería alternativa
```

**Regla**: Si TEST 1 falla → resolver antes de todo.
**Regla**: Si TEST 2 oscila mucho → ajustar Kp/Kd antes de seguir.
**Regla**: Los valores calibrados en TEST 2 se usan después en los fixes.

---

## Documentos de referencia

| Documento | Descripción |
|-----------|-------------|
| `shared/test-bno055-imuplus.ino` | Test standalone BNO055 |
| `shared/test-movimiento-omnidireccional.ino` | Test 9 movimientos con PD heading |
| `shared/cambios-bno055-init.md` | Parches init giróscopo |
| `shared/cambios-uart-sincronizacion.md` | Parches protocolo UART |
| `shared/cambios-rampa-pateo.md` | Parches rampa de pateo |
| `shared/evaluar-bohlebots-bno055.md` | Evaluación librería BohleBots |
| `docs/internal/cinematica-omnidireccional-movimientos.md` | Modelo cinemático completo |
| `docs/internal/giroscopo-bno055-analisis-tecnico.md` | Análisis BNO055 |
| `docs/internal/analisis-definitivo-delantero.md` | Análisis delantero |
| `docs/internal/analisis-definitivo-arquero.md` | Análisis arquero |

---

## Resultados de sesiones anteriores

| Fecha | Link | Items probados | Promovidos |
|-------|------|---------------|------------|
| _(ninguna aún)_ | | | |
