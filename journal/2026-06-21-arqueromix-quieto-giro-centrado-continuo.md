---
title: "ARQUEROMIX quieto — giro de centrado post-patada: PULSADO → CONTINUO lento"
date: 2026-06-21
status: vivo
placa: CENTRAL (ROBOT2, arquero)
env: central_robot2_arqueromix_quieto
autor: "Claude Opus 4.8 (1M context), vía Claude Code — pedido Virginia"
testeado-en-hardware: NO (compila; banco lo cierra el equipo)
---

# Giro de `orientar_frente` (post-patada): de pulsado a continuo y lento

## Qué pidió Virginia

> "El giro para buscar el frente del arco contrario son como pulsos, tiene que ser
> continuo y lento, no como pulsos y rápido. Modificá ese giro luego de patear."

Es el estado `orientar_frente` del MODO QUIETO (`-DARQMIX_QUIETO`): tras patear, el
robot gira para CENTRARSE con el arco rival (`goal_opp ≈ 0`) antes de retroceder a su
arco.

## Qué se cambió (cambio mínimo, 1 cosa)

`src/arqueromix/amix_fsm.cpp` — estado `orientar_frente`, rama del giro:
- **ANTES:** giro PULSADO (PFM). Cada ventana de `AMIX_T_GIRO_VENTANA`=350 ms giraba
  `AMIX_T_GIRO_ON`=90 ms a PWM 90 y quedaba QUIETO los otros 260 ms (leía el arco entre
  pulsos). Duty ≈ 26% → PWM efectivo ≈ 23.
- **AHORA:** giro CONTINUO a PWM constante (`AMIX_GIRO_FRENTE_PWM`) hacia el arco rival,
  sin ventana ON/OFF. Mismo sentido y mismo corte por `alineado_al_arco_opp()` + safety.

`src/arqueromix/amix_config.h`:
- `AMIX_GIRO_FRENTE_PWM`: **90 → 70** (PWM del giro continuo, arranque = piso nominal de
  las delanteras = lo más lento siendo continuo). Knob de banco.
- `AMIX_T_GIRO_VENTANA` / `AMIX_T_GIRO_ON`: marcadas **[sin uso]** (ya no se pulsa; se
  conservan por si se decide volver al PFM).

## Trade-off físico que se marcó ANTES de hacerlo (skill control-pid-zona-muerta)

El giro se había hecho PULSADO **a propósito**: es la técnica PFM para conseguir una
velocidad efectiva por debajo del piso del motor (a 23 PWM efectivo no hay continuo
posible — el motor se traba). Por eso:

- Continuo **saca los tirones** (lo que molestaba), pero **NO puede ser tan lento como el
  pulsado**. El continuo más lento posible es el PISO del motor (~70).
- Las tres juntas — continuo + suave + tan-lento-como-el-pulsado — **no se pueden** con
  este actuador. Se entregó: continuo + suave + lo-más-lento-posible.
- OJO: a 70 la TRASERA (piso ~107 en strafe) puede no participar → giro algo asimétrico
  (rota + traslada un poco), igual que el pulso de 90 anterior.

## Plan de banco (lo cierra el equipo — Claude NO cierra TASKs de hardware)

Env: `central_robot2_arqueromix_quieto`. Mirar SOLO el giro post-patada (estado
`orientar_frente`):

1. **Provocar un despeje** (pelota cerca y al frente) para que entre a la secuencia
   patada → pausa → `orientar_frente`.
2. Observar el giro de centrado:
   - ¿Sale a TIRONES / "parece pulsos otra vez"? → está por debajo del piso (stick-slip)
     → SUBIR `AMIX_GIRO_FRENTE_PWM` de a 5 (70→75→80…) hasta que sea parejo.
   - ¿Gira suave pero MUY rápido? → ya está cerca del piso; el continuo no da más lento.
     Avisar → se evalúa volver al pulsado o agregar arranque con rampa.
   - ¿Gira para el lado CONTRARIO al arco? → `-DARQMIX_FLIP_GIRO_ALINEAR` (invierte el
     giro de alineación Y el de centrado).
3. Criterio: el robot termina apuntando al arco rival (`|goal_opp| ≤ 12°`) sin tirones
   antes del retroceso.

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS** (FLASH code 17288, data 4040).
- Patrulla (`central_robot2_arqueromix`) NO se tocó en lógica; el cambio está en el bloque
  que solo corre en modo quieto.
- NO testeado en hardware (regla #1).

## Comando de flasheo

```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"; pio run -e central_robot2_arqueromix_quieto -t upload
```
