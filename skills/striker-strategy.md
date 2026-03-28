# ⚡ Estrategia de Delantero — RoboCupJunior Soccer Open

---

## 1. PRINCIPIOS

El delantero tiene UN objetivo: **meter la pelota en el arco rival.**

```
Prioridad 1: Encontrar la pelota (buscar)
Prioridad 2: Posicionarse "detrás de la pelota" (entre pelota y nuestro arco)
Prioridad 3: Empujar/patear hacia arco rival
Prioridad 4: No salir del campo (evitar líneas)
```

---

## 2. BEHIND-THE-BALL (CLAVE)

NUNCA ir directo a la pelota. Siempre posicionarse de modo que la pelota quede ENTRE el robot y el arco rival:

```cpp
void approach_behind_ball(float ball_x, float ball_y, float goal_x, float goal_y) {
    // Vector pelota → arco rival
    float dx = goal_x - ball_x;
    float dy = goal_y - ball_y;
    float dist = sqrtf(dx*dx + dy*dy);
    dx /= dist; dy /= dist;  // Normalizar

    // Punto "detrás" de la pelota (opuesto al arco)
    float behind_x = ball_x - dx * 120;  // 120mm detrás
    float behind_y = ball_y - dy * 120;

    // Ir a ese punto
    go_to_point(behind_x, behind_y);
}
```

---

## 3. ORBIT (RODEAR LA PELOTA)

Si el robot está del lado equivocado de la pelota (entre pelota y arco rival), tiene que orbitar SIN tocar la pelota:

```cpp
void orbit_ball(float ball_angle, float ball_dist, bool orbit_cw) {
    // Moverse en arco alrededor de la pelota
    float tangent_angle = ball_angle + (orbit_cw ? -90 : 90);
    float speed = 300;  // mm/s

    float vx = speed * sinf(tangent_angle * M_PI / 180);
    float vy = speed * cosf(tangent_angle * M_PI / 180);

    // Mantener distancia constante a la pelota
    float dist_error = ball_dist - 150;  // Target: 150mm
    float radial_speed = dist_error * 2.0f;
    vx += radial_speed * sinf(ball_angle * M_PI / 180);
    vy += radial_speed * cosf(ball_angle * M_PI / 180);

    set_robot_velocity(vx, vy, 0);
}
```

---

## 4. SHOOT/PUSH

Cuando estoy behind-the-ball y alineado con el arco:

```cpp
bool ready_to_shoot(float ball_angle, float goal_angle) {
    // Estoy alineado si la pelota y el arco están en la misma dirección
    float alignment = fabsf(normalize_angle(ball_angle - goal_angle));
    return alignment < 20.0f;  // <20° de desalineamiento
}

void execute_shot() {
    // Avanzar rápido hacia la pelota
    set_robot_velocity(0, 500, 0);  // Full speed forward

    // Si tiene kicker (solenoide):
    if (ball_in_capture_zone()) {
        fire_kicker();
    }
}
```

---

## 5. DETECCIÓN DE ARCO RIVAL

El arco rival tiene color (cyan o magenta según lado). La OpenMV lo detecta:

```python
# OpenMV: detectar arco por color
GOAL_CYAN_LAB = (50, 100, -50, -10, -50, -10)    # Calibrar
GOAL_MAGENTA_LAB = (30, 80, 20, 70, -30, 10)      # Calibrar

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

## 6. PATRÓN DE BÚSQUEDA

Si no veo la pelota, hacer patrón de búsqueda:

```cpp
void search_pattern() {
    static int search_step = 0;
    static unsigned long step_start = millis();

    // Girar lento buscando pelota
    set_robot_velocity(0, 0, 120);  // 120°/s

    // Si llevo >3 segundos sin encontrar: avanzar un poco
    if (millis() - step_start > 3000) {
        set_robot_velocity(0, 200, 0);  // Avanzar 200mm/s por 500ms
        if (millis() - step_start > 3500) {
            step_start = millis();  // Reiniciar timer
        }
    }
}
```

---

## FUENTES

- IITA 2025 season striker code (legacy analysis)
- RoBorregos: robocup-soccer-open-2024 (GitHub)
- Behind-the-ball: técnica estándar en RoboCup Junior
- Orbit: usado por equipos top para reposicionamiento
