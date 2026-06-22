---
title: "ARQUEROMIX quieto — rediseño post-patada por GIROSCOPIO (Fase 1: orientar)"
date: 2026-06-21
status: vivo
placa: CENTRAL (ROBOT2, arquero)
env: central_robot2_arqueromix_quieto
autor: "Claude Opus 4.8 (1M context), vía Claude Code — pedido Virginia"
testeado-en-hardware: NO (compila; banco lo cierra el equipo)
analisis: "workflow paralelo 6 agentes (4 lectores + síntesis + crítica adversarial)"
---

# Rediseño post-patada del arquero quieto — Fase 1 (orientar) + Fase 2a (retroceso recto)

> **Actualización 2026-06-21 (Virginia):** el orientar (Fase 1) quedó andando en banco (mira al
> oponente tras corregir el sentido). Se avanzó con **Fase 2a**: el retroceso del modo quieto
> deja de usar la corrección por cámara (`retroceder_rumbo_opp`, que lo desviaba → se salía de
> la cancha / se metía al área) y va **RECTO** (`patear_atras`), parando en la primera línea
> blanca. Cambio mínimo en `PATEANDO_atras`. La red de re-orientación (Fase 2b) queda para
> después, solo si el recto curva en banco. `retroceder_rumbo_opp` quedó sin uso.

## Qué reportó Virginia (banco)

El giro de centrado post-patada (`orientar_frente`) **no corrige bien**: poca ganancia / zona
muerta. Pidió: tras patear, quedar **mirando al arco del oponente con el GIROSCOPIO (no la
cámara)** y recién ahí retroceder manteniendo el frente (control parecido al strafe lateral
que funciona); idealmente además **centrarse lateral** frente al arco. **Movimientos correctos
primero, velocidad después.**

## Cómo se analizó

Workflow paralelo (6 agentes, verificado contra código real): 4 lectores (delantero
centralmix · strafe del arquero que funciona · GK real `strategy.cpp` · mapeo de motores) +
síntesis + **crítica adversarial**. La crítica tumbó 2 premisas P0 de la primera síntesis (ver
abajo) — por eso se diseñó en fases y la Fase 1 quedó como bang-bang, no PI.

## Diagnóstico (verificado en código)

1. `orientar_frente` y `retroceder_rumbo_opp` usan `goal_opp_angle` (CÁMARA, ~4 Hz, smearea al
   moverse). El strafe que anda usa `heading_error_deg` (GIROSCOPIO), estable y ya cableado.
2. `AMIX_ROT_MAX=30` deja la trasera bajo su piso (107) y, cerca del objetivo, el proporcional
   pide PWM < piso → zona muerta. Es el síntoma reportado.

## Qué se cambió (Fase 1 — SOLO orientar_frente)

`src/arqueromix/amix_fsm.cpp` — `orientar_frente`:
- **ANTES:** giro continuo mirando `goal_opp_angle` (cámara).
- **AHORA:** **bang-bang por giroscopio**. Si `|heading_error_deg| > AMIX_TOL_ORIENTAR_DEG` →
  `girar()` al piso (`AMIX_GIRO_FRENTE_PWM=50`) en el sentido que lleva `heading_error → 0`;
  dentro de la banda → `parar()` → `PATEANDO_atras`. Gateado por `heading_valid` (sin heading
  → fallback recto, no gira a ciegas).

`src/arqueromix/amix_config.h`:
- **NUEVA** `AMIX_TOL_ORIENTAR_DEG = 8.0f` (banda muerta de orientación; ancha a propósito).
- `AMIX_GIRO_FRENTE_PWM` queda en 50 (ahora es el PWM del bang-bang); comentario actualizado.
- `amix_fsm.h`: comentario del estado actualizado a "por giroscopio".

## Por qué bang-bang y banda ancha (no PID fino) — crítica adversarial

- **P0-2:** un proporcional cerca del cero pide PWM < piso → no gira → serpentea (el síntoma,
  ahora alrededor del cero). Bang-bang "gira al piso o para" lo esquiva. Lo mismo que hace el
  delantero para apuntar a la pelota (PWM fijo + tolerancia 15°), que anda.
- **Banda 8°:** a 4-6 Hz girando al piso el robot rota varios grados/ciclo; ±2° → overshoot.

## Qué quedó para después (decisión Virginia)

- **Fase 2 (retroceso recto + re-orientar por excepción):** pendiente. La crítica adversarial
  (**P0-1**) mostró que NO hay que corregir el retroceso con la trasera (la mete en diagonal):
  retroceso = traslación pura, corrección = rotación pura (parar y girar). Por eso NO se portó
  el PI de trim-de-trasera del GK real (`gk_strafe_hold.h`) — es para strafe, no para retroceso.
