---
title: "Convención de izquierda/derecha canónica (primera persona) + corrección de comentarios contradictorios"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [convencion, ejes, izquierda, derecha, camara, bno, lidar, docs, todo-el-equipo]
robot: ambos
area: percepcion + control
tipo: convencion + fix-docs
---

# Convención izq/der canónica + corrección de comentarios contradictorios

> **TL;DR.** Gustavo pidió asegurar que TODOS (personas y programas)
> interpreten izquierda/derecha igual: **primera persona, parado EN el robot
> mirando al frente** (la derecha del robot = mano derecha de alguien dentro
> del robot). Audité el código y encontré **comentarios contradictorios**:
> `cameras_fusion.h` decía `+90° = izquierda`, `behind_ball.h` decía
> `+90° = derecha` (misma fórmula `atan2(x,y)`), y `localization.h` tenía el
> orden de ToF `[2]=izq,[3]=der` (invertido respecto al hardware real). Creé el
> doc canónico `docs/CONVENCION-EJES-ROBOT.md`, corregí los comentarios
> incorrectos (NO la lógica), y dejé 2 verificaciones de hardware como tasks
> (signo de la cámara + sentido del BNO). Suite host-native: 246/246 verde.

## Qué pidió Gustavo

"Para decir derecha me paro como si fuera el robot mirando al frente y veo MI
derecha y MI izquierda — no miro al robot desde afuera. Es como un videojuego en
primera persona. Asegurémonos que TODOS los programas interpretan igual la
derecha e izquierda, y que esté documentado."

Esa es la convención **egocéntrica**, estándar en robótica. La adopté como
canónica.

## Qué encontré (auditoría de izq/der en el código)

| Archivo | Qué decía | ¿Correcto? |
|---|---|---|
| `kinematics.h` | +X=derecha, +Y=frente, omega+=CCW | ✅ |
| `behind_ball.h` | +x=derecha, atan2(x,y), +90=derecha | ✅ |
| `localization.cpp` | +90 mundo = WEST (izq del robot) | ✅ (marco cancha) |
| `cameras_fusion.h:40` | atan2(x,y), **+90 = izquierda** | ❌ contradice a behind_ball |
| `localization.h:21,31` | ToF **[2]=izq, [3]=der** | ❌ invertido vs HW real |

Las dos últimas son **errores de comentario**: la fórmula/el array de datos no
necesariamente estaban mal, pero la etiqueta confundía a cualquiera que leyera.
Y "el robot siente a la derecha pero la etiqueta dice izquierda" es exactamente
cómo nace un gol en contra.

## Qué hice

1. **`docs/CONVENCION-EJES-ROBOT.md`** (nuevo, canónico): regla de oro primera
   persona, tabla del marco robot (+X=derecha, +Y=frente, omega+=CCW=gira a la
   izquierda), marco cancha, mapeo de los 4 ToF, y la sutileza de signo CW/CCW.
2. **Corregí comentarios** (sin tocar lógica):
   - `cameras_fusion.h` + `.cpp`: `+90° = derecha` (era "izquierda").
   - `localization.h`: orden ToF `[0]=frente [1]=atrás [2]=derecha [3]=izquierda`
     + `tof_mount_angle_deg = {0,180,270,90}` en el comentario.
3. **Indexé en `FUENTES-DE-VERDAD.md`** como fuente canónica del tema.
4. Suite host-native: **246/246 verde** (solo comentarios + el array de ángulos
   ya corregido antes; nada de lógica).

## Lo que NO toqué (requiere hardware o es de CENTRAL) — temas-a-analizar

### A — Signo real del eje X de la cámara (P1 → TASK-202)
La cámara entrega `ball_x`. Si su +x no es la derecha del robot (depende de
montaje/HMIRROR/calibración), el robot se movería al lado opuesto de la pelota.
**Verificar en HW**: pelota a la derecha → `ball_x` debe ser positivo.

### B — Sentido de giro del BNO, CW vs CCW (P1 → TASK-202)
`localization.cpp` asume heading+ = CCW. Si el BNO da CW+, toda la pose y el
HeadingPID giran al revés. **Verificar en HW**: girar a la izquierda → heading
debe subir.

### C — Mezcla CW/CCW en strategy (P1, dominio CENTRAL)
`strategy.cpp` mezcla `atan2(x,y)` (crece CW/derecha) con heading/omega (CCW).
Hay que confirmar que el HeadingPID interpreta el error en el mismo sentido que
omega. **Dominio del agente CENTRAL** — señalado, no tocado desde TOP. Se valida
con `diag_central_drive` + TASK-202.

### D — Orientación interna de zonas ToF (P2 → TASK-203)
Para el barrido lidar-360, cada zona de cada sensor tiene que mapear al ángulo
real. El ToF izquierdo (otro fabricante, mirando abajo) puede tener la grilla
flippeada. Herramienta: `diag_top_tof_zonemap`.

## Por qué importa (coach)

Esto es exactamente el tipo de bug que NO aparece compilando ni en tests
host-native: el código es internamente consistente con una etiqueta equivocada.
Solo se cae en cancha, y se ve como "el robot va para el lado contrario". Fijar
UNA convención escrita + verificar los signos físicos (TASK-202) es barato ahora
y carísimo en Incheon. Ya pagamos una muestra de esto: el array de ángulos ToF
estaba cruzado (`{0,180,90,270}`) y solo se detectó cuando Gustavo confirmó las
posiciones físicas.

## Archivos

- `docs/CONVENCION-EJES-ROBOT.md` — **nuevo**, canónico.
- `src/shared/cameras_fusion.h` + `.cpp` — comentario +90=derecha.
- `src/shared/localization.h` — orden/ángulos ToF en comentarios.
- `docs/FUENTES-DE-VERDAD.md` — fila nueva (convención izq/der).
- `team-tasks/2026-05-31-task-202-*` — verificar cámara + BNO en HW.
- `team-tasks/2026-05-31-task-203-*` — orientación de zonas ToF (lidar-360).
