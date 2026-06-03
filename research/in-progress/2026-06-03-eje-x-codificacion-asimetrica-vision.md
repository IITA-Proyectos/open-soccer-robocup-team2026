---
title: "Eje X de la cámara: codificación asimétrica respecto de Y → la mitad izquierda no se representa"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: in-progress
tags: [vision, camaras, openmv, n6, protocolo, contrato, eje-x, coordenadas, top]
robot: ambos
area: vision
tipo: analisis
related:
  - hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py
  - hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py
  - software/teensy/Soccer 2026/src/top/cameras.cpp
  - software/teensy/Soccer 2026/src/shared/cameras_fusion.cpp
  - docs/firmware/CONTRATO-DATOS-CAMARAS.md
  - team-tasks/2026-05-18-task-022-camara-operativa.md
  - "TASK-202 (signo del eje X de la cámara)"
---

# Eje X de la cámara: codificación asimétrica → la izquierda se pierde

## Resumen ejecutivo (para leer en 30 segundos)

Hoy la cámara codifica la coordenada **Y** de la pelota/arcos **con un corrimiento**
(`Y_coded = Y + 100`, y el TOP le resta 100), así que Y puede ser negativa (atrás)
o positiva (adelante). Pero la coordenada **X NO tiene ese corrimiento**: el script
manda X como un número 0..200 y el TOP lo lee tal cual. El problema: dentro del
script, X arranca **con signo** (negativo = izquierda) y al final se clampea a
`[0,255]`, lo que **manda a 0 todo lo que estaba a la izquierda**. Resultado: una
pelota a la izquierda del robot se transmite como "X=0" y el TOP la interpreta como
**"al frente"**. La mitad izquierda del campo de visión se colapsa.

Esto es **más profundo que TASK-202** (que solo pregunta "¿el signo de X está bien?").
Acá el problema es de **representabilidad**: con la codificación actual, la izquierda
no se puede expresar, tenga el signo que tenga.

> ⚠️ **No es un fix que se aplique solo.** Cambiar cómo se codifica X toca el
> **contrato de 9 bytes** (sagrado) y al **parser del TOP**. Hay que decidirlo con
> el agente TOP / Gustavo. Este doc deja el análisis listo para esa decisión.

## Evidencia (archivo:línea)

**1) En la cámara, X nace con signo pero se clampea matando los negativos.**
En `transformar()` de [`cam-frontal-n6.py`](../../hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py) (idéntico en `cam-trasera-n6.py`):

```python
X = max(-127, min(200, X))     # ← el autor ESPERA X con signo (centrado en 0)
Y = max(-100, min(100, Y))
Y_coded = int(Y) + 100         # ← Y se corre +100  → 0..200 (signo preservado)
X_int   = int(X)               # ← X NO se corre
...
X_int   = max(0, min(255, X_int))   # ← clampe uint8: TODO X<0 → 0  (¡pierde izquierda!)
```

La asimetría es explícita: Y se corrige con `+100`, X no. El primer clamp deja entrar
X negativos (la intención era X centrado en 0), pero el clamp final a `[0,255]` los
aplasta a 0.

**2) En el TOP, el parser lee X sin corrimiento e Y con corrimiento.**
[`cameras.cpp`](../../software/teensy/Soccer%202026/src/top/cameras.cpp):

```cpp
packet_.ball_x = static_cast<int16_t>(byte);            // X: SIN offset (0..255)
packet_.ball_y = static_cast<int16_t>(byte) - Y_OFFSET; // Y: CON offset (-100)
```

**3) El consumidor (fusión) calcula el ángulo con ese X.**
[`cameras_fusion.cpp:100`](../../software/teensy/Soccer%202026/src/shared/cameras_fusion.cpp):

```cpp
const float angle_rad = std::atan2(x, y);   // +x = DERECHA
```

Con X siempre ≥ 0 (porque la izquierda quedó en 0), `atan2(x, y)` nunca da un ángulo
hacia la izquierda real. Una pelota a la izquierda → X=0 → `atan2(0, y)` ≈ 0 → "al
frente". El robot "no ve" nada a su izquierda como izquierda.

**4) El contrato lo describe como "X nominal 0..200" sin offset**, mientras que para
Y documenta el offset de 100: [`CONTRATO-DATOS-CAMARAS.md`](../../docs/firmware/CONTRATO-DATOS-CAMARAS.md)
§1.2 y §4. O sea: la asimetría está incluso en el contrato escrito.

## Matiz honesto (no exagerar el síntoma)

La **homografía está sin calibrar** (placeholder), así que el rango real de X que
produce el script hoy no se conoce. Dos escenarios posibles según cómo quede la H
calibrada:

- **Si la H produce X centrado en 0** (negativo a la izquierda, como sugiere el
  primer clamp `-127..200`): el bug es **real y grave** — se pierde toda la izquierda.
- **Si la H produce X en 0..200 con 100 = centro**: entonces no hay negativos, pero
  el **TOP está mal** (debería restar 100 a X igual que a Y; hoy no lo hace) → todos
  los ángulos quedan corridos hacia la derecha.

