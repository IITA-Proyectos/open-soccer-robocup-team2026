# 🧤 Estrategia de Arquero — RoboCupJunior Soccer Open

## Comportamiento actual IITA 2026 + mejoras propuestas

---

## 1. PRINCIPIOS

El arquero tiene UN objetivo: **que la pelota no entre al arco.** Todo lo demás es secundario.

```
Prioridad 1: Patrullar línea de fondo alineado con pelota
Prioridad 2: Si pelota cerca Y arco despejado → rush para pegarle
Prioridad 3: SIEMPRE volver a línea de fondo después del rush
Prioridad 4: Despejar pelota peligrosa (clearing a los costados)
Prioridad 5: Comunicar posición de pelota al delantero
```

---

## 2. COMPORTAMIENTO ACTUAL IITA: PATROL + RUSH + RETREAT

### FSM del arquero (3 estados)

```cpp
enum GKState {
    GK_PATROL,    // Patrullando línea de fondo, buscando pelota
    GK_RUSH,      // Pelota cerca + camino libre → avanzar a pegarle
    GK_RETREAT    // SIEMPRE volver a línea de fondo después del rush
};

GKState gk_state = GK_PATROL;
uint32_t rush_start_time = 0;

// Constantes (calibrar por robot)
const float GOAL_LINE_Y = 100;        // Y de la línea de fondo (mm desde pared)
const float RUSH_TRIGGER_DIST = 400;  // Distancia máxima para decidir rush (mm)
const float RUSH_MAX_DURATION = 1500; // Máximo tiempo en rush (ms)
const float RETREAT_Y = 120;          // Y objetivo al volver
const float PATROL_X_RANGE = 350;     // Rango lateral de patrulla (mm desde centro)
```

### Implementación completa

```cpp
void goalkeeper_update() {
    float ball_angle = world.ball_angle;    // Ángulo a pelota desde cámara
    float ball_dist = world.ball_dist;      // Distancia a pelota
    float ball_conf = world.ball.confidence;

    switch (gk_state) {

        case GK_PATROL:
            // ═══ PATRULLA EN LÍNEA DE FONDO ═══
            // Moverse lateralmente alineado con la pelota
            // Siempre sobre la línea de fondo, mirando al frente

            if (ball_conf > 30) {
                // Veo la pelota → alinearme lateralmente
                float target_x = constrain_f(world.ball.x[0],
                    FIELD_WIDTH/2 - PATROL_X_RANGE,
                    FIELD_WIDTH/2 + PATROL_X_RANGE);
                drive_field(
                    (target_x - my_x) * 3.0f,  // Proporcional lateral
                    (GOAL_LINE_Y - my_y) * 2.0f, // Mantener en línea de fondo
                    0  // Heading = mirando al frente
                );

                // ¿Condiciones para RUSH?
                bool ball_close = ball_dist < RUSH_TRIGGER_DIST;
                bool ball_in_front = fabsf(ball_angle) < 40;  // Pelota delante mío
                bool path_clear = !rival_between_me_and_ball();
                bool ball_approaching = world.ball.x[3] < -50; // Viene hacia mí

                if (ball_close && ball_in_front && (path_clear || ball_approaching)) {
                    gk_state = GK_RUSH;
                    rush_start_time = millis();
                }
            } else {
                // No veo pelota → quedarme centrado en línea de fondo
                drive_field(
                    (FIELD_WIDTH/2 - my_x) * 2.0f,
                    (GOAL_LINE_Y - my_y) * 2.0f,
                    0
                );
            }
            break;

        case GK_RUSH:
            // ═══ AVANZAR A PEGARLE A LA PELOTA ═══
            // Rush rápido hacia la pelota para despejarla
            // Máximo 1.5 segundos, después SIEMPRE volver

            if (millis() - rush_start_time > RUSH_MAX_DURATION) {
                // Tiempo agotado → RETREAT inmediato
                gk_state = GK_RETREAT;
                break;
            }

            if (ball_conf < 20) {
                // Perdí la pelota durante el rush → volver
                gk_state = GK_RETREAT;
                break;
            }

            if (ball_dist < 80) {
                // ¡Llegué a la pelota! Pegarle y volver
                // Empujar hacia adelante (o hacia un costado si hay rival)
                if (rival_in_front()) {
                    // Despejar al costado
                    float clear_dir = (my_x > FIELD_WIDTH/2) ? -45 : 45;
                    push_ball_in_direction(clear_dir);
                } else {
                    // Despejar hacia adelante
                    push_ball_in_direction(0);
                }
                delay(200);  // Empujar 200ms
                gk_state = GK_RETREAT;
                break;
            }

            // Ir hacia la pelota a máxima velocidad
            drive(800, ball_angle, 0);  // 800mm/s, dirección a pelota, heading=frente
            break;

        case GK_RETREAT:
            // ═══ VOLVER A LÍNEA DE FONDO ═══
            // Prioridad máxima: regresar al arco

            float dist_to_goal_line = fabsf(my_y - RETREAT_Y);
            if (dist_to_goal_line < 30) {
                // Llegué → volver a patrullar
                gk_state = GK_PATROL;
                break;
            }

            // Ir a línea de fondo lo más rápido posible
            drive_field(
                (FIELD_WIDTH/2 - my_x) * 2.0f,  // Centrarme lateralmente
                (RETREAT_Y - my_y) * 5.0f,        // RÁPIDO hacia atrás
                0
            );
            break;
    }
}
```

