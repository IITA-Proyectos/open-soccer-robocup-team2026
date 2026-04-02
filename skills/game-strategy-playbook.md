# ⚽ Playbook de Estrategias de Juego — RoboCupJunior Soccer Open

## Skill: formaciones, tácticas, coordinación, adaptación al rival

---

## 1. SISTEMA DE FORMACIONES DINÁMICAS

```cpp
enum Formation {
    FORM_CLASSIC,     // 1 arquero + 1 delantero (default)
    FORM_AGGRESSIVE,  // 2 atacando (0 defendiendo)
    FORM_DEFENSIVE,   // 2 defendiendo (pero sin multiple defense)
    FORM_DYNAMIC      // Roles adaptativos según posición de pelota
};

Formation current_formation = FORM_DYNAMIC;

void select_formation() {
    int time_remaining = get_game_time_remaining();  // segundos
    int score_diff = our_score - their_score;

    if (score_diff <= -3 && time_remaining < 120) {
        // Perdiendo feo, últimos 2 min → todo al ataque
        current_formation = FORM_AGGRESSIVE;
    } else if (score_diff >= 2 && time_remaining < 120) {
        // Ganando cómodo → proteger resultado
        current_formation = FORM_DEFENSIVE;
    } else {
        // Normal → adaptativo
        current_formation = FORM_DYNAMIC;
    }
}
```

---

## 2. FORMACIÓN DINÁMICA (RECOMENDADA)

```cpp
void execute_dynamic_formation() {
    float ball_y = world.ball.x[1];  // Y de la pelota en el campo
    bool ball_in_our_half = ball_y < FIELD_LENGTH / 2;
    bool ball_in_their_penalty = ball_y > FIELD_LENGTH - 400;
    bool ball_in_our_penalty = ball_y < 400;

    if (my_role == GOALKEEPER) {
        if (ball_in_our_penalty) {
            // Pelota peligrosa → defender activamente
            if (dist_to_ball < 200) {
                smart_clearing();
            } else {
                lateral_tracking();
            }
        } else if (ball_in_our_half) {
            // Pelota en nuestra mitad → defender posicional
            lateral_tracking();
        } else {
            // Pelota en mitad rival → avanzar a centro (support)
            float support_y = min(FIELD_LENGTH * 0.45f, ball_y - 300);
            go_to(FIELD_WIDTH / 2, support_y, 0);
        }
    }

    if (my_role == STRIKER) {
        if (ball_in_our_penalty && !world.am_i_closer_to_ball()) {
            // Arquero se encarga → posicionarme para contragolpe
            go_to(FIELD_WIDTH / 2, FIELD_LENGTH * 0.4f, 0);
        } else {
            // Atacar pelota normalmente
            execute_striker_behavior();
        }
    }
}
```

---

## 3. EVITAR MULTIPLE DEFENSE

```cpp
// CRÍTICO: nunca 2 robots en el área penal propia al mismo tiempo

bool im_in_own_penalty_area() {
    return my_y < PENALTY_AREA_Y && 
           fabsf(my_x - FIELD_WIDTH/2) < PENALTY_AREA_W/2;
}

void avoid_multiple_defense() {
    if (my_role == STRIKER && im_in_own_penalty_area()) {
        // El arquero tiene prioridad en el área
        if (partner_in_penalty_area()) {
            // ¡SALIR! Ir al centro
            go_to(FIELD_WIDTH / 2, FIELD_LENGTH / 2, 0);
            return;
        }
    }
    // El ARQUERO nunca sale del área para evitar multiple defense;
    // es el DELANTERO el que debe retirarse.
}
```

---

## 4. EMPUJE COORDINADO

```cpp
void coordinated_push() {
    // Solo funciona FUERA de áreas penales
    bool ball_in_any_penalty = world.ball.x[1] < 400 ||
                                world.ball.x[1] > FIELD_LENGTH - 400;
    if (ball_in_any_penalty) {
        // En área penal → solo 1 robot actúa
        execute_striker_behavior();
        return;
    }

    // Pelota en medio campo → ambos pueden cooperar
    if (world.am_i_closer_to_ball()) {
        // Yo empujo, behind-the-ball
        approach_behind_ball(world.ball.x[0], world.ball.x[1],
                             FIELD_WIDTH/2, FIELD_LENGTH);
    } else {
        // Compañero empuja, yo me posiciono como support
        float side = (world.ball.x[0] > FIELD_WIDTH/2) ? -250 : 250;
        go_to(world.ball.x[0] + side,
              world.ball.x[1] - 150,  // Un poco atrás
              0);  // Mirando al arco rival
    }
}
```

---

## 5. INTENTO DE PASE

