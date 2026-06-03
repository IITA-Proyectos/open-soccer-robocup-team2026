---
title: "Convención de ejes y de izquierda/derecha — CANÓNICA para todo el código"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: canonico
tipo: referencia
tags: [convencion, ejes, frame, izquierda, derecha, coordenadas, lidar, todo-el-equipo]
---

# Convención de ejes — izquierda y derecha, de una vez y para siempre

> **Por qué existe este doc.** Distintos archivos del firmware tenían etiquetas
> de "izquierda/derecha" contradictorias en sus comentarios (cameras_fusion
> decía +90°=izquierda, behind_ball decía +90°=derecha; localization tenía el
> orden de ToF izq/der invertido respecto al hardware real). Eso es una fuente
> directa de goles en contra: el robot SIENTE la pelota a la derecha pero se
> MUEVE a la izquierda. Este doc fija UNA convención y todo el código debe
> respetarla. **Si un archivo dice otra cosa, el archivo está mal, no este doc.**

## La regla de oro: SIEMPRE en primera persona (egocéntrico)

**Para decir "izquierda" o "derecha" nos paramos EN EL LUGAR DEL ROBOT, mirando
hacia su frente — como un videojuego en primera persona.** La derecha del robot
es la mano derecha de alguien parado dentro del robot mirando adelante. **NO**
se mira al robot desde afuera/enfrente (ahí izquierda y derecha se invierten).

```
            FRENTE del robot (+Y)
                 ▲
                 │
   IZQUIERDA ◀───┼───▶ DERECHA
     (-X)        │       (+X)
                 │
                ATRÁS (-Y)

   (vista desde ARRIBA; "izquierda/derecha" = las del robot, no las tuyas
    si lo mirás de frente)
```

## 1. Marco del ROBOT (todo lo relativo al robot)

| Eje / signo | Significado |
|---|---|
| **+X** | **DERECHA** del robot |
| **−X** | **IZQUIERDA** del robot |
| **+Y** | **FRENTE** del robot (hacia donde mira) |
| **−Y** | **ATRÁS** del robot |
| **+omega / heading creciente** | giro **CCW** (antihorario) visto desde arriba = **el robot gira hacia su IZQUIERDA** |
| **−omega / heading decreciente** | giro **CW** (horario) = el robot gira hacia su **DERECHA** |

**Ángulo de un objeto relativo al robot:** `atan2(x, y)` (¡ojo, x primero!).
- `0°` = justo al frente.
- `+90°` = a la **DERECHA** (porque +x = derecha).
- `−90°` = a la **IZQUIERDA**.
- `±180°` = atrás.

> ⚠️ **Sutileza de signo de giro (importante).** El "ángulo a un objeto" con
> `atan2(x,y)` crece hacia la **derecha** (CW). Pero `omega`/heading crece hacia
> la **izquierda** (CCW). Son dos sentidos opuestos en el mismo sistema. Por eso
> para apuntar a un objeto a la derecha (ángulo +) hay que girar CW (omega −).
> Cualquier lazo de control que mezcle "ángulo al objetivo" con "heading/omega"
> tiene que respetar esta diferencia de signo. **Riesgo si se confunde: el robot
> gira para el lado contrario (feedback positivo → spin).** Verificar en
> hardware (ver tema-a-analizar abajo + TASK-202).

## 2. Marco de la CANCHA (pose absoluta)

**Origen de coordenadas `(0, 0)`:** el **rincón de PARED** donde se cruzan la
**pared del arco propio** (sur) y la **pared lateral izquierda** (oeste) —
mirando desde detrás del arco propio hacia el rival, es la esquina
inferior-izquierda. **Es sobre la PARED, NO sobre la línea blanca**: el marco es
**pared-a-pared** porque los ToF miden a las paredes (`y=0` = pared sur,
`x=0` = pared oeste; lo confirma `localization.cpp::classify_wall`:
`WALL_SOUTH → y=dperp`, `WALL_WEST → x=dperp`). La línea blanca queda **120 mm
hacia adentro** en cada lado, así que su esquina cae en `(120, 120)`. Desde ese
origen:

- **+Y crece hacia el arco rival** — es el lado **LARGO** de la cancha (la
  dirección de juego, arco-a-arco).
- **+X crece hacia la DERECHA** del robot mirando al arco rival — es el lado
  **CORTO** (de lateral a lateral).
- **Heading 0 = el robot mira al arco rival** (+Y).

**Puntos de referencia (mm, marco pared-a-pared):**

