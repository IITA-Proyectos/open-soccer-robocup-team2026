---
id: TASK-044
title: "Medir el robot (Ø · alto · peso · rueda) y confirmar el reglamento de Roboliga 2026"
date_created: 2026-07-25
assigned: [gviollaz, "Elías Cordero"]
priority: P0
status: pending
estimated_hours: 4
tags: [hardware, mecanica, reglamento, mediciones, roboliga-2026]
blocks: [task-043, rediseno-chasis-v2, compra-motores]
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Claude Opus 5, Anthropic)"
---

# TASK-044 — Medir el robot y confirmar el reglamento de Roboliga 2026

## Resumen

Cerrar los datos físicos del robot (que **nunca se midieron**) y conseguir por escrito el
reglamento de **Roboliga 2026**. Sin esto, cualquier decisión de compra o de chasis se toma a ciegas.

## Contexto

- El repo **mide muy bien el control y muy mal la mecánica**: hay 858 tests y pisos de PWM titrados
  en banco, pero **no hay ni masa, ni diámetro, ni altura, ni diámetro de rueda medidos**.
  `hardware/mechanical/` contiene solo un README (cero CAD). Los gaps ya están declarados **P0**
  en el propio repo: `docs/competencia/MEJORAS-PENDIENTES.md` (A6 y A8) y `docs/competencia/TDP.md:621`.
- ⚠️ **Bandera roja de legalidad.** El reglamento RCJ 2026 renombró la liga a **Soccer Vision** y
  fijó **18,0 × 18,0 cm** (los 22 cm quedaron para Soccer Infrared). Dos artefactos del repo
  sugieren que el robot **podría no entrar**:
  - `src/central/config_central.h:151` → `WHEEL_RADIUS_MM = 100.0f` con el comentario
    *"distancia del centro a cada rueda"* → implicaría un círculo de ruedas de **Ø200 mm**.
    (Ojo: el nombre de la constante está **mal puesto** — no es el radio de la rueda, es el del
    robot — y está declarada TENTATIVA en 3 docs.)
  - El gerber del deck TOP mide ~**229,7 × 103,7 mm**.
  Ninguno de los dos reemplaza medir el robot armado **con calibre**.
- ❓ **El reglamento de Roboliga 2026 no está publicado.** El de 2025 decía tomar *"como base el
  reglamento oficial de RoboCupJunior Soccer"*, pero no está confirmado para 2026. Si Roboliga
  todavía corre con 22 cm, el problema de empaquetado se relaja mucho; si adopta los 18 cm, hay
  rediseño de chasis. **Cambia toda la planificación.**
- Además, el comité de RCJ anunció posibles cambios de tamaño para 2027/2028 → conviene conocerlos
  antes de congelar un chasis que apunta a Alemania.

## Pasos concretos

1. **Con calibre y balanza** (una tarde), medir y anotar en las Notas:
   - diámetro máximo del robot armado, altura total, **peso**;
   - **diámetro real de la rueda omni** y su ancho, y el modelo real (la BOM dice Nexus 38 mm
     "[a confirmar]"; otro doc dice 48 o 58 mm — **nadie lo confirmó**);
   - **radio real centro del robot → centro de rueda** (para arreglar `WHEEL_RADIUS_MM`);
   - modelo real del motor TT actual (la BOM lo deja "[a confirmar]").
2. **Con pinza amperimétrica**, medir el consumo de los motores en marcha y en bloqueo
   (no hay ni una medición de corriente en el repo).
3. **Medir el μ real de los rodillos** de la rueda sobre la alfombra de cancha (tirar del robot con
   un dinamómetro). Todo el argumento de "no hace falta reductora" (TASK-043) descansa en el límite
   de tracción: **hay que medirlo, no suponerlo**.
4. **Pedir por escrito a la organización de Roboliga**: qué documento y versión rigen en noviembre
   2026, y qué límites de **tamaño, peso y tensión** se van a usar en inspección.
5. Volcar todo a `docs/competencia/BOM.md` y al TDP, y cerrar los gaps A6/A8.

## Criterio de cierre

- [ ] Diámetro, altura y peso del robot medidos y anotados (con foto del calibre/balanza).
- [ ] Diámetro y modelo de la rueda omni confirmados.
- [ ] `WHEEL_RADIUS_MM` corregido en el firmware **y renombrado** a algo que diga lo que es
      (p. ej. `ROBOT_RADIUS_MM`), o documentado explícitamente si se deja como está.
- [ ] Corriente de motores medida (marcha y bloqueo).
- [ ] μ de los rodillos medido en la alfombra real.
- [ ] Respuesta **escrita** de Roboliga con reglamento, versión y límites de inspección.
- [ ] Veredicto explícito: **¿el robot actual entra en 18,0 cm?** Sí / No / No aplica (si Roboliga
      usa otro límite).

## Notas / decisiones

*(A completar por el equipo.)*

## Cambios de estado

- 2026-07-25 — creada (`pending`). Bloquea TASK-043 (compra de motores).