**En los dos casos hay un problema.** Lo inequívoco a nivel de código —sin depender
de la calibración— es la **asimetría entre cómo se codifica X y cómo se codifica Y**.
Conviene resolverla **antes** del banco para que Virginia calibre contra una
convención correcta, en vez de calibrar y después descubrir que la izquierda no anda.

## Tema a analizar (formato coach)

**Prioridad: P1** (impacto alto en partidos: un robot que no ve la pelota a su
izquierda persigue mal y se queda "tuerto" de un lado).

- **risk-no-fix:** el robot interpreta toda la mitad izquierda como "al frente / a la
  derecha". Persigue mal la pelota cuando está a la izquierda, el arquero calcula mal
  el ángulo del arco rival a su izquierda. Difícil de diagnosticar en cancha porque
  "a veces ve bien" (cuando la pelota está a la derecha).
- **risk-fix:** cualquier arreglo toca la **semántica del contrato de 9 bytes**
  (aunque no el tamaño ni los headers). Si se cambia solo un lado (script o TOP) y no
  el otro, se rompe la interpretación. Hay que tocar **las dos puntas a la vez** y
  re-validar en banco. Requiere coordinación con el agente TOP.
- **tiempo (estimación honesta):**
  - Código (ambas opciones de abajo): ~1–2 h (script + parser + tests host del parser
    actualizados + bump de `contract-schema`).
  - Banco (validar izquierda/centro/derecha + homografía): ~1 h, junto con TASK-202.

## Opciones de fix (sin decidir acá)

### Opción A — Codificar X simétrico a Y (recomendada a discutir)
`X_coded = X + 100` en el script (X ∈ [-100,100] → 0..200), y el parser hace
`ball_x = byte - 100`, igual que con Y.
- **Pros:** simetría total X/Y, izquierda representable, fácil de razonar, el clamp
  uint8 deja de destruir información. Alinea el código con la intención del autor
  (X centrado en 0).
- **Contras:** **cambio semántico del contrato** → `contract-schema` pasa a 2; hay
  que actualizar parser TOP + tests + doc del contrato + ejemplos byte-a-byte.
  El sentinel (X=0,Y_coded=0) sigue funcionando: un objeto real en X=-100,Y=-100 es
  el único punto que colisiona (extremo improbable; ya está el guard `if X_int==0 and
  Y_coded==0: X_int=1`, que habría que adaptar a la nueva codificación).

### Opción B — Mantener X 0..200 con 100=centro y restar 100 en el TOP
Si se decide que la H entrega X en 0..200 con 100=centro, el script no cambia y el
TOP resta 100 a X (`ball_x = byte - 100`).
- **Pros:** el script de competencia no se toca; cambio chico en un solo archivo
  (`cameras.cpp`).
- **Contras:** sigue siendo asimétrico conceptualmente (X y Y codificados distinto en
  el script pero ambos restando 100 en el TOP — confuso); depende de que la H
  realmente entregue 100=centro, cosa que NO está garantizada hoy. **También** es un
  cambio de interpretación del contrato (X pasa a tener offset implícito).

> En ambas opciones el **tamaño del paquete (9 bytes), los headers (201/202/203) y el
> sentinel** no cambian. Lo que cambia es la **semántica del byte X**.

## Plan de prueba en banco (obligatorio — se enlaza con TASK-202)

1. Con la cámara montada y la homografía calibrada (TASK-022), poner la pelota en
   **3 posiciones**: izquierda clara, centro, derecha clara (a la misma distancia).
2. Leer el `ball_x` que reporta el TOP en cada una (con `diag_top_*` o el print de
   bring-up del script).
3. Criterio de aceptación:
   - Pelota a la **derecha** → `ball_x` y el ángulo de fusión **positivos**.
   - Pelota a la **izquierda** → `ball_x` y el ángulo **negativos** (hoy darían 0/al
     frente — ese es el síntoma a confirmar/refutar).
   - Pelota al **centro** → `ball_x` ≈ centro, ángulo ≈ 0.
4. Si la izquierda no se distingue del centro → se confirma el bug → aplicar la opción
   elegida (A o B) en **las dos puntas** y re-correr.

## Coordinación y captura a 2027

- **NO implementar sin acordar con el agente TOP** (contrato sagrado). Este doc es el
  insumo de esa charla.
- Una vez decidido, mover a `research/completed/` con el veredicto y el resultado de
  banco, y actualizar `CONTRATO-DATOS-CAMARAS.md` (`contract-schema`) +
  `docs/FUENTES-DE-VERDAD.md` en el mismo commit.
- Lección capitalizable a 2027: **codificar siempre las coordenadas con signo de la
  misma forma en los dos ejes** y testear los 4 cuadrantes en host antes del banco.

## Estado

`in-progress` — análisis cerrado, **decisión pendiente** (opción A vs B) + validación
de banco (TASK-202). No se tocó código ni contrato en esta sesión.