| Punto | (x, y) |
|---|---|
| Origen — esquina propia-izquierda (pared) | `(0, 0)` |
| Esquina rival-derecha (pared) | `(1820, 2430)` |
| Centro de la cancha | `(910, 1215)` |
| **Boca del arco propio** (centro, sobre la línea blanca sur) | `(910, 120)` |
| **Boca del arco rival** (centro, sobre la línea blanca norte) | `(910, 2310)` |
| Esquina de la **línea blanca** (SO) | `(120, 120)` |
| Cancha de juego (dentro de la línea) | `x ∈ [120, 1700]`, `y ∈ [120, 2310]` |

> **Los arcos NO están en la pared.** Las "posts" del arco van **sobre la línea
> blanca** (reglamento RCJ 2026), no en el plano de la pared. La boca del arco
> está en `y = 120` (propio) / `y = 2310` (rival); `y = 0` y `y = 2430` son las
> **PAREDES** (que van por detrás de los arcos). El arco es una caja de **600 mm
> de ancho × 100 mm de alto × 74 mm de profundidad** (centrado → abarca
> `x ∈ [610, 1210]`); su fondo queda ~74 mm por detrás de la línea, hacia la
> pared. La **localización** mide a las PAREDES (2430 × 1820), así que ese marco
> NO cambia; los `y = 120 / 2310` son sólo la referencia del **arco** (para
> detección por cámara / táctica).

| Eje | Dirección | Medida (pared-a-pared / línea blanca) |
|---|---|---|
| **+Y** | hacia el **arco rival** — lado **LARGO** (arco-a-arco) | **2430 mm** pared / **2190 mm** línea blanca |
| **+X** | a la **DERECHA** del robot mirando al arco — lado **CORTO** (lateral) | **1820 mm** pared / **1580 mm** línea blanca |
| **−X** | a la **IZQUIERDA** (pared "WEST", x=0) | — |

> **Dos medidas, no una (importante para no confundir los valores).** El
> reglamento RCJ Soccer 2026 define la **cancha de juego** (dentro de la línea
> blanca) en **2190 × 1580 mm** (219 × 158 cm) y un **outer area de 12 cm por
> lado** hasta las paredes → total **pared-a-pared 2430 × 1820 mm** (243 × 182
> cm). **Los ToF miden a las PAREDES**, así que la localización usa **2430 (Y,
> largo) × 1820 (X, corto)**.

**Coherencia con el código** (`pinout_common.h` + `localization.cpp::classify_wall`):
- `FIELD_HEIGHT_MM = 2430` (extensión en **Y**, arco-a-arco, el largo) y
  `FIELD_WIDTH_MM = 1820` (extensión en **X**, lateral, el corto). La pared
  NORTE (arco rival, +Y) está en `y = field_height = 2430`; las paredes
  ESTE/OESTE (laterales) en `x ∈ [0, field_width] = [0, 1820]`.
- Con heading 0 (mirando al arco), `world_angle = +90°` cae en `WALL_WEST` (−X) =
  izquierda del robot; `+270°` cae en `WALL_EAST` (+X) = derecha. ✅ Mismo signo
  que el marco robot.

> ⚠️ **CORREGIDO 2026-06-03 (este doc + firmware).** Antes este doc y el firmware
> tenían **`FIELD_WIDTH=2430` en X y `FIELD_HEIGHT=1820` en Y** — o sea, el eje
> hacia el arco rival (+Y) con el valor del lado **corto**. Está **invertido**:
> arco-a-arco es el lado **largo** (2430). Verificado contra el field spec oficial
> RCJ 2026 (cancha de juego 219 × 158 cm → 243 × 182 cm pared-a-pared, arcos en
> las paredes cortas). Se dieron vuelta los valores en `pinout_common.h`,
> `diag_pose_live.cpp` y los comentarios de `localization.h`; `FUENTES-DE-VERDAD`
> actualizado. (`FIRMWARE-PLACA-ARRIBA §6.4`, que tenía X=1820/Y=2430, era el
> correcto en cuanto a dimensiones.)

## 3. Mapeo de los 4 ToF (confirmado en banco 2026-05-30/31)

Coherente con todo lo anterior:

| Índice | Posición | Ángulo de montaje | Mira hacia |
|---|---|---|---|
| TOF[0] | **FRENTE** | 0°   | +Y (frente) |
| TOF[1] | **ATRÁS**  | 180° | −Y (atrás) |
| TOF[2] | **DERECHA**| 270° | +X (derecha del robot) |
| TOF[3] | **IZQUIERDA**| 90° | −X (izquierda del robot) |

(En `pinout_common.h`: `TOF_MOUNT_ANGLE_DEG = {0,180,270,90}`.)

