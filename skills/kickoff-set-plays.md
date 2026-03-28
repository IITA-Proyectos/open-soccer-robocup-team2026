# 🏁 Jugadas de Salida (Kickoff Set Plays) — RoboCupJunior Soccer Open

## Skill: jugadas ensayadas para kickoff atacante y defensor

---

## 1. POR QUÉ IMPORTAN LAS JUGADAS DE SALIDA

El kickoff es el momento de mayor ventaja potencial:
- La pelota está en posición conocida (centro exacto)
- Los robots están en posiciones conocidas
- El rival aún no reaccionó
- Un gol en los primeros 3 segundos es posible si la jugada está bien ejecutada

**Observación de torneos:** Los equipos que tienen jugadas de salida ensayadas roban la pelota ANTES que los rivales la vean. Ganar el primer toque es ganar la mitad de la jugada.

---

## 2. KICKOFF ATACANTE (NOSOTROS SACAMOS)

### Reglas de colocación (2026)

```
- El equipo que saca coloca robots PRIMERO
- Todos los robots en SU mitad del campo
- El equipo que NO saca: robots a >30cm del centro (fuera del círculo)
- Señal del árbitro (módulo RCJ) → GO
```

### Play A: "Disparo Directo" (la más simple)

```
Colocación:
  Delantero: justo delante del centro, apuntando al arco rival
  Arquero: centro de nuestra área

         [ARCO RIVAL]

     . . . . CENTRO . . . .
              🟠       ← pelota
              [D]↑     ← delantero apuntando al arco

              [G]       ← arquero en línea de fondo
         [NUESTRO ARCO]
```

```cpp
void kickoff_play_A_direct_shot() {
    // === DELANTERO ===
    if (my_role == STRIKER) {
        // t=0: GO! Avanzar a máxima velocidad hacia pelota
        drive(1000, 0, 0);  // Full speed, derecho, heading=frente

        // Esperar hasta tocar pelota (~200ms)
        uint32_t t0 = millis();
        while (millis() - t0 < 500) {
            if (ball_in_capture_zone()) {
                fire_kicker();  // ¡Patear!
                break;
            }
            drive(1000, 0, 0);
            delay(10);
        }

        // Después del tiro: seguir la pelota
        str_state = STR_APPROACH;
    }

    // === ARQUERO ===
    if (my_role == GOALKEEPER) {
        // Quedarse en posición defensiva
        gk_state = GK_PATROL;
    }
}
```

### Play B: "Robo de Centro" (agresiva, para rival que saca)

```
Colocación (cuando el RIVAL saca):
  Delantero: lo más cerca posible del círculo central (30cm del centro)
  Arquero: centro de nuestra área

         [ARCO RIVAL]
              [R]  ← rival va a sacar
     . . . . CENTRO . . . .
              🟠       ← pelota
     . . . . . . . . . . .
           [D]↑          ← nuestro delantero en borde del círculo

              [G]       ← arquero
         [NUESTRO ARCO]
```

```cpp
void kickoff_play_B_steal_center() {
    // === DELANTERO ===
    if (my_role == STRIKER) {
        // t=0: GO! Sprint hacia la pelota
        // El rival también va, gana el más rápido
        uint32_t t0 = millis();

        // Fase 1: Sprint directo al centro (300ms)
        while (millis() - t0 < 300) {
            drive(1200, 0, 0);  // MÁXIMA velocidad
            delay(10);
        }

        // Fase 2: Si llegué primero, empujar pelota a un lado
        // (no al centro donde está el rival)
        if (world.ball.confidence > 50 && world.ball_dist < 200) {
            // Empujar pelota hacia la pared más cercana
            float push_dir = (my_x > FIELD_WIDTH/2) ? 45 : -45;
            drive(800, push_dir, 0);  // Empujar diagonal
            delay(300);
        }

        // Continuar con comportamiento normal
        str_state = STR_APPROACH;
    }

    // === ARQUERO ===
    if (my_role == GOALKEEPER) {
        // Avanzar un poco para interceptar si pelota pasa al delantero
        drive_field(0, 100, 0);  // Avanzar lento
        delay(500);
        gk_state = GK_PATROL;
    }
}
```

### Play C: "Pase Lateral" (avanzada, requiere coordinación)

```
Colocación (nosotros sacamos):
  Delantero: centro, ligeramente hacia un lado
  Arquero: avanzado a mitad campo, del otro lado

         [ARCO RIVAL]

     . . . . CENTRO . . . .
         🟠
    [D]↑                    ← delantero va a empujar pelota lateral
                  [G]↑     ← arquero avanzado, listo para recibir

         [NUESTRO ARCO]
```

