---
title: "Arco propio/rival — cómo funciona hoy y propuesta concreta (versión corta)"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
status: in-progress
tipo: propuesta
robot: ambos (DELANTERO mix / ARQUERO mix)
horizonte: "Incheon 2026"
---

# Arco propio / rival — cómo funciona hoy y propuesta concreta

> Pedido por Gustavo, 2026-06-21. Documento corto y concreto.
> Relevamiento completo (largo) en [`2026-06-21-deteccion-arco-ataque-mix-delantero-arquero.md`](2026-06-21-deteccion-arco-ataque-mix-delantero-arquero.md).
> **Decisión del equipo:** la **cámara trasera** (`goal_own`) se **da por validada** para avanzar con lo
> nuevo. Si en banco falla, se debuggea ahí. *(El cierre formal de hardware lo hace el equipo, no Claude.)*

---

## 1. Cómo funciona hoy

**Quién decide qué arco es el propio:** lo decide la placa **TOP**, no la CENTRAL. La CENTRAL recibe la
respuesta ya hecha.

```
  cám frontal ─┐
               ├─► TOP ─► goal_polarity ─► escribe en el WorldSnapshot:
  cám trasera ─┘   (decide)                 goal_opp_*  = arco RIVAL  (a atacar)
                                            goal_own_*  = arco PROPIO (a defender)
                                                  │
                                    UART 0x60 @100 Hz (ya resuelto)
                                                  │
                                          ┌───────┴────────┐
                                   DELANTERO MIX      ARQUERO MIX
                                   usa goal_opp       HOY ignora goal_own
```

- **TOP** corre `goal_polarity`: *"el arco que el robot tiene al frente (`|ángulo| < 90°`) es el rival;
  el de atrás es el propio"*. Lo confirma con un **latch** de 30 lecturas (~0,3 s) y lo **congela** toda la
  mitad. Si no ve nada, usa fail-safe (amarillo = rival). → `src/shared/goal_polarity.{h,cpp}`.
  Verificado: corre en el flash de competencia (`top_robot2_pri` tiene `-DTOP_ENABLE_SNAPSHOT_TIMER`).

- **DELANTERO MIX** (`centralmix`, Elías): **ya ataca el arco correcto.** Lee `goal_opp` (vía un alias de
  color interno) y orbita la pelota hasta tener el rival al frente para patear. **Funciona hoy.**

- **ARQUERO MIX** (`arqueromix`, Virginia): **no mira el arco.** Defiende "lo que tenga atrás al arrancar"
  por *homing* a la línea del área, y despeja **recto al frente**. Si quedó girado, puede despejar hacia su
  propio arco (autogol). El dato del arco propio **le llega pero no lo usa.**

---

## 2. Tu pregunta: si la detección vive en el TOP, ¿cómo se entera la CENTRAL cuál es el propio?

**No la calcula: la lee de un campo del mensaje.** El TOP ya resolvió "propio vs rival" y lo manda
etiquetado dentro del `WorldSnapshot`. La CENTRAL no vuelve a decidir nada.

El campo exacto (contrato `WorldSnapshot`, `src/shared/types.h:118-123`):

```c
uint8_t goal_own_visible;          // 1 = lo está viendo;  0 = NO mirar el ángulo (sentinela)
int16_t goal_own_angle_centideg;   // ángulo al arco propio (válido solo si visible=1)
int16_t goal_own_distance_mm;      // distancia al arco propio (válido solo si visible=1)
```

(Y su gemelo `goal_opp_*` para el rival, `types.h:113-116`.)

**Y acá está lo bueno:** en el arquero mix ese dato **ya se está recibiendo y copiando** a la estructura
interna `g_aio` — solo que la FSM todavía no lo lee. En el build default:

```c
// amix_comm.cpp:103-110  (ya corre hoy)
g_aio.goal_blue_visible = own_vis;                    // ← arco PROPIO llega acá
g_aio.goal_blue_angle   = own_vis ? own_ang  : 0.0f;  //   (ángulo al propio)
g_aio.goal_blue_dist    = own_vis ? own_dist : 0.0f;
```

Es decir: **la CENTRAL ya tiene el arco propio a mano** (`g_aio.goal_blue_*`, o renombrado a `goal_own_*`
si lo limpiamos). Lo único que falta es **que la FSM del arquero lo use**.

> Resumen en una línea: el TOP decide → lo escribe como `goal_own_*` en el snapshot → la CENTRAL lo lee
> como un dato más. La "inteligencia" no se duplica; viaja resuelta por el cable.

---

## 3. La propuesta (concreta)

### Arquero mix (Virginia) — `src/arqueromix/` — lo importante
Que la FSM **lea `goal_own_angle` y oriente el despeje LEJOS del arco propio**, en vez de "siempre recto al
frente". Aditivo, con fallback garantizado:

```
al despejar:
   si goal_own_visible:   patear en dirección OPUESTA a goal_own_angle
   si NO visible (==0):   recto al frente   ← exactamente el comportamiento de hoy
```

- **Dato de entrada:** `g_aio.goal_blue_*` (ya poblado) — o exponer un `g_aio.goal_own_*` limpio.
- **Qué cambia:** `amix_fsm.cpp` (lógica de despeje) y, si se quiere claridad, `amix_io.h` (campo `goal_own`).
- **Lo que NO se toca:** el homing por línea sigue igual (es el respaldo robusto).

### Delantero mix (Elías) — `src/centralmix/` — chico, de robustez
1. **Cerrar la trampa del flag:** evitar que `-DMIX_ATTACK_BLUE` y el alias `kArcoRivalEsAmarillo` puedan
   quedar inconsistentes (hoy, si alguien compila con el flag sin tocar el alias, ataca al revés).
2. **Corregir el README** de centralmix: dice "confirmá a qué arco atacás antes del partido" — es falso, el
   TOP lo autodetecta.
- **Qué cambia:** `mix_comm.cpp` / `mix_fsm.cpp` + `README`. Comportamiento funcional **idéntico**; es
  limpieza + cerrar un agujero.

### Sin colisión
Elías solo toca `src/centralmix/`, Virginia solo `src/arqueromix/`. **Cero archivos compartidos** → trabajan
en paralelo. Nadie edita `src/shared/goal_polarity.*` (ya está hecho y corriendo).

> Mejoras que quedan FUERA de este round (son trabajo del TOP, no de los mix): fallback por perilla en vez
> de fijo, y re-latch de polaridad entre mitades. Track aparte, una sola mano.

---

## 4. Banco mínimo

| # | Qué | Criterio de aceptación |
|---|---|---|
| 0 | **Observabilidad:** ver `goal_own/goal_opp` y el latch (hoy la telemetría del TOP está dormida en competencia) | Hay un canal para leer los campos durante el banco |
| 1 | Arquero ve su arco propio atrás y despeja | El despeje sale **alejándose** del arco propio en ≥9/10 cuando `goal_own_visible=1` |
| 2 | Arquero girado 45° / sin ver el arco | Cae a "recto al frente" sin romper. **Cero despejes hacia el arco propio.** |
| 3 | Delantero (regresión) | Sigue atacando el arco rival igual que hoy (el cambio era solo de robustez) |

Si el Banco 1 falla (la cámara trasera da ángulos malos), **ahí** se debuggea `goal_own` — pero la feature
se diseña asumiéndola buena, como decidió el equipo.

---

*Documento de análisis. NO autoriza tocar firmware — espera el OK de Gustavo. Apoyo de Claude; atribución
según `AI-INSTRUCTIONS.md`.*
