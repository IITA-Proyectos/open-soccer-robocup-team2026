# ⚡ Estrategia de Delantero — RoboCupJunior Soccer Open

## Comportamiento actual IITA 2026 + mejoras propuestas

---

## 1. PRINCIPIOS

El delantero tiene UN objetivo: **meter la pelota en el arco rival.**

```
Prioridad 1: Recorrer cancha buscando pelota
Prioridad 2: Aproximarse a la pelota
Prioridad 3: Posicionarse del lado OPUESTO al arco rival (behind-the-ball)
Prioridad 4: Empujar hacia el arco
Prioridad 5: Si no veo pelota: volver a buscar
```

---

## 2. COMPORTAMIENTO ACTUAL IITA: SEARCH → APPROACH → POSITION → PUSH

### FSM del delantero (4 estados)

```cpp
enum STRState {
    STR_SEARCH,      // Recorrer cancha buscando pelota
    STR_APPROACH,     // Pelota detectada, ir hacia ella
    STR_POSITION,     // Cerca de pelota, posicionarme behind-the-ball
    STR_PUSH          // Alineado con arco, empujar pelota
};

STRState str_state = STR_SEARCH;
```

### Implementación completa

```cpp
void striker_update() {
    float ball_angle = world.ball_angle;
    float ball_dist = world.ball_dist;
    float ball_conf = world.ball.confidence;
    bool ball_visible = ball_conf > 25;

    switch (str_state) {

        case STR_SEARCH:
            // ═══ RECORRER CANCHA BUSCANDO PELOTA ═══
            // Avanzar lento girando para escanear con la cámara
            // Si hay WorldModel y compañero ve la pelota, ir hacia allá

            if (ball_visible) {
                str_state = STR_APPROACH;
                break;
            }

            // ¿Mi compañero la ve? Ir hacia donde dice
            if (world.partner_alive && world.ball.confidence > 10) {
                // Pelota estimada por compañero
                float dx = world.ball.x[0] - my_x;
                float dy = world.ball.x[1] - my_y;
                float dir = atan2f(dx, dy) * 57.3f;
                drive(300, dir, 0);  // Ir hacia la estimación
            } else {
                // Nadie la ve: patrón de búsqueda
                search_pattern();
            }
            break;

        case STR_APPROACH:
            // ═══ IR HACIA LA PELOTA ═══

            if (!ball_visible) {
                // Perdí la pelota
                if (millis() - last_ball_seen > 500) {
                    str_state = STR_SEARCH;
                }
                // Mientras tanto: Kalman predict, seguir yendo
                break;
            }

            if (ball_dist < 250) {
                // Cerca de la pelota → posicionarme
                str_state = STR_POSITION;
                break;
            }

            // Ir hacia la pelota
            drive(min(600.0f, ball_dist * 1.5f), ball_angle, 0);
            break;

        case STR_POSITION:
            // ═══ POSICIONARSE BEHIND-THE-BALL ═══
            // El robot debe quedar entre la pelota y NUESTRO arco
            // (así cuando empuje, empuja hacia arco RIVAL)

            if (!ball_visible && millis() - last_ball_seen > 300) {
                str_state = STR_SEARCH;
                break;
            }

            {
                // Calcular punto "behind" (detrás de pelota, lado nuestro arco)
                float goal_x = FIELD_WIDTH / 2;
                float goal_y = FIELD_LENGTH;  // Arco rival
                float bx = world.ball.x[0], by = world.ball.x[1];

                float dx = goal_x - bx;
                float dy = goal_y - by;
                float d = sqrtf(dx*dx + dy*dy);
                if (d < 1) d = 1;
                dx /= d; dy /= d;

                float behind_x = bx - dx * 130;  // 130mm detrás
                float behind_y = by - dy * 130;

                float dist_to_behind = sqrtf(sq(behind_x - my_x) + sq(behind_y - my_y));

                if (dist_to_behind < 50) {
                    // Estoy posicionado → ¿alineado con arco?
                    float to_goal_angle = atan2f(goal_x - my_x, goal_y - my_y) * 57.3f;
                    float alignment = fabsf(normalize_angle(ball_angle - to_goal_angle));
                    if (alignment < 25) {
                        str_state = STR_PUSH;
                    }
                } else {
                    // Ir al punto behind (sin tocar pelota todavía)
                    // Si estoy del lado equivocado: orbitar
                    bool wrong_side = my_y > by + 50;  // Estoy delante de pelota
                    if (wrong_side) {
                        orbit_ball(ball_angle, ball_dist, my_x > bx);
                    } else {
                        go_to(behind_x, behind_y, 0);
                    }
                }
            }
            break;

        case STR_PUSH:
            // ═══ EMPUJAR PELOTA HACIA ARCO RIVAL ═══

            if (!ball_visible && millis() - last_ball_seen > 200) {
                str_state = STR_SEARCH;
                break;
            }

            if (ball_dist > 300) {
                // Pelota se alejó → volver a approach
                str_state = STR_APPROACH;
                break;
            }

            // ¡Empujar!
            drive(700, ball_angle, 0);  // 700mm/s hacia la pelota

            // Si tiene kicker y pelota en zona de captura:
            if (ball_dist < 60 && has_kicker) {
                fire_kicker();
                str_state = STR_APPROACH;  // Buscar pelota de nuevo
            }
            break;
    }
}
```

