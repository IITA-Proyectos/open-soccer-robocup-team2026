# ⚽ Soccer Match FSM — Máquina de Estados del Partido

## Para RoboCupJunior Soccer Open (2 robots: arquero + delantero)

---

## 1. ESTADOS DEL PARTIDO

```cpp
enum MatchState {
    MS_KICKOFF_WAIT,     // Esperando señal del módulo de comunicación
    MS_SEARCH_BALL,      // No veo la pelota, buscar
    MS_ATTACK,           // Tengo la pelota o voy hacia ella
    MS_DEFEND,           // La pelota viene hacia nuestro arco
    MS_REPOSITION,       // Volver a posición óptima
    MS_AVOID_LINE,       // Sobre línea blanca, retroceder
    MS_STUCK_RECOVERY,   // Robot trabado >2s
    MS_HALFTIME_STOP     // Parada por juez o halftime
};
```

---

## 2. COMPORTAMIENTO POR ROL

### Delantero

```
KICKOFF_WAIT → (señal start) → SEARCH_BALL
SEARCH_BALL → (veo pelota) → ATTACK
ATTACK:
  Si pelota lejos: ir hacia ella (approach)
  Si pelota cerca: orbitar para quedar "behind the ball"
  Si alineado con arco: disparar/empujar
  Si pelota se fue: SEARCH_BALL
AVOID_LINE: retroceder 100mm, volver a estado anterior
STUCK: girar random 90-180°, avanzar 200mm, SEARCH_BALL
```

### Arquero

```
KICKOFF_WAIT → (señal start) → DEFEND
DEFEND:
  Si pelota visible: movimiento lateral para alinearse con pelota
  Si pelota muy cerca (<300mm): CLEARING (empujar a un lado)
  Si pelota en mitad rival: REPOSITION (volver al centro del arco)
REPOSITION: ir al centro del área, heading = mirar hacia adelante
AVOID_LINE / STUCK: igual que delantero
```

---

## 3. COMUNICACIÓN ENTRE ROBOTS

El delantero y arquero se comunican por WiFi/BT (via ESP32):

```cpp
struct SoccerTeamMessage {
    uint8_t  my_role;         // GOALKEEPER o STRIKER
    uint8_t  my_state;        // MatchState actual
    float    my_x, my_y;     // Mi posición estimada
    float    ball_angle;      // Dónde veo la pelota (ángulo)
    float    ball_dist;       // Distancia a pelota
    bool     ball_visible;    // ¿La veo?
    uint8_t  ball_confidence; // 0-100
};  // Enviar cada 100ms (10 Hz)
```

### Fusión de visión de 2 robots

Si ambos robots ven la pelota, fusionar:

```cpp
void fuse_ball_position(SoccerTeamMessage partner) {
    if (partner.ball_visible && my_ball_visible) {
        // Promediar posiciones ponderadas por confianza
        float w_me = my_confidence / (float)(my_confidence + partner.ball_confidence);
        float w_partner = 1.0f - w_me;
        fused_ball_x = w_me * my_ball_x + w_partner * partner_ball_x;
        fused_ball_y = w_me * my_ball_y + w_partner * partner_ball_y;
    } else if (partner.ball_visible) {
        // Solo el compañero la ve
        fused_ball_x = partner_ball_x;
        fused_ball_y = partner_ball_y;
    }
    // Si solo yo la veo: uso mi dato
}
```

---

## 4. ANTI-COLISIÓN ENTRE PROPIOS

Evitar que arquero y delantero choquen:

```cpp
float dist_to_partner = distance(my_pos, partner_pos);
if (dist_to_partner < 200) {  // 200mm = peligro
    // El ARQUERO tiene prioridad de paso
    if (my_role == STRIKER) {
        // Delantero esquiva al arquero
        dodge_away_from(partner_pos);
    }
    // El arquero no esquiva, mantiene su línea
}
```

---

## 5. MÓDULO DE COMUNICACIÓN DEL ÁRBITRO (2026)

Desde 2025, hay un módulo oficial que envía señal de start/stop:

```cpp
// Pin GPIO conectado al módulo del árbitro
#define REFEREE_PIN 34

bool game_running() {
    return digitalRead(REFEREE_PIN) == HIGH;
}

void loop() {
    if (!game_running()) {
        stop_all_motors();
        return;  // No hacer nada hasta que el juez dé la señal
    }
    // ... juego normal ...
}
```

---

## FUENTES

- RoboCupJunior Soccer Rules 2026
- IITA legacy 2025 season code (analizado)
- CAMBADA: role assignment and coordination
- Ver repo hermano `wro-2026-robosport-nacional-iita-salta` skills/05-strategy/ para FSM genéricas
