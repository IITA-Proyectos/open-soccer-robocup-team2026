---
id: TASK-030
title: "Sacar lámina protectora de los lentes de los 2 OTOS"
date_created: 2026-05-24
date_completed: 2026-05-24
assigned: [gviollaz]
priority: P2
status: completed
estimated_hours: 0.1
blocks: []
tags: [hardware, down-board, otos, mantenimiento, fisico]
---

> **Cerrada 2026-05-24** — Gustavo sacó las 2 láminas protectoras de los
> OTOS U5 y U6 en la misma sesión, sin esperar tapa de protección final.
> Test cuantitativo post-lámina (TASK-029) PASA con 6.5% de error sobre
> 300 mm. Las láminas eran efectivamente la causa principal del tracking
> errático observado pre-corrección.
>
> Cuidado adicional pendiente: cuando esté la tapa de protección, montarla
> para evitar daño físico a los lentes durante manipuleo.

# TASK-030 — Sacar lámina protectora de los OTOS

## Resumen

Los 2 módulos SparkFun Qwiic OTOS (U5 + U6) de la placa DOWN vienen
con una **lámina protectora** sobre el lente óptico (film transparente
que protege el lente durante el envío y manipulación). **Esa lámina
sigue puesta**.

Sin sacarla, el OTOS no enfoca el piso correctamente — el tracking
óptico es degradado o errático. Esto puede ser una de las causas (junto
con la superficie de prueba) del tracking pobre observado en el test del
2026-05-24 sobre hoja A4 (28 mm reportados vs 300 mm reales).

## Por qué está postergada por diseño

**Decisión del usuario 2026-05-24:** sacar la lámina sin tener una tapa
de protección sobre el robot expone el lente del OTOS a:
- Golpes durante el manipuleo en banco.
- Polvo y partículas que pueden rayar el lente.
- Caídas accidentales del robot al moverlo entre superficies.

La lámina es **un envoltorio temporal de fábrica, no parte del
funcionamiento**. Se saca **una sola vez** justo antes de que el robot
empiece a operar de forma protegida con su tapa o chasis cerrado.

## Lo que hay que hacer (cuando esté la tapa)

1. Confirmar que la tapa/chasis de protección del robot está lista y
   se puede montar sobre el plato base (placa DOWN).
2. Con la placa **desenergizada** (batería + USB desconectados):
   - Desmontar momentáneamente la placa DOWN del chasis si es necesario
     acceder a los OTOS por debajo.
   - Identificar visualmente los 2 OTOS (U5 a la izquierda, U6 a la
     derecha — ver `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`
     §6 para orientación).
   - Despegar con cuidado la lámina protectora de cada OTOS (lente
     transparente). Usar uñas o pinzas finas, NO objetos punzantes
     metálicos (raya el lente).
   - Limpiar con paño microfibra seco si quedó adhesivo.
3. Re-montar la placa + tapa de protección sobre el chasis.
4. Power up (con power cycle completo — TASK-028) y re-correr TASK-029
   (validación cuantitativa OTOS).

## Criterio de cierre

- [ ] Tapa de protección lista y montada.
- [ ] Las 2 láminas removidas sin daño al lente.
- [ ] Test de TASK-029 re-ejecutado con resultados dentro de tolerancia.
- [ ] Anotación en journal nuevo confirmando mejora del tracking.

## Cambios de estado

- 2026-05-24: creada al cierre de la sesión de hardware-up de DOWN.
  Usuario decide postergar conscientemente. Documentada acá para que la
  próxima sesión no se "olvide" de esta variable confound en TASK-029.
