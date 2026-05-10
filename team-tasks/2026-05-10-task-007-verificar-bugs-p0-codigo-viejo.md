---
id: TASK-007
title: "Verificar si bugs P0 del código viejo se fixearon (Plan B insurance)"
date_created: 2026-05-10
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 4
blocks: [Plan B activation]
tags: [firmware, legacy, bugs, plan-b, verificacion]
---

# TASK-007 — Verificar fixes de bugs P0 del código viejo

## Resumen

Verificar si los **4 bugs P0** identificados en marzo 2026 (`docs/internal/analisis-definitivo-*.md`) **siguen presentes en el código viejo del nacional 2025** o si ya fueron fixeados. Esto es seguro contra el Plan B: si el firmware nuevo de las 3 placas no llega a tiempo para Incheon, retrocedemos al robot viejo — pero solo si los bugs P0 están corregidos.

## Contexto

**Bugs P0 del análisis de marzo 2026** (documentados, no necesariamente fixeados):

### Bug A — Arquero: gap mortal `3 < |Yp| < 5`
- Archivo: `software/robot-arquero/definitivo-arquero_6-9-2026`
- Síntoma: cuando la pelota está casi centrada pero no exactamente, el robot **se congela** (no patea ni se mueve).
- Detalle: `analisis-definitivo-arquero.md` BUG 1.
- Pasa porque hay un umbral para patear (`abs(Yp) <= 3`) y otro para moverse (`abs(Yp) >= 5`), y el rango entre 3 y 5 cae en `else { parar(); }`.

### Bug B — Arquero: `currentYaw` raw vs `error` normalizado
- Archivo: mismo arquero.
- Síntoma: el arquero solo funciona si se enciende apuntando al norte magnético. En la cancha real eso no se cumple.
- Detalle: `analisis-definitivo-arquero.md` BUG 4.
- Pasa porque hay comparaciones como `if (currentYaw <= 10 or currentYaw >= 350)` que solo dan true cerca del 0 absoluto, no del heading inicial.

### Bug C — Delantero: `velocidadActualPateo` no se resetea
- Archivo: `software/robot-delantero/definitivo-delantero.cpp`
- Síntoma: la rampa de aceleración del pateo solo funciona la primera vez. Las patadas siguientes arrancan a velocidad máxima inmediatamente.
- Detalle: `analisis-definitivo-delantero.md` BUG 2.

### Bug D — `PATEANDO_atras_arquero` sin timeout
- Archivo: arquero.
- Síntoma: si los sensores de línea fallan, el robot retrocede indefinidamente hasta salir de la cancha.
- Detalle: `analisis-definitivo-arquero.md` BUG 2.

## Pasos concretos

### 1. Verificar Bug A (gap arquero)

1. Abrir `software/robot-arquero/definitivo-arquero_6-9-2026` en VSCode.
2. Buscar el estado `moverce_derecha`.
3. Localizar el bloque que verifica `Yp` para decidir patear o moverse.
4. ¿Hay un branch que cubra el caso `3 < abs(Yp) < 5`?
   - Si sí (algo como `else if`): bug **fixeado**.
   - Si no (gap entre los dos `if`): bug **sigue presente**.

### 2. Verificar Bug B (currentYaw raw)

1. En el mismo archivo, buscar todas las apariciones de `currentYaw`.
2. ¿Se compara con valores absolutos como `<= 10` o `>= 350`?
   - Si sí: bug **sigue presente**.
   - Si las comparaciones son contra `error` (normalizado ±180): bug **fixeado**.

### 3. Verificar Bug C (velocidadActualPateo)

1. Abrir `software/robot-delantero/definitivo-delantero.cpp`.
2. Buscar `velocidadActualPateo`.
3. ¿Se resetea a 0 al entrar a algún estado de patada (ej. `PATEANDO_pausa_inicial`)?
   - Si sí: bug **fixeado**.
   - Si no (solo se incrementa, nunca se reinicia): bug **sigue presente**.

### 4. Verificar Bug D (PATEANDO_atras_arquero sin timeout)

1. En el arquero, buscar `case PATEANDO_atras_arquero:`.
2. ¿Hay un check de tiempo tipo `if (millis() - millis_inicio_estado >= XXXX)` que salga a otro estado?
   - Si sí: bug **fixeado**.
   - Si no, solo sale por sensores de línea: bug **sigue presente**.

### 5. Reportar resultados

1. Crear `research/in-progress/2026-MM-DD-verificacion-bugs-p0-codigo-viejo.md` con tabla:
   | Bug | Estado | Si sigue presente, plan de fix |
   |-----|--------|--------------------------------|
   | A | fixeado / presente | unificar umbrales `<= 5` |
   | B | fixeado / presente | reemplazar `currentYaw` por `error` |
   | C | fixeado / presente | resetear al entrar al estado |
   | D | fixeado / presente | timeout 3000ms |
2. **Si algún bug sigue presente:** crear tareas en `team-tasks/` para fixearlo con prioridad **P0 si el Plan A se complica** (firmware nuevo no llega a tiempo). Mientras Plan A avance bien, dejarlos como P2.

## Criterio de cierre

- [ ] Bug A verificado.
- [ ] Bug B verificado.
- [ ] Bug C verificado.
- [ ] Bug D verificado.
- [ ] Tabla de resultados en `research/in-progress/`.
- [ ] Si bugs siguen presentes: tasks creadas en `team-tasks/` (P0 condicional al estado del Plan A).

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
