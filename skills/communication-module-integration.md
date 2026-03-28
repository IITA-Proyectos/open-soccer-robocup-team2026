# 📡 Integración del Módulo de Comunicación RCJ Soccer

## Skill: start/stop, UART inter-robot, SuperTeam

---

## 1. MÍNIMO OBLIGATORIO: START/STOP

```cpp
// ============================================================
// Módulo de Comunicación RCJ Soccer — Integración
// ============================================================

#define MODULE_OUT1 34      // Pin conectado a OUT1 del módulo
#define MODULE_SERIAL Serial4  // UART conectado a TX/RX del módulo

class RCJModule {
public:
    void begin() {
        pinMode(MODULE_OUT1, INPUT);
        // UART opcional para inter-robot
        MODULE_SERIAL.begin(9600);  // Verificar baud del módulo
    }

    // ===== START/STOP (obligatorio) =====

    bool game_running() {
        return digitalRead(MODULE_OUT1) == HIGH;
    }

    // Esperar que el árbitro dé start
    void wait_for_start() {
        while (!game_running()) {
            // LED rojo = esperando
            set_led(RED);
            delay(50);
        }
        set_led(GREEN);  // GO!
    }

    // ===== COMUNICACIÓN INTER-ROBOT (opcional) =====

    // Mensaje compacto para compartir entre robots del mismo equipo
    struct TeamMsg {
        uint8_t  sync;          // 0xAA
        uint8_t  role;          // GOALKEEPER=0, STRIKER=1
        uint8_t  state;         // FSM state
        int16_t  my_x, my_y;   // Mi posición (mm)
        int16_t  my_heading;    // Décimas de grado
        int16_t  ball_x, ball_y; // Pelota en campo (mm)
        uint8_t  ball_conf;     // 0-100
        uint8_t  checksum;      // XOR de todos los bytes
    };  // 14 bytes

    bool send_to_partner(TeamMsg& msg) {
        msg.sync = 0xAA;
        msg.checksum = compute_checksum(&msg);
        return MODULE_SERIAL.write((uint8_t*)&msg, sizeof(msg)) == sizeof(msg);
    }

    bool receive_from_partner(TeamMsg& msg) {
        while (MODULE_SERIAL.available() > 0) {
            uint8_t b = MODULE_SERIAL.peek();
            if (b != 0xAA) {
                MODULE_SERIAL.read();  // Descartar basura
                continue;
            }
            if (MODULE_SERIAL.available() >= (int)sizeof(TeamMsg)) {
                MODULE_SERIAL.readBytes((uint8_t*)&msg, sizeof(TeamMsg));
                if (msg.sync == 0xAA && verify_checksum(&msg)) {
                    return true;
                }
            }
            break;
        }
        return false;
    }

private:
    uint8_t compute_checksum(TeamMsg* m) {
        uint8_t* p = (uint8_t*)m;
        uint8_t cs = 0;
        for (int i = 0; i < (int)sizeof(TeamMsg) - 1; i++) cs ^= p[i];
        return cs;
    }
    bool verify_checksum(TeamMsg* m) {
        return m->checksum == compute_checksum(m);
    }
};
```

---

## 2. USO EN EL LOOP PRINCIPAL

```cpp
RCJModule rcj;
WorldModel world;

void setup() {
    rcj.begin();
    // ... init motores, IMU, cámaras ...
    rcj.wait_for_start();  // Bloquea hasta que el árbitro dé GO
}

void loop() {
    // SIEMPRE verificar start/stop primero
    if (!rcj.game_running()) {
        stop_all_motors();
        return;
    }

    // ... update sensores, odometría ...

    // Enviar mi estado al compañero cada 100ms
    static uint32_t last_send = 0;
    if (millis() - last_send > 100) {
        RCJModule::TeamMsg msg;
        msg.role = my_role;
        msg.state = current_state;
        msg.my_x = (int16_t)world.my_x;
        msg.my_y = (int16_t)world.my_y;
        msg.my_heading = (int16_t)(world.my_heading * 10);
        msg.ball_x = (int16_t)world.ball.x[0];
        msg.ball_y = (int16_t)world.ball.x[1];
        msg.ball_conf = (uint8_t)world.ball.confidence;
        rcj.send_to_partner(msg);
        last_send = millis();
    }

    // Recibir datos del compañero
    RCJModule::TeamMsg partner_msg;
    if (rcj.receive_from_partner(partner_msg)) {
        world.process_partner_message(
            partner_msg.my_x, partner_msg.my_y,
            partner_msg.my_heading / 10.0f,
            partner_msg.ball_x, partner_msg.ball_y,
            partner_msg.ball_conf, partner_msg.state);
    }

    // ... FSM, estrategia ...
}
```

