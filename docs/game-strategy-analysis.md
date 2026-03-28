# Estrategias de Juego para RoboCupJunior Soccer Open 2026

## Análisis profundo: formaciones, coordinación, reglas que limitan, y tácticas ganadoras

**Autor:** Gustavo Viollaz (IITA Salta) con asistencia de Claude Opus 4.6  
**Fecha:** 2026-03-28

---

## 1. RESTRICCIONES DE REGLAS QUE DEFINEN LA ESTRATEGIA

Antes de hablar de estrategia, hay que entender las reglas que la limitan:

### 1.1 Multiple Defense (Defensa Múltiple)

> "Si más de un robot del equipo defensor entra al área penal, toma posición defensiva y afecta sustancialmente el juego, se llama 'Multiple Defense'. El robot con menor influencia en el juego es movido al centro del campo. Si hay un arquero involucrado, el otro robot es el que se mueve."

**Implicación:** NO se puede poner 2 robots defendiendo dentro del área penal. Si ambos están en el área, el árbitro mueve al delantero al centro. El arquero se queda.

### 1.2 Pushing (Empuje)

> "Si un robot atacante y un defensor se tocan dentro del área penal, y al menos uno tiene contacto con la pelota, puede llamarse 'pushing'. La pelota se mueve al neutral spot más lejano. Un gol anotado por pushing no se concede."

**Implicación:** No se puede entrar al área rival empujando robots. La fuerza bruta no funciona dentro del área penal.

### 1.3 Forcing (2026 nuevo)

> "Se llama cuando un robot con la pelota usa mayor fuerza para 'forzar' a dos robots rivales dentro de su área penal."

**Implicación:** Si tu delantero empuja la pelota contra 2 defensores rivales en su área, es falta del atacante. Hay que buscar espacios, no forzar.

### 1.4 Lack of Progress

> "Si no hay progreso en el juego durante un tiempo razonable (pelota trabada entre robots, sin cambio de posición), el árbitro mueve la pelota a un neutral spot."

**Implicación:** Las estrategias de bloqueo estático no funcionan. Tiene que haber movimiento.

### 1.5 Foul

> "Si un robot ataca o carga continuamente contra un robot que NO tiene la pelota, el árbitro llama foul. El robot es considerado dañado."

**Implicación:** No se puede marcar al jugador rival sin pelota. No existe el "marking" de fútbol real.

---

## 2. FORMACIONES Y ROLES

### 2.1 Formación Clásica: Arquero + Delantero (1-1)

```
         [ARCO RIVAL]

              D  ← Delantero: busca pelota, ataca

         - - - centro - - -

              G  ← Arquero: defiende arco

         [NUESTRO ARCO]
```

**Ventajas:** Simple, predecible, siempre hay alguien defendiendo.
**Desventajas:** Solo 1 robot atacando. Si el delantero pierde la pelota, no hay backup.
**Cuándo usar:** Estrategia default. Funciona contra la mayoría de equipos.

### 2.2 Formación Agresiva: Doble Ataque (0-2)

```
         [ARCO RIVAL]

           D1    D2  ← Ambos buscan/empujan pelota

         - - - centro - - -

              (vacío)

         [NUESTRO ARCO]
```

**Ventajas:** Presión máxima, la pelota siempre está atacada por alguno.
**Desventajas:** Arco propio DESPROTEGIDO. Un contragolpe = gol seguro.
**Cuándo usar:** Últimos 2 minutos perdiendo. O contra equipo claramente inferior.
**Riesgo:** Si el rival tiene buen delantero, es suicidio.

### 2.3 Formación Defensiva: Doble Defensa (2-0)

```
         [ARCO RIVAL]

              (vacío)

         - - - centro - - -

           G    D  ← Ambos defienden, D sale cuando puede

         [NUESTRO ARCO]
```

**⚠️ CUIDADO con Multiple Defense:** Los dos NO pueden estar dentro del área penal al mismo tiempo. El delantero debe quedarse FUERA del área o moverse al medio campo cuando el arquero está dentro.

**Ventajas:** Muy difícil que anoten. Buen contra contra equipos agresivos.
**Desventajas:** No atacás. Dependés de contragolpes.
**Cuándo usar:** Ganando por 1-2 goles en los últimos minutos.

### 2.4 Formación Dinámica: Roles Adaptativos (RECOMENDADA)

```
Si pelota en mitad rival:
  Delantero: atacar pelota (behind-the-ball → shoot)
  Arquero: avanzar a mitad de campo (support, NO entrar a area rival)

Si pelota en mitad propia:
  Delantero: bajar a mitad campo, interceptar
  Arquero: defender arco (lateral tracking)

Si pelota en nuestro area penal:
  Arquero: clearing (despejar)
  Delantero: posicionarse para recibir despeje (mitad campo)

Si pelota perdida (nadie la ve):
  Delantero: buscar activamente (patrón de búsqueda)
  Arquero: cubrir arco, comunicar si la ve
```