```cpp
void kickoff_play_C_lateral_pass() {
    // === DELANTERO ===
    if (my_role == STRIKER) {
        // Empujar pelota LATERALMENTE hacia donde está el arquero
        float pass_dir = (world.partner_x > FIELD_WIDTH/2) ? 80 : -80;  // Casi lateral
        drive(800, pass_dir, 0);
        delay(400);  // Empujar

        // Luego ir hacia el arco rival (desmarque)
        drive_field(0, 600, 0);
        delay(500);
        str_state = STR_SEARCH;
    }

    // === ARQUERO ===
    if (my_role == GOALKEEPER) {
        // Esperar pelota lateralmente
        delay(400);  // Esperar que el delantero la empuje

        // Ir hacia donde va la pelota
        str_state = STR_APPROACH;  // Temporalmente actuar como striker

        // Después de 3 segundos: volver a rol de arquero
        delay(3000);
        gk_state = GK_RETREAT;
    }
}
```

---

## 3. KICKOFF DEFENSOR (RIVAL SACA)

### Play D: "Muro en el Medio" (conservadora)

```
Colocación:
  Delantero: en el borde del círculo, tapando línea de tiro al arco
  Arquero: en línea de fondo centrado

              [R] → 🟠     ← rival va a patear
     . . . . . . . . . . .
           [D]             ← delantero bloqueando

              [G]           ← arquero atrás
```

```cpp
void kickoff_play_D_wall() {
    if (my_role == STRIKER) {
        // Quedarme como muro 300ms, luego ir a buscar pelota
        delay(300);
        str_state = STR_APPROACH;
    }
    if (my_role == GOALKEEPER) {
        gk_state = GK_PATROL;
    }
}
```

### Play E: "Robo Agresivo" (la favorita de IITA)

```
Colocación:
  Delantero: lo más cerca posible del círculo, apuntando al centro
  Arquero: posición normal

  Idea: el delantero arranca a máxima velocidad hacia el centro
        para llegar a la pelota ANTES que el rival termine de sacar.
        Si el rival es lento o su kickoff es malo, robamos la pelota.
```

```cpp
void kickoff_play_E_aggressive_steal() {
    if (my_role == STRIKER) {
        // SPRINT al centro
        drive(1200, 0, 0);  // Máxima velocidad, directo al frente

        uint32_t t0 = millis();
        while (millis() - t0 < 800) {
            // Si veo la pelota y está cerca: tomar control
            if (world.ball.confidence > 40 && world.ball_dist < 300) {
                str_state = STR_POSITION;  // Posicionarme behind-the-ball
                return;
            }
            drive(1000, 0, 0);
            delay(10);
        }

        // Si no la conseguí en 800ms: buscar normalmente
        str_state = STR_SEARCH;
    }

    if (my_role == GOALKEEPER) {
        gk_state = GK_PATROL;
    }
}
```

---

## 4. SELECCIÓN DE JUGADA

```cpp
void execute_kickoff(bool we_kick) {
    if (we_kick) {
        // Nosotros sacamos: disparo directo es lo más confiable
        kickoff_play_A_direct_shot();

        // Alternativa si el rival es débil:
        // kickoff_play_C_lateral_pass();
    } else {
        // Rival saca: robo agresivo
        kickoff_play_E_aggressive_steal();

        // Alternativa conservadora:
        // kickoff_play_D_wall();
    }
}
```

### Tabla de selección

| Situación | Nosotros sacamos | Rival saca |
|-----------|:--:|:--:|
| Partido normal | Play A (directo) | Play E (robo agresivo) |
| Rival rápido | Play A (directo) | Play D (muro) |
| Rival lento | Play C (pase lateral) | Play E (robo agresivo) |
| Ganando | Play A (seguro) | Play D (muro conservador) |
| Perdiendo | Play C (arriesgado) | Play E + B (doble robo) |

---

## 5. TIMING CRÍTICO

```
t=0ms:    Señal GO del módulo RCJ
t=50ms:   Motores aceleran (rampa del OmniDriveBase)
t=150ms:  Robot alcanza velocidad máxima
t=300ms:  Delantero llega al centro (~450mm recorridos a 1200mm/s)
t=400ms:  Primer contacto con pelota (si todo sale bien)
t=500ms:  Pelota en movimiento hacia arco rival

Ventana crítica: 0-400ms
Si el rival llega antes de los 300ms, perdió el robo.
```

### Factores que afectan el timing

| Factor | Impacto | Cómo mejorar |
|--------|---------|-------------|
| Distancia de colocación | 30cm del centro (mínimo regla) vs más lejos | Colocar lo más cerca posible |
| Aceleración del robot | Ruedas patinan al arrancar | Rampa de aceleración suave |
| Tiempo de reacción al GO | ~20ms lectura GPIO + ~30ms aceleración | Polling rápido del módulo RCJ |
| Orientación al colocar | Si no apunta al centro, pierde tiempo girando | Colocar apuntando exacto |

---

## FUENTES

- **IITA 2025 season:** jugadas de kickoff probadas en competencia
- RoboCupJunior Soccer Rules 2026: colocación de kickoff, 30cm del centro
- Observación de torneos: equipos que roban primer toque ganan la jugada
- Ver skills/game-strategy-playbook.md para estrategia general
- Ver skills/communication-module-integration.md para señal GO del módulo
