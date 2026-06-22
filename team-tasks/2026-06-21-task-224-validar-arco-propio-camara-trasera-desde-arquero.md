---
task: TASK-224
fecha: 2026-06-21
asignado: equipo (banco con R2 + cámara trasera)
prioridad: P2
placa: TOP (cámara trasera) / contexto arquero CENTRAL
estado: abierta
bloquea: Fase 3 del rediseño post-patada del arquero quieto (centrado lateral)
solicitado-por: Virginia (vía análisis 2026-06-21)
---

# TASK-224 — ¿La cámara trasera ve el arco PROPIO de forma confiable desde la posición del arquero?

## Por qué existe

El rediseño del post-patada del arquero quieto (ver
`docs/superpowers/specs/2026-06-21-arquero-quieto-orientar-giroscopio-design.md`) quiere, en
su **Fase 3**, que el arquero se **centre lateralmente frente a su propio arco**. La única
señal para "estoy centrado frente a mi arco" es el **arco propio** (`goal_own_angle` /
`goal_own_visible`), que ve la **cámara trasera** del TOP.

**Problema:** esa señal NO está validada en banco (lo dice `amix_config.h` explícitamente), y
hay sospecha fuerte (crítica adversarial) de que **justo cuando el arquero está pegado a su
arco —donde termina el retroceso— la cámara trasera NO ve el arco propio** (queda demasiado
cerca / fuera de FOV). Si eso es así, construir el centrado sobre esta señal = código que se
saltea siempre.

**Antes de escribir la Fase 3 hay que medir esto.** Sin este dato, la Fase 3 queda en backlog.

## Qué medir (criterio de cierre)

Con R2 en cancha y el monitor (panel de arcos / telemetría TOP), colocar el arquero en las
posiciones donde realmente va a estar tras el retroceso y registrar:

1. **¿`goal_own_visible` = true** cuando el arquero está sobre la línea de su área, centrado
   frente al arco? (la posición donde termina el retroceso de la Fase 2).
2. **¿Y cuando está corrido a los costados** del arco (donde necesitaría centrarse)?
3. **¿`goal_own_angle` es estable y monótono** con la posición lateral? (ej.: corrido a la
   derecha → ángulo crece de un lado; a la izquierda → del otro). O sea: ¿sirve como señal de
   "a qué lado y cuánto me corrí"?
4. **¿El signo** de `goal_own_angle` vs el lado físico es el esperado? (define `AMIX_ARCO_OWN_SIGN`).

## Resultados posibles → qué hacemos

- **Ve el arco propio confiable y monótono** → construir Fase 3 con `rear_goal_dev` (centrado
  por arco propio), con el signo medido.
- **NO lo ve cuando está cerca del arco** (lo más sospechado) → NO usar arco propio; centrar
  por **otra señal**: la línea del área (DOWN) + simetría de tiempo (strafe N ms a cada lado),
  o dejar el centrado lateral fuera del alcance por ahora.

## Notas

- Esto es de VISIÓN (cámara trasera / arco propio), por eso TASK 2xx aunque el consumidor sea
  el arquero (CENTRAL).
- NO bloquea las Fases 1 y 2 del rediseño (esas usan el giroscopio, no el arco propio).
- Relacionada: la cámara trasera y su calibración LAB (`cameraBack-pack`, TASK-214 matrices).