---

## 3. TÁCTICAS ESPECÍFICAS

### 3.1 Empuje Coordinado (Push Tandem)

Dos robots empujando la pelota juntos hacia el arco rival. Legal FUERA del área penal.

```
    [Robot A]──🟠──[Robot B]
              ↓
         empujan juntos hacia arco
```

**Cuándo funciona:** La pelota está en el medio del campo, lejos de áreas penales. Ambos robots se alinean detrás de la pelota y empujan.

**Implementación:**
```cpp
void push_tandem() {
    // Robot A (más cerca): behind-the-ball, empujar
    // Robot B (más lejos): posicionarse al costado de A
    //   para evitar que rival desvíe la pelota
    if (am_i_closer_to_ball()) {
        approach_behind_ball();
        push_forward();
    } else {
        // Support: posicionarme para recibir si la pelota rebota
        float offset_x = (world.ball.x[0] > FIELD_W/2) ? -200 : 200;
        go_to(world.ball.x[0] + offset_x, world.ball.x[1] - 100, 0);
    }
}
```

**⚠️ Limitación:** Dentro del área rival, si ambos tocan al defensor, es pushing/forcing. Solo funciona en medio campo.

### 3.2 Pase (el Santo Grial del Junior Soccer)

Muy pocos equipos Junior implementan pases reales. Los que lo hacen ganan ventaja ENORME.

```
Robot A tiene la pelota cerca de la pared:
  1. Robot A detecta que el arco está bloqueado por rival
  2. Robot A ve que Robot B está libre al otro lado
  3. Robot A empuja pelota lateralmente hacia Robot B
  4. Robot B intercepta y dispara al arco (ahora libre)

Tiempo total: ~2 segundos
```

**Requisitos:**
- WorldModel compartido (ambos robots saben dónde está el compañero)
- Robot A debe poder determinar que el arco está bloqueado
- Robot B debe anticipar el pase y posicionarse
- Timing preciso (la pelota se frena por fricción)

**Implementación simplificada:**
```cpp
// Robot A (con pelota):
if (goal_blocked_by_rival() && partner_is_in_scoring_position()) {
    // "Pase" = empujar pelota hacia el compañero
    float pass_angle = angle_to(world.partner_x, world.partner_y);
    push_ball_in_direction(pass_angle);
}

// Robot B (sin pelota):
if (partner_has_ball() && i_have_clear_shot()) {
    // Posicionarme para recibir
    go_to(scoring_position_x, scoring_position_y, angle_to_goal);
}
```

### 3.3 Desmarque (Run Behind)

```
Situación: pelota en posesión de nuestro delantero, rival bloqueando arco.

  [ARCO RIVAL]
    [RIVAL_GK]     ← bloqueando
         🟠 [D]    ← nuestro delantero con pelota

              [G]  ← nuestro arquero
  [NUESTRO ARCO]

Acción: Arquero avanza por el otro lado mientras delantero distrae.

  [ARCO RIVAL]
    [RIVAL_GK]     ← ocupado con D
 [G]→    🟠 [D]    ← D empuja pelota lateral a G
              
  [NUESTRO ARCO]   ← desprotegido momentáneamente
```

**Riesgo:** Arco propio queda vacío. Solo hacer si estás seguro del pase.

### 3.4 Wall Play (Usar Paredes)

```
La pelota está contra la pared. El rival la bloquea de frente.

  PARED ─────────────────────
         🟠 [RIVAL]
         [D]

Acción: Empujar pelota contra la pared en ángulo
         → rebota y pasa al rival por el otro lado

  PARED ─────🟠──────────────
              ↗ [RIVAL]
         [D]
```

Esto es básicamente un auto-pase usando la pared.

### 3.5 Blocking Run (Pantalla)

```
Delantero va hacia el arco con la pelota.
Rival GK viene a bloquearlo.

Acción: Arquero se posiciona como "pantalla" para bloquear
        al rival GK (sin tocarlo, eso sería foul).

⚠️ CUIDADO: No tocar al rival sin pelota = foul.
   Solo posicionarse en el camino, no cargar.
```

---

## 4. TÁCTICAS DEFENSIVAS

### 4.1 Intercepción Predictiva

No ir hacia donde ESTÁ la pelota, ir hacia donde VA A ESTAR.

```cpp
// Arquero: si la pelota viene hacia mi arco
if (world.ball_approaching_own_goal()) {
    auto intercept = world.ball.find_intercept(my_x, my_y, my_speed);
    if (intercept.reachable) {
        go_to(intercept.x, intercept.y, 0);  // Interceptar
    }
}
```

### 4.2 Clearing Inteligente

Cuando el arquero despeja, no tirar al centro (donde está el rival). Despejar a los costados.