```cpp
struct PassOpportunity {
    bool available;
    float target_x, target_y;
    float pass_angle;
};

PassOpportunity evaluate_pass() {
    PassOpportunity pass = {false, 0, 0, 0};

    // ¿Tengo el arco bloqueado?
    bool goal_blocked = world.rival_blocking_ball();
    if (!goal_blocked) return pass;  // Arco libre → disparar, no pasar

    // ¿Mi compañero tiene tiro libre?
    float partner_to_goal = distance(world.partner_x, world.partner_y,
                                      FIELD_WIDTH/2, FIELD_LENGTH);
    bool partner_has_shot = partner_to_goal < 800 &&
                            !rival_between(world.partner_x, world.partner_y,
                                           FIELD_WIDTH/2, FIELD_LENGTH);

    if (partner_has_shot) {
        pass.available = true;
        pass.target_x = world.partner_x;
        pass.target_y = world.partner_y;
        pass.pass_angle = atan2f(pass.target_x - world.ball.x[0],
                                  pass.target_y - world.ball.x[1]) * 57.3f;
    }
    return pass;
}

void try_pass_or_shoot() {
    auto pass = evaluate_pass();
    if (pass.available) {
        push_ball_in_direction(pass.pass_angle);  // "Pase"
    } else {
        execute_shot();  // Disparo directo
    }
}
```

---

## 6. KICKOFF

```cpp
void execute_kickoff(bool we_kick) {
    if (we_kick) {
        if (my_role == STRIKER) {
            // Avanzar y patear al arco
            go_to(FIELD_WIDTH/2, FIELD_LENGTH/2 + 50, 0);
            delay(200);  // Dejar que arranque
            fire_kicker();  // O empujar fuerte
        } else {
            // Arquero se queda en posición
            go_to(FIELD_WIDTH/2, 200, 0);
        }
    } else {
        // Defensa de kickoff
        if (my_role == STRIKER) {
            // Posicionarme cerca del círculo para interceptar
            go_to(FIELD_WIDTH/2 + 100, FIELD_LENGTH/2 - 350, 0);
        } else {
            // Arquero atrás
            go_to(FIELD_WIDTH/2, 150, 0);
        }
    }
}
```

---

## 7. ADAPTACIÓN AL RIVAL

```cpp
enum RivalStyle {
    RIVAL_UNKNOWN,
    RIVAL_AGGRESSIVE,   // Ambos atacan fuerte
    RIVAL_DEFENSIVE,    // Ambos defienden
    RIVAL_BALANCED,     // 1 GK + 1 STR normal
    RIVAL_ONE_STRONG    // Un robot bueno, otro malo
};

RivalStyle rival_style = RIVAL_UNKNOWN;

void analyze_rival() {
    // Después de 1 minuto de juego, analizar patrón
    if (game_time > 60000) {
        if (goals_received > 3) {
            rival_style = RIVAL_AGGRESSIVE;
        } else if (goals_scored == 0 && shots_blocked > 5) {
            rival_style = RIVAL_DEFENSIVE;
        }
    }
}

void adapt_to_rival() {
    switch (rival_style) {
        case RIVAL_AGGRESSIVE:
            // Priorizar defensa, contragolpes
            if (my_role == STRIKER) {
                // No subir tanto, quedarme en mitad campo
                max_y_for_striker = FIELD_LENGTH * 0.6f;
            }
            break;

        case RIVAL_DEFENSIVE:
            // Más paciencia, buscar ángulos, pases
            // Ambos pueden subir más
            if (my_role == GOALKEEPER) {
                // Avanzar más para dar soporte
                min_y_for_goalkeeper = FIELD_LENGTH * 0.35f;
            }
            break;

        default:
            break;
    }
}
```

---

## 8. TABLA DE DECISIÓN RÁPIDA

| Situación | Delantero hace | Arquero hace |
|-----------|---------------|-------------|
| Pelota en centro | Behind-the-ball → shoot | Avanzar a centro como support |
| Pelota cerca arco rival | Disparar/orbitar | Quedarse en mitad campo |
| Pelota cerca nuestro arco | Ir a mitad campo (contragolpe) | Clearing / lateral tracking |
| Pelota perdida | Buscar (patrón rotación) | Cubrir arco, comunicar si ve |
| Ganando, últimos 2min | Jugar conservador | Defender fuerte |
| Perdiendo, últimos 2min | Atacar sin parar | Subir a ayudar (¡cuidado arco!) |
| Rival empuja en área | Alejarse, dejar que canten pushing | Cubrir arco |
| Kickoff nuestro | Patear al arco | Posición defensiva |
| Kickoff rival | Interceptar | Posición defensiva |

---

## FUENTES

- RoboCupJunior Soccer Rules 2026: multiple defense, pushing, forcing, lack of progress, fouls
- Ver docs/game-strategy-analysis.md para análisis completo con diagramas
- Ver skills/soccer-match-fsm.md para FSM del partido
- Ver skills/goalkeeper-strategy.md y striker-strategy.md para tácticas por rol
- Ver skills/multi-camera-world-model.md para WorldModel
