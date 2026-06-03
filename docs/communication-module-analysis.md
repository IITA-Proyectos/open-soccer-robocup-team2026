# Módulo de Comunicación RCJ Soccer — Análisis Completo

## Hardware oficial, capacidades, integración y estrategias avanzadas

**Autor:** Gustavo Viollaz (IITA Salta) con asistencia de Claude Opus 4.6  
**Fecha:** 2026-03-28

---

## 1. QUÉ ES EL MÓDULO

El Soccer Communication Module es un dispositivo oficial provisto por el Soccer League Committee de RoboCupJunior. Cada equipo recibe uno por robot en el mundial. Obligatorio desde Brasil 2025, expandido en 2026.

### Hardware

- **MCU:** ESP32 (WiFi + BLE integrados)
- **Display:** Pantalla que muestra countdown de penalidades
- **Acelerómetro/Giroscopio:** En desarrollo (mencionado en repo oficial como "currently working on")
- **Firmware:** Open source (C/C++), repo oficial: `robocup-junior/soccer-communication-module`
- **Licencia:** Apache-2.0

### Pinout del módulo

| Pin | Función | Descripción |
|-----|---------|------------|
| **BAT+** | Alimentación | Conectar directo a batería (5.3-25V). SIN switch |
| **GND** | Tierra | Conectar a GND de batería |
| **3V3** | Alimentación alternativa | Si la batería no está en rango 5.3-25V |
| **OUT1** | Start/Stop (digital) | **3.3V = GO, 0V = STOP** |
| **OUT2** | Start/Stop (digital) | Igual que OUT1 (redundante) |
| **TX** | UART transmit | Para comunicación inter-robot vía wireless del módulo |
| **RX** | UART receive | Para comunicación inter-robot vía wireless del módulo |
| **LOGV** | Nivel lógico UART | Conectar al voltaje deseado (3.3V-5.5V) |
| **A0** | Canal bit 0 | Para seleccionar canal de comunicación |
| **A1** | Canal bit 1 | Para seleccionar canal de comunicación |

### Canales de comunicación

4 canales disponibles seleccionados con A0 y A1:

| A1 | A0 | Canal |
|:--:|:--:|:--:|
| LOW | LOW | 0 |
| LOW | HIGH | 1 |
| HIGH | LOW | 2 |
| HIGH | HIGH | 3 |

Robots del mismo equipo deben estar en el mismo canal para comunicarse entre sí.

---

## 2. FUNCIONES ACTUALES (2026)

### 2.1 Start/Stop del árbitro (OBLIGATORIO)

La función principal y obligatoria: el árbitro controla el inicio y parada del juego desde una app móvil que se comunica con los módulos.

```
Árbitro (app móvil)
  ↓ (wireless)
Módulo en Robot A  →  OUT1 = 3.3V (GO) o 0V (STOP)
Módulo en Robot B  →  OUT1 = 3.3V (GO) o 0V (STOP)
```

El robot DEBE obedecer esta señal. Si OUT1 = 0V, el robot debe detenerse completamente.

> **Cómo lo recibe el TOP (fix 2026-06-02 / TASK-039):** el árbitro RCJ llega al TOP (Teensy 4.0) como **NIVEL GPIO, no UART**: **pin 5 = OUT1 (PLAY/STOP)** y **pin 6 = OUT2 (espejo de OUT1)**, con `0 = juego PARADO`, `1 = juego EN CURSO (3.3V)`. El firmware (`src/top/comm_arbiter.cpp`, `read_referee_gpio()`) lee ambos pines con `INPUT_PULLDOWN` y toma `match_running = (pin5 O pin6 en alto)` (OR). En PLAY sube SOLO UNO de los dos pines (5 o 6) y el otro queda en 0 (probado en banco 2026-06-02, Gustavo); por eso el AND nunca daba GO y el OR sí. En STOP ambos pines quedan en 0 → OR = STOP. Sigue siendo **fail-safe**: si el cable del COMM se desconecta, ambos pines leen 0 por el `INPUT_PULLDOWN` → `match_running = false` (STOP). El antiguo camino por UART (`COMM_REFEREE_CMD` por el Serial del módulo COMM) quedó **obsoleto**: el UART del COMM (TOP Serial2, pines 7/8) se usa SOLO para partner ESP-NOW / status. El CENTRAL y la strategy NO cambian: siguen consumiendo `referee_cmd` / `match_running` DENTRO del `WORLD_SNAPSHOT` que manda el TOP; lo único que cambió es la FUENTE en el TOP.

### 2.2 Penalización con countdown

Cuando el árbitro penaliza un robot (ej: pushed out, damage), el módulo muestra un countdown en el display. El equipo puede recolocar al robot cuando el timer llega a cero.

### 2.3 Comunicación UART inter-robot (DISPONIBLE)

