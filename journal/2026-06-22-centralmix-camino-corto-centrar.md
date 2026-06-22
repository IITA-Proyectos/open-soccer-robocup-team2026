# 2026-06-22 — centralmix: camino CORTO al centrar (elegir el sentido de la órbita)

## Qué se pidió (Elías)
Al centrar/orbitar la pelota para alinearla con el arco, el robot arrancaba **siempre para el mismo
sentido** y a veces le daba **la vuelta larga**. Que tome el **camino más corto**. Elías propuso dos
señales: (1) la X/ángulo del arco por cámara, (2) el heading (~0 = mirando al arco).

## Diseño (workflow multi-agente + red-team)
**Cascada visión-primaria + heading-respaldo** (espejo de cómo la FSM ya dispara la patada):

1. **VÍA 1 — VISIÓN (medida verdadera):** si se ve el arco, orbitar hacia su lado por el signo de
   `goal_opp_angle`. Perilla `MIX_CENTRAR_SHORT_SIGN` (+1/−1; 0 = apaga todo → viejo). **Esta es la
   buena y resuelve el caso común.** (Ya estaba implementada; el código de Elías era correcto cuando
   se ve el arco.)
2. **VÍA 2 — HEADING (respaldo, arco NO visible + `heading_valid`):** orbitar hacia el rumbo de
   arranque. Perilla PROPIA `MIX_CENTRAR_HEAD_SIGN`, **default 0 = APAGADO** hasta titularla en banco.
3. **VÍA 3 — DEFAULT:** ni arco ni heading útil → horario fijo (comportamiento viejo).

## El hallazgo clave del red-team (3 verdictos ALTA)
- **El signo del heading NO es el de la visión.** `heading_error` es **rotación del cuerpo** desde el
  boot, no un bearing al arco. Si arrancó mirando al arco, `goal_opp_angle ≈ −heading_error` → signo
  **opuesto**. Reusar `MIX_CENTRAR_SHORT_SIGN` para el heading mandaría por el lado **largo** casi
  siempre. **Fix:** perilla separada `MIX_CENTRAR_HEAD_SIGN`, independiente, a titular aparte.
- **One-shot + proxy:** si el arco está tapado al entrar, el heading sella un sentido y no lo
  reconsidera aunque la cámara después vea el arco del otro lado. **Mitigado** dejando la VÍA 2
  apagada por default (opt-in); si en banco molesta, se agrega un re-flip acotado (anotado, no hecho).
- **±180 (arco casi atrás):** el test de signo puro no es el corto. Caso angosto, documentado como límite.
- **El gate de frescura era código muerto** (la pelota recién se vio al entrar a centrar) → se quitó;
  el único gate real del respaldo es `heading_valid`.

## Premisa frágil del heading (honestidad)
La VÍA 2 sólo vale si el robot **arranca mirando al arco rival** (heading_inicial se sella al boot, NO
contra el arco) Y `heading_valid` es true. Por eso la visión SIEMPRE manda y el heading queda apagado
hasta confirmar en banco. Conecta con TASK-115 (¿llega heading válido?).

## Bug aparte encontrado (flageado, NO arreglado acá)
El disparo de patada por heading (`CENTRANDO_*`, ~líneas 549/607: `|error|<=1` tras 6s) NO gatea
`heading_valid` → con la fuente muerta podría patear desalineado. Toca la patada tuneada → spawn-task
separado (no se mete en este cambio).

## Compilación
`pio run -e central_robot1_mix_bno` → **SUCCESS**. Cambio aislado al bloque de entrada AVANZANDO→
CENTRANDO (mix_fsm.cpp) + 1 constante (mix_config.h). NO toca la patada, los CENTRANDO_*, ni el flujo.
NO validado en banco (regla #1).

## Banco (cuando se quiera)
- VÍA 1 (visión): si orbita por el lado LARGO → invertir `MIX_CENTRAR_SHORT_SIGN` a −1.
- VÍA 2 (heading): activarla poniendo `MIX_CENTRAR_HEAD_SIGN`=+1; arrancar mirando al arco, tapar el
  arco, girar el cuerpo a un lado, ver si orbita HACIA el arco; si va por el largo, invertir a −1.

Ver [[project-iita-soccer-2026-strategy]]. Hermano de `2026-06-22-centralmix-jugada-pelota-atras.md`.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
