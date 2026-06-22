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

# Rediseño post-patada del arquero quieto — Fase 1 (orientar por giroscopio)

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

## Plan de banco (Fase 1)

Mirar SOLO `orientar_frente` tras un despeje: (1) ¿queda mirando al **oponente** ±8° sin
serpentear? (2) si serpentea → subir `AMIX_TOL_ORIENTAR_DEG`. (3) si tironea → subir
`AMIX_GIRO_FRENTE_PWM`. (Sentido ya corregido en código; si AÚN gira al revés, avisar — sería
otro tema, no este mapeo.)

## Comando de flasheo

```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"; pio run -e central_robot2_arqueromix_quieto -t upload
```

## Diseño completo
`docs/superpowers/specs/2026-06-21-arquero-quieto-orientar-giroscopio-design.md`
