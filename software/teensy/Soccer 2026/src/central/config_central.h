// config_central.h — Constantes del firmware del Zircon (motor server)
//
// El Zircon Rev v15 con Teensy 4.1 ejecuta el firmware "motor server":
//   - Recibe WORLD_SNAPSHOT por Serial7 (pin 28) desde la placa TOP.
//   - Aplica cinemática inversa omni-3.
//   - Aplica PWM a los 3 drivers H-bridge.
//   - Reporta status básico al TOP.
//
// Selección de robot por compilación:
//   pio run -e central_robot1   → arquero (#define ROBOT1)
//   pio run -e central_robot2   → delantero (#define ROBOT2)
//
// Pinout basado en hardware/electronics/mapa-pines-teensy-ambos-robots.md
// (documento del 2026-03-20). NO MODIFICAR sin coordinar con Enzo + verificar
// con el journal 2026-03-20-diferencias-pines-motores-arquero-delantero.md.

#pragma once

namespace iitasoccer {

// ============================================================
// Pinout de motores — depende del robot
// ============================================================
#if defined(ROBOT1)  // Arquero
    constexpr int PIN_INA1 = 2;
    constexpr int PIN_INB1 = 5;
    constexpr int PIN_PWM1 = 3;

    constexpr int PIN_INA2 = 8;
    constexpr int PIN_INB2 = 7;
    constexpr int PIN_PWM2 = 6;

    constexpr int PIN_INA3 = 11;
    constexpr int PIN_INB3 = 12;
    constexpr int PIN_PWM3 = 4;

    // Sentido por motor (+1 normal, -1 invertido por hardware).
    // VALIDADO en banco (María/Elías, diag_central_line_sweep_robot1): el motor 2
    // (driver U17, pines 8/7/6) tiene INA/INB cruzados por HW → va invertido.
    // Fuente: docs/firmware/DIAG-CENTRAL-MOTORS.md + journal 2026-05-29 / 2026-06-01.
    constexpr int MOTOR_INVERT[3] = { +1, -1, +1 };
#elif defined(ROBOT2)  // Delantero
    constexpr int PIN_INA1 = 8;
    constexpr int PIN_INB1 = 7;
    constexpr int PIN_PWM1 = 6;

    constexpr int PIN_INA2 = 11;
    constexpr int PIN_INB2 = 12;
    constexpr int PIN_PWM2 = 4;

    constexpr int PIN_INA3 = 2;
    constexpr int PIN_INB3 = 5;
    constexpr int PIN_PWM3 = 3;