### Diagrama de la FSM

```
                    ┌────────────────┐
                    │   GK_PATROL    │
                    │ Línea de fondo │
                    │ lateral track  │
                    └───────┬────────┘
                            │
                  pelota cerca (<400mm)
                  + delante mío (<40°)
                  + camino libre O viene hacia mí
                            │
                            ▼
                    ┌────────────────┐
                    │   GK_RUSH      │
                    │ Avanzar rápido │
                    │ hacia pelota   │
                    └───────┬────────┘
                            │
                  toqué pelota (dist<80mm)
                  O timeout (>1.5s)
                  O perdí la pelota
                            │
                            ▼
                    ┌────────────────┐
                    │  GK_RETREAT    │
                    │ Volver a línea │
                    │ de fondo ASAP  │
                    └───────┬────────┘
                            │
                  llegué a línea de fondo
                            │
                            ▼
                    [vuelve a GK_PATROL]
```

---

## 3. PREDICCIÓN DE TIRO (MEJORA SOBRE IITA ACTUAL)

El Kalman tracker predice dónde cruzará la pelota la línea del arco:

```cpp
void predict_goal_crossing(BallKalman& ball, float& cross_x, float& cross_time) {
    if (ball.x[3] >= 0) {
        cross_x = ball.x[0];  // No viene hacia nosotros
        cross_time = 999;
        return;
    }

    // Simular trayectoria con fricción
    float bx = ball.x[0], by = ball.x[1];
    float bvx = ball.x[2], bvy = ball.x[3];
    float t = 0, dt = 0.01f;
    while (by > GOAL_LINE_Y && t < 3.0f) {
        bx += bvx * dt; by += bvy * dt;
        bvx *= ball.friction; bvy *= ball.friction;
        t += dt;
        if (bx < 0 || bx > FIELD_WIDTH) bvx = -bvx * 0.7f;
    }
    cross_x = bx;
    cross_time = t;
}

// Integrar con PATROL: si el tiro viene rápido, anticipar lateralmente
void goalkeeper_predictive_patrol() {
    float cross_x, cross_time;
    predict_goal_crossing(world.ball, cross_x, cross_time);

    if (cross_time < 1.0f) {
        // ¡Tiro en camino! Moverme al punto de cruce con urgencia
        emergency_lateral_move(cross_x);
    } else {
        // Tracking normal
        goalkeeper_update();  // FSM normal
    }
}
```

---

## 4. ZONA DE OPERACIÓN

```
┌─────────────────────────┐
│    [ARCO RIVAL]         │
│                         │
│                         │
│  - - - centro - - - - - │
│                         │  ← GK nunca pasa de acá
│                         │     (excepto durante RUSH, máx 1.5s)
│  ┌─────────────────┐   │
│  │ ZONA DE PATRULLA │   │  ← Movimiento lateral normal
│  │   (línea fondo)  │   │
│  └─────────────────┘   │
│  [████ ARCO ████]       │
└─────────────────────────┘
```

---

## 5. CALIBRACIÓN DEL RUSH

| Parámetro | Valor inicial | Ajustar si... |
|-----------|:--:|---|
| `RUSH_TRIGGER_DIST` | 400mm | El GK sale demasiado (bajar) o nunca sale (subir) |
| `RUSH_MAX_DURATION` | 1500ms | Se queda fuera mucho tiempo (bajar) |
| `PATROL_X_RANGE` | 350mm | No cubre el arco entero (subir) o sale mucho (bajar) |
| `GOAL_LINE_Y` | 100mm | Demasiado pegado a pared (subir) o lejos (bajar) |
| Rush speed | 800mm/s | Demasiado lento para llegar (subir) |

---

## FUENTES

- **IITA 2025 season:** comportamiento actual del arquero (patrol + rush + retreat)
- CAMBADA: goalkeeper positioning and prediction
- Ball tracking con predicción: ver repo hermano `ball-tracking-advanced.md`
- RoboCupJunior Soccer Rules 2026: multiple defense, pushing