El módulo puede relayear datos por UART entre robots del mismo canal. Esto permite que el arquero y el delantero se comuniquen sin necesidad de un ESP32 adicional propio.

```
Robot A (Teensy) → UART TX → Módulo A → (wireless) → Módulo B → UART RX → Robot B (Teensy)
```

### 2.4 Score tracking

El árbitro puede registrar goles desde la app. Double click para +1, hold para -1.

---

## 3. LO QUE LAS REGLAS 2026 DICEN

> "Each team will be expected to interface with this module using a single 2.54mm GPIO pin at present to start and stop the robots. **The Soccer League Committee plans on extending this to using UART for more complex applications in future years.**"

Esto confirma que:
- 2026: obligatorio usar OUT1/OUT2 para start/stop (GPIO simple)
- Futuro: planean usar UART para funcionalidades más complejas
- El UART ya está disponible físicamente, solo que aún no es obligatorio

---

## 4. INTEGRACIÓN BÁSICA (MÍNIMO OBLIGATORIO)

### Conexión de hardware

```
Módulo → Robot:
  BAT+  →  Batería + (directo, SIN switch)
  GND   →  Batería -
  OUT1  →  Pin digital del Teensy (ej: pin 34)
  (opcional) LOGV → 3.3V del Teensy
  (opcional) TX → RX del Teensy Serial (para UART inter-robot)
  (opcional) RX → TX del Teensy Serial (para UART inter-robot)
  (opcional) A0, A1 → 3.3V o GND (para seleccionar canal)
```

**CRÍTICO:** El módulo DEBE estar alimentado siempre, incluso cuando el robot está fuera del campo. No poner switch entre batería y módulo.

### Código mínimo (solo start/stop)

```cpp
#define REFEREE_MODULE_PIN 34  // OUT1 del módulo

void setup() {
    pinMode(REFEREE_MODULE_PIN, INPUT);
}

bool game_running() {
    return digitalRead(REFEREE_MODULE_PIN) == HIGH;  // 3.3V = GO
}

void loop() {
    if (!game_running()) {
        stop_all_motors();
        return;  // No hacer NADA hasta que el árbitro dé GO
    }
    // ... lógica del partido ...
}
```

---

## 5. INTEGRACIÓN AVANZADA: UART INTER-ROBOT

### Usar el módulo para comunicación entre propios

En vez de agregar un ESP32 dedicado para WiFi entre robots, se puede usar el UART del módulo oficial. Esto simplifica hardware y es más robusto (el módulo ya tiene conexión estable).

```cpp
// Robot A (Teensy) - enviar datos al compañero
#define MODULE_SERIAL Serial4  // TX4/RX4 conectados a TX/RX del módulo

void setup() {
    MODULE_SERIAL.begin(9600);  // Verificar baud rate del módulo
}

// Enviar estado al compañero
void send_team_data() {
    TeamMessage msg;
    msg.header = 0xAA;  // Sync byte
    msg.role = my_role;
    msg.state = current_state;
    msg.my_x = (int16_t)world.my_x;
    msg.my_y = (int16_t)world.my_y;
    msg.ball_x = (int16_t)world.ball.x[0];
    msg.ball_y = (int16_t)world.ball.x[1];
    msg.ball_conf = world.ball.confidence;
    msg.checksum = calculate_checksum(&msg);

    MODULE_SERIAL.write((uint8_t*)&msg, sizeof(msg));
}

// Recibir datos del compañero
bool receive_team_data(TeamMessage& msg) {
    while (MODULE_SERIAL.available() >= sizeof(TeamMessage)) {
        uint8_t byte = MODULE_SERIAL.read();
        if (byte == 0xAA) {  // Header encontrado
            // Leer resto del mensaje
            uint8_t buf[sizeof(TeamMessage)];
            buf[0] = 0xAA;
            MODULE_SERIAL.readBytes(buf + 1, sizeof(TeamMessage) - 1);
            memcpy(&msg, buf, sizeof(TeamMessage));
            if (verify_checksum(&msg)) return true;
        }
    }
    return false;
}
```

### ⚠️ Limitaciones del UART del módulo

- **Baud rate:** Verificar en firmware del módulo (puede estar fijo)
- **Latencia:** Mayor que ESP-NOW directo (~50-100ms vs ~5-15ms)
- **Ancho de banda:** Limitado, no enviar más de 30 bytes cada 100ms
- **Confiabilidad:** Wireless puede tener pérdida de paquetes
- **4 canales compartidos:** Si otro equipo usa el mismo canal, puede haber interferencia

---

## 6. SUPERTEAM: 5 ROBOTS, CAMPO GRANDE

En SuperTeam, los equipos combinan robots de diferentes países. El campo es más grande ("Big Field") y hay hasta 5 robots por equipo.

### Desafío de coordinación

