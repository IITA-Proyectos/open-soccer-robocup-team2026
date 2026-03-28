# 🧤 Estrategia de Arquero — RoboCupJunior Soccer Open

---

## 1. PRINCIPIOS

El arquero tiene UN objetivo: **que la pelota no entre al arco.** Todo lo demás es secundario.

```
Prioridad 1: Bloquear tiros (alinearse con pelota)
Prioridad 2: Despejar pelota peligrosa (clearing)
Prioridad 3: No salir del área (quedarse cerca del arco)
Prioridad 4: Comunicar posición de pelota al delantero
```

---

## 2. MOVIMIENTO LATERAL (TRACKING)

El arquero se mueve lateralmente alineándose con la pelota:

```cpp
// Posición ideal del arquero:
//   X = misma X que la pelota (alineado)
//   Y = cerca de nuestro arco (y_arco + offset)
float GOAL_Y = 20;           // 20mm desde la pared de fondo
float GOAL_OFFSET = 80;      // Distancia del arco
float MAX_X_RANGE = 350;     // No salir más allá del arco

void goalkeeper_track(float ball_x, float ball_y) {
    // Solo moverse lateralmente
    float target_x = constrain(ball_x, -MAX_X_RANGE, MAX_X_RANGE);
    float target_y = GOAL_Y + GOAL_OFFSET;

    // PID lateral
    float error_x = target_x - my_x;
    float speed_x = pid_lateral.compute(error_x);

    // Mantener heading mirando al frente
    float heading_error = normalize_angle(0 - my_heading);
    float omega = pid_heading.compute(heading_error);

    set_robot_velocity(speed_x, 0, omega);
}
```

---

## 3. PREDICCIÓN DE TIRO

Usar el Kalman tracker para predecir dónde cruzará la pelota la línea del arco:

```cpp
void predict_goal_crossing(BallKalman& ball, float& cross_x, float& cross_time) {
    // Si la pelota viene hacia nuestro arco (vy negativo en nuestro sistema)
    if (ball.x[3] >= 0) {
        cross_x = ball.x[0];  // No viene, quedarme alineado
        cross_time = 999;
        return;
    }

    // Calcular cuándo la pelota llega a Y = línea del arco
    float dy = ball.x[1] - GOAL_Y;
    float vy = -ball.x[3];  // Velocidad hacia el arco (positiva)

    // Con fricción: tiempo = ln(1 - dy*(1-f)/(vy*dt)) / ln(f)
    // Simplificación: simular paso a paso
    float bx = ball.x[0], by = ball.x[1];
    float bvx = ball.x[2], bvy = ball.x[3];
    float t = 0;
    float dt = 0.01f;
    while (by > GOAL_Y && t < 3.0f) {
        bx += bvx * dt;
        by += bvy * dt;
        bvx *= ball.friction;
        bvy *= ball.friction;
        t += dt;
        // Rebote en paredes laterales
        if (bx < -FIELD_W/2 || bx > FIELD_W/2) bvx = -bvx * 0.7f;
    }
    cross_x = bx;
    cross_time = t;
}

// Uso:
float cross_x, cross_time;
predict_goal_crossing(ball, cross_x, cross_time);
if (cross_time < 1.0f) {
    // ¡Tiro en camino! Moverse a cross_x con urgencia
    emergency_lateral_move(cross_x);
} else {
    // Tiro lento o no viene → tracking normal
    goalkeeper_track(ball.x[0], ball.x[1]);
}
```

---

## 4. CLEARING (DESPEJE)

Cuando la pelota está muy cerca del arco, despejar:

```cpp
if (ball.x[1] < GOAL_Y + 150 && dist_to_ball < 200) {
    // Pelota peligrosa y cerca → despejar
    // Empujar hacia un lado (no hacia el centro)
    if (ball.x[0] > 0) {
        // Pelota a la derecha: despejar a la derecha
        set_robot_velocity(300, 200, 0);
    } else {
        set_robot_velocity(-300, 200, 0);
    }
}
```

---

## 5. ZONA DE OPERACIÓN

El arquero NO debe salir de su zona:

```
┌───────────────────────┐
│                       │
│   [delantero zone]    │
│                       │
│- - - - - - - - - - - -│  Centro del campo
│                       │
│  ┌─────────────┐    │
│  │  GK ZONE     │    │  Zona del arquero
│  │  (área chica) │    │
│  └─────────────┘    │
│  [███ ARCO ███]       │
└───────────────────────┘
```

```cpp
const float GK_MAX_Y = 600;  // No pasar la mitad del campo
const float GK_MAX_X = 400;  // No salir de los laterales del arco

void constrain_goalkeeper() {
    if (my_y > GK_MAX_Y) {
        // Demasiado lejos del arco → volver
        state = MS_REPOSITION;
    }
}
```

---

## FUENTES

- IITA 2025 season goalkeeper code (legacy analysis)
- CAMBADA: goalkeeper positioning and prediction
- Ball tracking con predicción: ver repo hermano `ball-tracking-advanced.md`