    // Sentido por motor — por ahora IGUAL que ROBOT1 (decisión 2026-06-03).
    // ⚠️ FALTA VERIFICAR EN BANCO si el delantero se comporta igual que el arquero:
    // correr diag_central_motors en el delantero y confirmar si esto es así o no.
    // OJO: en el delantero los pines están ROTADOS → el índice 1 de este array es
    // el driver U7 (11/12/4), NO el U17. Si la inversión real es del driver U17
    // (que en el delantero es el índice 0), el array correcto sería { -1, +1, +1 }.
    // El banco lo dirime.
    constexpr int MOTOR_INVERT[3] = { +1, -1, +1 };  // = ROBOT1, sin validar en delantero
#else
    #error "Debe definirse ROBOT1 (arquero) o ROBOT2 (delantero) en build_flags"
#endif

// ============================================================
// BNO055 — ⚠️ YA NO SE CONECTA EN CENTRAL (2026-05-31).
// Los 2 BNO están en el TOP; el heading llega por WORLD_SNAPSHOT de ARRIBA. Estas
// constantes + el módulo imu_zircon quedan como compat: solo se usan si se compila
// con -DCENTRAL_HAS_LOCAL_BNO (ver main_central.cpp). Default: OFF.
// ============================================================
constexpr int    BNO055_I2C_ADDR    = 0x28;
constexpr int    BNO055_INIT_TIMEOUT_MS = 3000;
constexpr int    BNO055_STABILIZE_MS    = 1000;
constexpr int    BNO055_GYRO_CALIB_MS   = 2000;
constexpr int    BNO055_HEADING_SAMPLES = 10;

// ============================================================
// Cinemática del robot (TENTATIVO — confirmar con montaje físico real)
// ============================================================
constexpr float PI_F            = 3.14159265358979323846f;
constexpr float WHEEL_RADIUS_MM = 100.0f;   // distancia del centro a cada rueda

// Configuración tentativa: ruedas a 60°, -60°, 180°. Convención frente = +Y.
// ⚠️ Confirmar con Enzo + medir en el robot armado.
constexpr float WHEEL_ANGLES_DEG[3] = { 60.0f, -60.0f, 180.0f };

// ============================================================
// Control de motores
// ============================================================
constexpr int MAX_PWM           = 255;    // Arduino analogWrite range
constexpr float MAX_SPEED_MM_S  = 1000.0f; // velocidad máxima estimada del robot

// ============================================================
// UARTs inter-placa (reasignados 2026-05-31 — ver MAPA-CONEXIONES-3-PLACAS.md)
//   • TOP→CENTRAL  (WORLD_SNAPSHOT): Serial7  RX7=pin 28, TX7=pin 29
//   • DOWN→CENTRAL (LINE_URGENT):    Serial1  RX1=pin 0,  TX1=pin 1
//   • Serial2 (7/8) queda LIBRE para el driver del motor 2 (U17) → conflicto F8 RESUELTO.
// ============================================================
constexpr long  UART_TOP_BAUD   = 230400;
// #17: las 4 constantes de pin de abajo son INFORMATIVAS (mapa de cableado). En
// Teensy 4.1 los pines de Serial7/Serial1 son FIJOS — comm_top.cpp/comm_down.cpp
// llaman Serial7.begin()/Serial1.begin() sin pasar pines, así que editar estos
// números acá NO reasigna nada. Si cambia el cableado, se cambia de Serial, no el pin.
constexpr int   UART_TOP_RX     = 28;  // Serial7 RX7 (TOP→CENTRAL)  [informativo]
constexpr int   UART_TOP_TX     = 29;  // Serial7 TX7                [informativo]
constexpr int   UART_DOWN_RX    = 0;   // Serial1 RX1 (DOWN→CENTRAL) [informativo]
constexpr int   UART_DOWN_TX    = 1;   // Serial1 TX1                [informativo]

// ============================================================
// Watchdog
// ============================================================
// El watchdog REAL del snapshot vive en world_model.cpp (SNAPSHOT_TIMEOUT_MS = 500 ms):
// si no llega WORLD_SNAPSHOT del TOP en ese tiempo, main_central frena los motores.
// (#15: se removió COMMAND_TIMEOUT_MS=200 — era código muerto + heredado del rol viejo
//  de "motor-server"; la CENTRAL ya NO recibe MotorCommand del TOP, arma el comando local.)
//
// merge 2026-06-03: el bloque KICKER que traía la rama del agente NO se reintegra
// — el robot NO tiene kicker físico (ya removido en main, TASK-011 cancelada). No
// reintroducir PIN_KICKER_SOL / KICKER_*.

// ============================================================
// Arranque manual fail-safe (F3) — SOLO banco, gateado por CENTRAL_ENABLE_MANUAL_START
// ============================================================
// Pulsador de arranque para cuando la placa COMM no manda START (bench testing).
// ⚠️ Pin 9 ASUMIDO de diag_central_motors — CONFIRMAR que hay un pulsador cableado
// al pin 9 (INPUT_PULLUP) en el Zircon; si no, usar ENTER por USB (backup). NUNCA
// activar este flag en competencia (arrancar sin árbitro viola el protocolo RCJ).
constexpr int PIN_MANUAL_START_BUTTON = 9;

// ============================================================
// LED de estado
// ============================================================
constexpr int PIN_LED_STATUS = 13;  // LED_BUILTIN del Teensy 4.1

}  // namespace iitasoccer