### Diagrama de la FSM

```
    ┌────────────────┐
    │   STR_SEARCH   │
    │ Recorrer campo │
    │ girar+avanzar  │
    └───────┬────────┘
            │ veo pelota
            ▼
    ┌────────────────┐
    │  STR_APPROACH   │
    │ Ir hacia pelota │
    └───────┬────────┘
            │ dist < 250mm
            ▼
    ┌────────────────┐
    │  STR_POSITION   │
    │ Behind-the-ball │
    │  orbit si mal   │
    └───────┬────────┘
            │ alineado < 25°
            ▼
    ┌────────────────┐
    │   STR_PUSH      │
    │ Empujar a arco  │
    │  (o kicker)     │
    └────────────────┘

   Cualquier estado → STR_SEARCH si pierde pelota >500ms
```

---

## 3. PATRÓN DE BÚSQUEDA (IITA)

El delantero recorre la cancha buscando la pelota:

```cpp
void search_pattern() {
    static uint8_t phase = 0;
    static uint32_t phase_start = millis();
    uint32_t elapsed = millis() - phase_start;

    switch (phase) {
        case 0:  // Girar en el lugar escaneando
            drive(0, 0, 0);  // Quieto
            // La omni puede girar sin trasladarse
            turn_relative(120);  // Girar 120°
            phase = 1; phase_start = millis();
            break;

        case 1:  // Avanzar un poco hacia el centro
            if (elapsed < 800) {
                drive_field(0, 300, 0);  // Avanzar 300mm/s al frente
            } else {
                phase = 2; phase_start = millis();
            }
            break;

        case 2:  // Girar de nuevo
            turn_relative(-120);  // Girar al otro lado
            phase = 3; phase_start = millis();
            break;

        case 3:  // Avanzar lateral
            if (elapsed < 600) {
                drive_field(250, 0, 0);  // Lateral
            } else {
                phase = 0; phase_start = millis();
            }
            break;
    }
}
```

---

## 4. ORBIT (RODEAR LA PELOTA)

Si estoy del lado equivocado (entre pelota y arco rival), orbitar sin tocar:

```cpp
void orbit_ball(float ball_angle, float ball_dist, bool orbit_cw) {
    float tangent_angle = ball_angle + (orbit_cw ? -90 : 90);
    float speed = 300;

    float vx = speed * sinf(tangent_angle * DEG2RAD);
    float vy = speed * cosf(tangent_angle * DEG2RAD);

    // Mantener distancia constante
    float dist_error = ball_dist - 150;
    float radial_speed = dist_error * 2.0f;
    vx += radial_speed * sinf(ball_angle * DEG2RAD);
    vy += radial_speed * cosf(ball_angle * DEG2RAD);

    set_robot_velocity(vx, vy, 0);
}
```

---

## 5. DETECCIÓN DE ARCO RIVAL

```python
# OpenMV: detectar arco por color
GOAL_CYAN_LAB = (50, 100, -50, -10, -50, -10)    # Calibrar
GOAL_MAGENTA_LAB = (30, 80, 20, 70, -30, 10)     # Calibrar

def detect_goal(img, target_color):
    blobs = img.find_blobs([target_color], pixels_threshold=100,
                           area_threshold=100, merge=True)
    if blobs:
        best = max(blobs, key=lambda b: b.pixels())
        angle = (best.cx() - img.width()//2) * FOV_DEG / img.width()
        return {'angle': angle, 'width': best.w(), 'visible': True}
    return {'visible': False}
```

---

## FUENTES

- **IITA 2025 season:** comportamiento actual del delantero (search → approach → position → push)
- RoBorregos: robocup-soccer-open-2024 (GitHub)
- Behind-the-ball: técnica estándar en RoboCup Junior
- Orbit: usado por equipos top para reposicionamiento