- **Fase 3 (centrado lateral):** **POSPUESTA**. Depende del arco propio por cámara trasera, no
  validado y probablemente invisible cuando el arquero está pegado a su arco (**P1-4**). Abierta
  **TASK-224** para medirlo antes de construir.

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS**.
- Patrulla (`central_robot2_arqueromix`) NO tocada en lógica (el cambio es del modo quieto).
- NO testeado en hardware (regla #1).

## Iteración de banco 2026-06-21 (Virginia) — sentido invertido

- **Banco:** el giro funcionaba y se DETENÍA (lazo estable), pero el arquero quedaba **mirando
  a NUESTRO arco** (180° del oponente). Firma clásica de **sentido de giro invertido** en un
  bang-bang de heading: el lazo igual converge, pero al punto OPUESTO (skill
  control-pid-zona-muerta: "un signo invertido también queda estable, a 180° del objetivo").
- **Fix:** invertir el sentido **SOLO en `orientar_frente`** (`(heading_error>0)?-1:+1` →
  `?+1:-1`). El equilibrio del bang-bang pasa de 180° (nuestro arco) a 0° (oponente).
- **Por qué NO `-DARQMIX_FLIP_GIRO_ALINEAR`** (lo que se pensó primero): ese flag invierte
  `AMIX_GIRO_ALINEAR_SIGN`, usado en TRES giros — orientar, **alineación pre-patada**
  (`ALINEAR_arco_opp`, por cámara) y retroceso. Flipearlo arregla el orientar pero puede
  romper la alineación pre-patada (que puede estar bien). El orientar (giroscopio, ángulo del
  rumbo) y la alineación (cámara, ángulo al objetivo) tienen sentidos opuestos a propósito. Fix
  aislado > flag global.
- ⚠️ Si en banco se ve que ANTES de patear el robot apunta para el lado equivocado, o el
  retroceso gira mal, ENTONCES sí el `AMIX_GIRO_ALINEAR_SIGN` global está invertido para este
  robot → ahí sí flag global. Hoy no hay evidencia de eso.

## Iteración de banco 2026-06-21 (Virginia) — Fase 2a se metía al área

- **Banco:** el retroceso recto SÍ paraba en la línea, pero a `AMIX_ATRAS=120` se **metía en el
  área chica** (cruzaba la línea por inercia + latencia antes de frenar).
- **Fix 1 — velocidad separada y más lenta:** `AMIX_ATRAS_QUIETO=80` (nueva) + primitiva
  `retroceder_quieto()`. El retroceso del modo quieto va más lento → para más justo en la línea
  sin cruzarla. NO toca `AMIX_ATRAS` (la patrulla queda igual). Knob: si aún se mete → 75; si no
  arranca → subir.
- **Fix 2 — seguridad "nunca salirse" (pedido Virginia):** gate de **frescura del enlace DOWN**.
  El retroceso lee `linea()` cada tick (verificado: `amix_comm_tick` refresca `line_present`/
  `line_depth` ANTES del FSM cada loop). Además, si `down_link_fresh==false` (no llega línea hace
  >500 ms = enlace caído), el arquero **NO retrocede a ciegas**: frena y sale a `esperar_quieto`.
  Sin dato de línea confiable, mejor quieto que salirse.
- **PENDIENTE (alcance honesto):** "consciente EN TODO momento" NO está completo. Otros estados
  del modo quieto trasladan SIN chequear línea: `inicio_lateral_izq` (strafe izq 1.6 s a ciegas)
  y el strafe a la pelota en `esperar_quieto`. Si se quiere consciencia total de borde, ese es el
  próximo paso (aparte, para no arriesgar el strafe de juego que ya anda).

## Plan de banco (Fases 1 + 2a)

- **Orientar (Fase 1):** ¿queda mirando al **oponente** ±8° sin serpentear? Si serpentea → subir
  `AMIX_TOL_ORIENTAR_DEG`; si tironea → subir `AMIX_GIRO_FRENTE_PWM`.
- **Retroceso (Fase 2a):** ¿va derecho y **para en la línea SIN meterse al área**? Si se mete →
  bajar `AMIX_ATRAS_QUIETO` (80→75). Si curva → Fase 2b (re-orientación). Probar también: cortar
  el enlace DOWN a propósito y ver que NO retrocede a ciegas (frena).

## Comando de flasheo

```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"; pio run -e central_robot2_arqueromix_quieto -t upload
```

## Diseño completo
`docs/superpowers/specs/2026-06-21-arquero-quieto-orientar-giroscopio-design.md`
