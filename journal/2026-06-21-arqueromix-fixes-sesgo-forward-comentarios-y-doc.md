---
title: "arqueromix — fixes de patrulla (sesgo forward) + auditoría de comentarios + doc actualizada"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco) + workflow paralelo (3 frentes)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco + doc
---

# arqueromix — fixes de patrulla + sesgo forward + comentarios + doc

## Pedido (Virginia, banco)

"Anda. Pero (1) todavía se pasa a veces a la IZQUIERDA; (2) a veces se queda PEGADO al rebotar; (3) a
veces se va muy ATRÁS y entra al área/corner izquierdo. Cuando toca la línea con la parte de atrás,
adelantarse un poco MÁS; tal vez más potencia en la rueda TRASERA para TENDENCIA a avanzar y no
retroceder. Documentá todo, guardá este programa como bueno, subilo a GitHub, revisá que los
comentarios sean correctos y que haya documentación clara de cómo funciona (actualizá o creá)."

## Workflow paralelo (3 frentes + síntesis)

1. **Diseño de motion** (confirmó la geometría omni-3: M1 del-IZQ@330°, M2 del-DER@210°, M3 trasera@90°;
   avanzar = M1+, M2−). Diagnosticó: sobrepaso izq = asimetría AI_REAR_ENEG; se pega = T_SALIR muy corto;
   deriva atrás = sin tendencia forward. **Corrigió un error**: el sesgo forward debe ir con signos FIJOS
   en ad/aiproporcional, NO "según el signo del PWM" (eso amplifica el strafe, no avanza).
2. **Auditoría de comentarios** (stale tras tantos cambios).
3. **Auditoría de doc** (el diagrama §5 decía 11 estados + impulso_inicial, que ya no existe).

## Cambios de MOTION (amix_config.h + 2 líneas en amix_motors.cpp)

| Constante | Cambio | Por qué |
|---|---|---|
| `AMIX_AI_REAR_ENEG` | 75→**65** | Sobrepaso IZQ (asimetría 1.875×→1.625× vs derecha=40) |
| `AMIX_T_SALIR_LINEA` | 350→**380** | Balance: no sobrepasar / no quedar pegado al rebotar |
| `AMIX_T_INICIO_AVANCE_MIN` | 400→**500** | Adelantarse más al tocar la línea de fondo |
| `AMIX_FORWARD_BIAS_PWM` | **NUEVO=10** | SESGO FORWARD: micro-empuje recto al frente (M1+=, M2-=) en ad/aiproporcional → tendencia a avanzar, no derivar atrás. Flag `-DARQMIX_NO_FORWARD_BIAS`. |

(El sesgo forward es la idea de Virginia "más potencia trasera para avanzar", implementada correctamente
según la geometría: avanzar es M1+/M2−, no la trasera sola.) Compila default + `-DARQMIX_NO_FORWARD_BIAS`.

## Comentarios corregidos (no mentir sobre lo que hace el código)

- `amix_fsm.h`: "10 estados" + "estado inicial impulso_inicial" → **12 estados**, inicial = inicio_retroceder.
- `amix_fsm.cpp`: golpe "M1=250,M2=150" → rampa; "~450 ms" del rebote → referencia a la constante; patear_atras → constante.
- `amix_motors.cpp`: impulso_inicial (marcado DEAD CODE + valores actuales 70/110); patear_atras (AMIX_ATRAS=120).
- Patrón: referir a la CONSTANTE por nombre en vez de hardcodear números (no vuelven a quedar stale).

## Documentación actualizada

- **`DOCUMENTACION.md` §5:** reescrito el diagrama de estados (12 estados, homing + arco/línea + profundidad
  + sesgo forward + despeje). Quitado `impulso_inicial`. "11 estados"→"12" en §1/§3/§4.
- **`DOCUMENTACION.md` §17.4 (nueva):** tuneo del movimiento de patrulla (lento/asimetría/rebote/forward).
- **`DOCUMENTACION.md` §16 + `FSM-ARQUERO-MIX-EXPLICADA.md`:** el safety de 50 s marcado como ⚠️ PENDIENTE
  (nunca se bajó; sigue en 50000 — TASK del equipo).
- **`FSM-ARQUERO-MIX-EXPLICADA.md`:** notas de conteo de estados actualizadas (el .h ya dice 12).
- Fuente de verdad: `amix_config.h` = los números; `FSM-ARQUERO-MIX-EXPLICADA.md` = mapa para modificar;
  `DOCUMENTACION.md` = histórico + decisiones de banco. Los .md referencian, no copian valores.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS** (default + `-DARQMIX_NO_FORWARD_BIAS`).
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco (regla #1). Todo cambio de motion = banco.
- Checkpoint previo: tag `arqueromix-bueno-2026-06-21-patrulla-lenta`.

## Plan de banco (Virginia)

1. ¿Dejó de pasarse a la IZQUIERDA? Si ahora se pasa a la DERECHA → subir `AMIX_AI_REAR_ENEG` 65→70.
2. ¿Sigue pegándose al rebotar? → `AMIX_T_SALIR_LINEA` 380→400. ¿Sobrepasa? → 360.
3. ¿Tiende a avanzar (no se mete atrás)? Si se va MUCHO para adelante / sale del arco → bajar
   `AMIX_FORWARD_BIAS_PWM` (10→6) o apagar `-DARQMIX_NO_FORWARD_BIAS`. Si todavía deriva atrás → subir (10→14).
4. ¿Se traba/espasmódica? → `AMIX_PD_BASE` muy bajo, subir 0.85→0.90.
5. Pelota: ¿la centra lejos y patea cerca? (esos estados ya existían, sin cambios).

## Archivos

- `amix_config.h` (4 constantes + AMIX_FORWARD_BIAS_PWM), `amix_motors.cpp` (sesgo forward en ad/aiproporcional + comentarios), `amix_fsm.cpp`/`amix_fsm.h` (comentarios), `DOCUMENTACION.md` (§5/§16/§17.4 + conteos), `FSM-ARQUERO-MIX-EXPLICADA.md` (notas).
