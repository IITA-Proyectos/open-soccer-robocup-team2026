// config_zircon.h — Constantes del firmware del Zircon (motor server)
//
// El Zircon Rev v15 con Teensy 4.1 ejecuta el firmware "motor server":
//   - Recibe MotorCommand por Serial1 desde la placa TOP.
//   - Aplica cinemática inversa omni-3.
//   - Aplica PWM a los 3 drivers H-bridge.
//   - Reporta status básico al TOP.
//
// Selección de robot por compilación:
//   pio run -e zircon_robot1   → arquero (#define ROBOT1)
//   pio run -e zircon_robot2   → delantero (#define ROBOT2)
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
#else
    #error "Debe definirse ROBOT1 (arquero) o ROBOT2 (delantero) en build_flags"
#endif

// ============================================================
// BNO055 (idéntico en ambos robots)
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
// UART hacia TOP
// ============================================================
constexpr long  UART_TOP_BAUD   = 230400;
constexpr int   UART_TOP_RX     = 0;  // Serial1 RX1
constexpr int   UART_TOP_TX     = 1;  // Serial1 TX1

// ============================================================
// Watchdog
// ============================================================
// Si no llega un MotorCommand del TOP en este tiempo, los motores se detienen
// para evitar que el robot quede a velocidad fija si el TOP se cuelga o se desconecta.
constexpr uint32_t COMMAND_TIMEOUT_MS = 200;

// ============================================================
// LED de estado
// ============================================================
constexpr int PIN_LED_STATUS = 13;  // LED_BUILTIN del Teensy 4.1

}  // namespace iitasoccer