> ⚠️ **Orientación interna de las zonas ≠ posición del sensor.** Cada ToF
> entrega una grilla 8×8; el orden de sus zonas depende de cómo está montado el
> chip. **Verificado en banco (2026-05-31):**
> - **TOF0/1/2 (frente/atrás/derecha)** comparten la misma orientación interna.
> - **TOF3 (izquierdo)** es de otro fabricante, montado mirando abajo → su
>   grilla está **rotada 180°** respecto a los otros 3 (arriba↔abajo y
>   izq↔der). Se corrige invirtiendo fila y columna: `(fila,col)→(7-fila,7-col)`.
>
> La corrección vive en `src/shared/tof_zone_orient.h` (header puro, con
> `test_tof_zone_orient` host-native, 7 tests) — **una sola fuente de verdad**
> que usan el diag y el futuro firmware del lidar-360. Verificar visualmente con
> `diag_top_tof_zonemap` tecla `c` (alterna vista cruda/corregida). TASK-203.
>
> (Nota: el marco común de TOF0/1/2 además tiene una rotación de ~90° respecto a
> la grilla "cruda impresa"; eso se resuelve al mapear zona→azimut para el
> lidar-360, no en `tof_zone_orient`, que solo iguala los 4 sensores entre sí.)

## 4. Quién usa qué (estado de cumplimiento)

| Módulo | Convención declarada | ¿Coincide con este doc? |
|---|---|---|
| `kinematics.h` (motores) | +X=derecha, +Y=frente, omega+=CCW | ✅ |
| `behind_ball.h` (táctica) | +x=derecha, atan2(x,y), +90=derecha | ✅ |
| `localization.cpp` (pose) | +90 mundo = izquierda (WEST) | ✅ (marco cancha, mismo signo) |
| `localization.h` (comentarios struct) | decía `[2]=izq,[3]=der` | ❌ CORREGIDO 2026-05-31 a `[2]=der,[3]=izq` |
| `cameras_fusion.h` (comentario GoalFused) | decía `+90=izquierda` | ❌ CORREGIDO 2026-05-31 a `+90=derecha` |
| Parser de cámara (signo real de x) | — | ⏳ A VERIFICAR EN HARDWARE (TASK-202) |
| Sentido del BNO (CW vs CCW) | localization asume CCW+ | ⏳ A VERIFICAR EN HARDWARE (TASK-202) |

## 5. Temas-a-analizar (requieren hardware o tocar CENTRAL — NO tocados acá)

### A — Signo real del eje X de la cámara (P1)
La cámara entrega `ball_x`. Si su +x NO es la derecha del robot (depende de
montaje + calibración + HMIRROR), entonces cuando la pelota está a la derecha,
`ball_x` sería negativo y el robot (que cree +x=derecha) se movería a la
**izquierda** → se aleja de la pelota. **Verificar en HW** (TASK-202): poner la
pelota a la derecha y ver el signo de `ball_x` en el debug. Si está invertido,
negar el signo en `cam_obs_to_robot_frame`.

### B — Sentido de giro del BNO (P1)
`localization.cpp:27` ya avisa: "asume BNO heading+ = CCW; si da CW positivo,
invertir el signo". Si el BNO real gira al revés, toda la localización y el
HeadingPID quedan con el signo de giro invertido → el robot gira para el lado
opuesto. **Verificar en HW** (TASK-202): girar el robot a su izquierda y ver si
el heading sube (CCW+) o baja.

### C — Mezcla de sentidos CW/CCW en strategy (P1, dominio CENTRAL)
`strategy.cpp` hace `ball_angle_rel = atan2(bx,by)` (crece CW/derecha) y luego
`heading_pid_set_target(heading + ball_angle_rel)` con heading CCW. Hay que
confirmar que el HeadingPID interpreta el error en el mismo sentido que omega
(kinematics dice omega+ = CCW). Si no, el lazo de orientación tiene signo
invertido. **Esto es dominio del agente CENTRAL** — se deja señalado, no se
toca desde TOP. Validar junto con TASK-202 (diag_central_drive ya existe).

## 6. Cómo usar este doc

- **Cualquiera que escriba código que toque izquierda/derecha, X/Y, o ángulos:**
  leer este doc primero. Si tu módulo necesita otra convención interna, conviene
  evitarlo; si es inevitable, convertir explícitamente en el borde y comentarlo.
- **Cualquiera que lea un comentario de izquierda/derecha en el código:** si
  contradice este doc, el comentario está mal → corregirlo y avisar.
- Está indexado en `docs/FUENTES-DE-VERDAD.md` como la fuente canónica del tema.