```cpp
void smart_clearing() {
    // ¿Hay rival en el centro?
    if (rival_in_center()) {
        // Despejar hacia el lado libre
        float clear_dir = (world.ball.x[0] > FIELD_W/2) ? -60 : 60;  // Grados
        push_ball_in_direction(clear_dir);
    } else {
        // Centro libre → despejar al medio
        push_ball_in_direction(0);
    }
}
```

### 4.3 Contragolpe

```
1. Arquero despeja pelota
2. Delantero (que estaba en mitad de campo) intercepta
3. Delantero tiene campo libre → ataque rápido
4. Rival está en nuestra mitad → su arco está expuesto
```

**Implementación:** El delantero NUNCA baja al área propia. Se queda en mitad de campo listo para recibir despejes.

---

## 5. ADAPTACIÓN AL RIVAL

### 5.1 Contra rival con buen delantero

```
Problema: su delantero es rápido y preciso.
Solución:
  - Arquero más agresivo (más adelante, interceptar antes)
  - Delantero baja a ayudar cuando pelota está en nuestra mitad
  - ⚠️ Sin entrar al área penal (multiple defense)
```

### 5.2 Contra rival que solo defiende

```
Problema: sus 2 robots defienden, no atacan.
Solución:
  - Paciencia. Buscar ángulos.
  - Usar paredes para pasar.
  - Delantero orbita buscando huecos.
  - Si ambos están en su área → multiple defense a ellos.
```

### 5.3 Contra rival con kicker potente

```
Problema: su delantero tiene kicker de largo alcance.
Solución:
  - Arquero más atrás, pegado al arco.
  - No dejar pelota quieta en el centro (el rival puede disparar de lejos).
  - Mantener pelota en movimiento.
```

### 5.4 Contra rival que empuja (pushing)

```
Problema: su robot usa fuerza bruta dentro del área.
Solución:
  - No pelear pelota en el área (dejar que el árbitro llame pushing).
  - Esperar que el árbitro mueva la pelota al neutral spot.
  - Posicionarse cerca del neutral spot para ganar la pelota primero.
```

---

## 6. COORDINACIÓN POR COMUNICACIÓN

### 6.1 Mensajes estratégicos entre robots

```cpp
// Extender TeamMessage con intención estratégica
enum TeamIntent {
    INTENT_IDLE,            // Sin acción
    INTENT_GOING_FOR_BALL,  // Voy a buscar la pelota
    INTENT_HAVE_BALL,       // Tengo la pelota
    INTENT_COVERING_GOAL,   // Cubriendo arco
    INTENT_OPEN_FOR_PASS,   // Estoy libre para recibir pase
    INTENT_CLEARING,        // Voy a despejar
    INTENT_SEARCHING        // Buscando pelota
};
```

### 6.2 Decisión distribuida

```cpp
void decide_who_goes() {
    if (world.am_i_closer_to_ball()) {
        // Yo voy a la pelota
        my_intent = INTENT_GOING_FOR_BALL;
    } else {
        // Compañero va, yo cubro
        if (my_role == GOALKEEPER) {
            my_intent = INTENT_COVERING_GOAL;
        } else {
            my_intent = INTENT_OPEN_FOR_PASS;
        }
    }

    // Excepción: si pelota viene a mi arco, arquero SIEMPRE defiende
    if (my_role == GOALKEEPER && world.ball_approaching_own_goal()) {
        my_intent = INTENT_COVERING_GOAL;
    }
}
```

---

## 7. ESTRATEGIA DE KICKOFF

### 7.1 Nosotros pateamos (kickoff)

```
Colocación:
  Delantero: frente a la pelota, apuntando al arco rival
  Arquero: en el área propia

Acción:
  t=0: Señal GO
  t=0.1s: Delantero avanza y patea pelota hacia arco rival
  t=0.5s: Delantero sigue la pelota
  t=1.0s: Si pelota rebotó, delantero reposiciona
```

### 7.2 Rival patea (defense kickoff)

```
Colocación:
  Ambos robots fuera del círculo central (30cm del centro)
  Delantero: cerca del borde del círculo, listo para interceptar
  Arquero: en posición defensiva

Acción:
  t=0: Señal GO
  t=0.1s: Delantero se mueve hacia la pelota para interceptar
  t=0.5s: Si pelota va hacia nuestro arco → arquero defiende
         Si pelota queda en centro → delantero ataca
```

---

## FUENTES

- RoboCupJunior Soccer Rules 2026 (múltiple defense, pushing, forcing, lack of progress, fouls)
- RoboCupJunior Australia Rules 2026 (multiple defense specifics)
- RoboCupJunior Forum: "Can we change pushing/multiple defense?" (agosto 2025)
- CAMBADA MSL: role assignment, passing, coordinated play
- PCBWay team (2024): aggressive 4-camera strategy
- RoBorregos (2024): OpenMV + striker/goalkeeper roles
- IITA legacy 2025: análisis de 23 deficiencias