---

## 3. CANALES (A0, A1)

```cpp
// Configurar canal del módulo
// Conectar A0 y A1 a pines digitales del Teensy para cambiar canal
#define MODULE_A0 35
#define MODULE_A1 36

void set_module_channel(uint8_t channel) {
    // channel: 0, 1, 2, o 3
    pinMode(MODULE_A0, OUTPUT);
    pinMode(MODULE_A1, OUTPUT);
    digitalWrite(MODULE_A0, channel & 0x01);
    digitalWrite(MODULE_A1, (channel >> 1) & 0x01);
}

// En setup:
set_module_channel(0);  // Canal 0 para nuestro equipo
```

Para SuperTeam, acordar canal con los otros equipos del SuperTeam.

---

## 4. CONEXIÓN ELÉCTRICA

```
        ┌─────────────────┐
        │  MÓDULO RCJ     │
        │                 │
BATERÍA─┤BAT+         OUT1├──── Pin 34 (Teensy)
     ───┤GND          OUT2├──── (opcional, redundante)
        │              TX ├──── RX4 Pin 31 (Teensy)
        │              RX ├──── TX4 Pin 32 (Teensy)
   3.3V─┤LOGV          A0├──── Pin 35 (o 3.3V/GND fijo)
        │              A1├──── Pin 36 (o 3.3V/GND fijo)
        └─────────────────┘

⚠️ BAT+ va DIRECTO a la batería. Sin switch, sin regulador.
   El módulo tiene su propio regulador interno.
   Rango: 5.3V a 25V (LiPo 2S=7.4V o 3S=11.1V: OK)
   Consumo: hasta 500mA
```

---

## 5. SUPERTEAM: PROTOCOLO COMPATIBLE

Para SuperTeam con robots de otros equipos, usar un protocolo más simple:

```cpp
struct SuperTeamMsg {
    uint8_t  sync = 0xBB;    // 0xBB para distinguir de TeamMsg
    uint8_t  robot_id;       // 0-4 (asignado por árbitro)
    int16_t  my_x, my_y;    // Posición (mm)
    int16_t  ball_x, ball_y; // Pelota (mm)
    uint8_t  ball_conf;
    uint8_t  checksum;
};  // 11 bytes, enviar cada 200ms
```

Compartir esta estructura con los otros equipos del SuperTeam antes del partido.

---

## 6. TABLA DE DECISIÓN: MÓDULO vs ESP-NOW PROPIO

| Aspecto | Módulo RCJ | ESP-NOW propio |
|---------|:-:|:-:|
| Start/stop árbitro | **Obligatorio** | No puede |
| Latencia | ~50-100ms | **~5-15ms** |
| Ancho de banda | Limitado | **Amplio** |
| Confiabilidad | Buena | **Excelente** |
| SuperTeam compatible | **Sí** | Solo con propios |
| Hardware extra | Ninguno (provisto) | ESP32 en cada robot |
| Canales | 4 (compartidos) | **250** |
| Costo | Gratis | ~$5 por ESP32 |

**Recomendación:** Usar AMBOS. Módulo para start/stop + SuperTeam. ESP-NOW propio para WorldModel a 10Hz.

---

## FUENTES

- GitHub: `robocup-junior/soccer-communication-module`
- RoboCupJunior Soccer Rules 2026
- RoboCupJunior Soccer SuperTeam Rules 2026
- Ver docs/communication-module-analysis.md para análisis completo
- Ver skills/multi-camera-world-model.md para WorldModel