Con 5 robots de equipos DIFERENTES que nunca practicaron juntos:
- Los robots pueden tener software incompatible
- La comunicación entre robots de diferentes equipos es el mayor desafío
- El módulo oficial es la forma estándar de coordinación

### Protocolo simple para SuperTeam

Si IITA participa en SuperTeam con robots de otro equipo, necesitamos un protocolo mínimo:

```cpp
// Protocolo SuperTeam mínimo via módulo UART
struct SuperTeamMsg {
    uint8_t sync = 0xBB;       // Diferente del msg de equipo normal
    uint8_t robot_id;          // 0-4 (asignado por el árbitro)
    uint8_t role;              // GK, DEF, MID, ATK, FLEX
    int16_t my_x, my_y;       // Mi posición
    int16_t ball_x, ball_y;   // Dónde veo la pelota
    uint8_t ball_confidence;
    uint8_t checksum;
};
// 13 bytes, enviar cada 200ms (5 Hz) para no saturar
```

### Estrategia con módulo para SuperTeam

```
1. Pre-partido: acordar con los otros equipos:
   - Canal del módulo (A0, A1)
   - Roles de cada robot (GK, DEF, MID, ATK, FLEX)
   - Si los robots soportan UART del módulo o no

2. Durante el partido:
   - Si los otros robots NO soportan UART: jugar independientemente
   - Si SÍ soportan: compartir posición y pelota para evitar colisiones

3. Protocolo minimal:
   - Cada robot envía su posición y dónde ve la pelota
   - Cada robot decide independientemente qué hacer
   - La coordinación es "pasiva": evitar ir al mismo lugar
```

---

## 7. WORLD STATE COMPARTIDO: ¿LO USA ALGUIEN?

### Respuesta corta: NO, todavía no

El módulo es demasiado nuevo (2025). Ningún equipo Junior ha implementado un world state compartido a través del módulo. Las razones:

1. El módulo se usó por primera vez en Brasil 2025
2. La mayoría de equipos solo implementó start/stop (GPIO)
3. El UART inter-robot es una feature avanzada que pocos exploraron
4. Los equipos que quieren comunicación inter-robot usan su propio ESP32 (más control, más rápido)

### Lo que SÍ se podría hacer con el módulo

Para IITA 2026, usar el módulo como canal secundario de comunicación:

```
Canal primario: ESP-NOW propio (baja latencia, 10Hz, mundo compartido)
Canal secundario: UART del módulo oficial (backup, SuperTeam)
```

> **Nota (fix 2026-06-02 / TASK-039):** el start/stop del árbitro NO viaja por el UART del módulo. En el TOP el árbitro es **NIVEL GPIO en los pines 5/6** (OUT1/OUT2), no UART; ver la nota en §2.1.

El ESP-NOW propio sigue siendo mejor para el WorldModel completo entre arquero y delantero. Pero el módulo es esencial para:
- Start/stop del árbitro (obligatorio)
- SuperTeam (cuando se juega con robots de otros equipos)
- Backup de comunicación si el ESP-NOW propio falla

---

## 8. FUTURO DEL MÓDULO (2027+)

Las reglas 2026 dicen explícitamente que planean extender el UART para "more complex applications". Posibilidades:

- **Estado del partido:** Enviar score, tiempo restante, evento (gol, penalty, halftime)
- **Posición de árbitro:** El módulo podría enviar instrucciones de reposicionamiento
- **Datos compartidos:** Un estándar oficial para que todos los robots compartan posición
- **Gyro/accel integrados:** El repo oficial menciona que están trabajando en habilitar el MPU del ESP32

Si esto se implementa, el módulo podría convertirse en el estándar para comunicación inter-robot, eliminando la necesidad de ESP32 propios.

---

## 9. RECOMENDACIONES PARA IITA 2026

| Prioridad | Acción | Complejidad |
|:-:|--------|:-:|
| **P0 (obligatorio)** | Conectar OUT1 para start/stop | Trivial |
| **P0 (obligatorio)** | Alimentar módulo directo de batería | Trivial |
| **P1 (recomendado)** | ESP-NOW propio para WorldModel entre robots | Media |
| **P2 (nice to have)** | UART del módulo como backup de comunicación | Baja |
| **P3 (SuperTeam)** | Protocolo SuperTeam minimal via módulo | Media |

---

## FUENTES

- GitHub: `robocup-junior/soccer-communication-module` (firmware, esquemáticos, 3D model)
- RoboCupJunior Soccer Rules 2025: introducción del módulo en Brasil
- RoboCupJunior Soccer Rules 2026: extensión a "at least world championships"
- RoboCupJunior Soccer SuperTeam Rules 2026: 5 robots, Big Field
- RoboCupJunior Forum: "Documentation communication module" (julio 2023)
- RoboCupJunior Forum: "Communication module placement" (junio 2024)
- RoboCupJunior official: "Introducing the RCJ Soccer SuperTeam Communication Module"
