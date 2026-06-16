#include "otos.h"
#include "config_down.h"

#include <Arduino.h>
#include <Wire.h>
#include <cmath>
#include <SparkFun_Qwiic_OTOS_Arduino_Library.h>

#include "otos_health.h"
#include "otos_fusion.h"

// ============================================================================
// SparkFun Qwiic OTOS lib activada 2026-05-24 (TASK-012).
// API: getPosition/getVelocity con sfe_otos_pose2d_t {x, y, h}.
// Unidades seteadas en init: linear=meters, angular=degrees. Convertidos a
// mm y rad/s en otos_tick() para mantener el contrato del resto del firmware.
// Bus físico: U5 → Wire (I²C0, SDA=18 SCL=19); U6 → Wire1 (I²C1, SDA=17 SCL=16).
// (Antes este comentario decía "Wire2 / I2C2" — typo viejo corregido 2026-05-24.)
//
// Salud por OTOS (audit 2026-06-03 #6/#13/#15): el retorno I²C de getPosition/
// getVelocity SÍ se chequea por tick y alimenta una máquina de salud pura
// (otos_health.h). Si un OTOS deja de responder N ticks seguidos, se baja su
// flag de salud (latch) y la confidence emitida cae — CENTRAL deja de creer una
// pose congelada. La recuperación NO es automática: requiere CENTRAL_RESET_OTOS
// o reboot (re-detecta sin recalibrar IMU, ver otos_reset()).
// ============================================================================

namespace iitasoccer {

namespace {

QwiicOTOS g_otos_left;   // U5 → Wire  (I²C0)
QwiicOTOS g_otos_right;  // U6 → Wire1 (I²C1)

// Salud por OTOS (reemplaza los viejos g_left_ready/g_right_ready de solo-boot).
// .bss zero-init → present=false/alive=false; otos_init() los activa al detectar.
OtosHealth g_left_health{};
OtosHealth g_right_health{};

float g_x_mm = 0.0f;
float g_y_mm = 0.0f;
float g_heading_deg = 0.0f;
float g_vx = 0.0f, g_vy = 0.0f, g_omega = 0.0f;
float g_slip = 0.0f;

uint32_t g_tick_count = 0;

// Última pose de cada OTOS (usadas para análisis diferencial).
float g_left_x = 0.0f,  g_left_y = 0.0f,  g_left_h = 0.0f;
float g_right_x = 0.0f, g_right_y = 0.0f, g_right_h = 0.0f;

// Última velocidad de cada OTOS. La lib SparkFun YA está activa (TASK-012,
// 2026-05-24): otos_tick() lee getVelocity() y llena estos campos, y la fusión
// de abajo computa g_vx/g_vy/g_omega con datos reales. (Antes, con la lib
// comentada, quedaban en 0 — bug latente ya cerrado.)
float g_left_vx = 0.0f,  g_left_vy = 0.0f,  g_left_w = 0.0f;
float g_right_vx = 0.0f, g_right_vy = 0.0f, g_right_w = 0.0f;

// Configura unidades + tracking tras una detección exitosa (begin()==true).
// calibrateImu() es BLOQUEANTE (~0.5 s, numSamples=255): se hace SOLO al boot,
// no en la recuperación en caliente (otos_reset re-detecta sin recalibrar).
void otos_setup_units(QwiicOTOS& dev, bool calibrate_imu) {
    dev.setLinearUnit(kSfeOtosLinearUnitMeters);
    dev.setAngularUnit(kSfeOtosAngularUnitDegrees);
    if (calibrate_imu) dev.calibrateImu();
    dev.resetTracking();
}

}  // namespace

bool otos_init() {
    Wire.begin();
    Wire1.begin();
#ifdef DOWN_OTOS_FAST_I2C
    // F2 (banco, §7.2 ARQUITECTURA-LAZO-DOWN-RT): Qwiic Fast-mode 400 kHz (estándar
    // Qwiic) → 4× menos tiempo de bus que los 100 kHz default. El OTOS lo soporta; si
    // en banco diera errores I²C espurios a 400 kHz, bajar a 100 kHz (fail-safe: la
    // salud por-OTOS ya degrada solo ante lecturas fallidas). Gateado off-by-default.
    Wire.setClock(400000);
    Wire1.setClock(400000);
#endif

    oh_reset(g_left_health);
    oh_reset(g_right_health);

    // QwiicOTOS::begin() retorna bool: true si respondió I²C, false si no.
    // Si responde: seteamos unidades a m + deg, calibramos IMU (~0.5 s bloqueante
    // con default numSamples=255), y reseteamos tracking a (0, 0, 0).
    if (NUM_OTOS >= 1) {
        const bool ok = g_otos_left.begin(Wire);
        oh_set_present(g_left_health, ok);
        if (ok) otos_setup_units(g_otos_left, /*calibrate_imu=*/true);
    }
    if (NUM_OTOS >= 2) {
        const bool ok = g_otos_right.begin(Wire1);
        oh_set_present(g_right_health, ok);
        if (ok) otos_setup_units(g_otos_right, /*calibrate_imu=*/true);
    }

    return g_left_health.alive || g_right_health.alive;
}

void otos_tick() {
    g_tick_count++;

    // Lectura I²C de los 2 OTOS. sfe_otos_pose2d_t = {float x, y, h}.
    // Position en metros (m), Velocity en m/s, Heading en grados, ω en deg/s
    // (porque seteamos linear=meters + angular=degrees en otos_init).
    //
    // CHEQUEO DE SALUD (audit #6): capturamos el retorno I²C de AMBAS lecturas.
    // Si cualquiera de las dos falla, esta lectura cuenta como fallo para ese OTOS.
    // Tras N fallos consecutivos oh_update baja `alive` (latch) y la fusión deja
    // de tratar ese OTOS como vivo.
    sfe_otos_pose2d_t pl{}, pr{}, vl{}, vr{};
    bool left_ok = false, right_ok = false;
#ifdef DOWN_OTOS_FAST_I2C
    // F2 (banco, §7.2): UNA transacción de 18 B (pos+vel+acc) por OTOS en vez de DOS
    // (getPosition + getVelocity). Pasa de 4 transacciones I²C a 2; combinado con 400 kHz,
    // ~8× menos bloqueo (~3-4 ms → ~0.4-0.6 ms, por debajo de 1 tick de línea). Fail-safe
    // IDÉNTICO al de hoy: una lectura fallida deja {left,right}_ok=false → NO se propaga la
    // pose (misma degradación, no hay teleport-al-origen). El `acc` se lee y se descarta.
    sfe_otos_pose2d_t al{}, ar{};
    if (g_left_health.alive)  left_ok  = (g_otos_left.getPosVelAcc(pl, vl, al)  == ksfTkErrOk);
    if (g_right_health.alive) right_ok = (g_otos_right.getPosVelAcc(pr, vr, ar) == ksfTkErrOk);
#else
    if (g_left_health.alive) {
        const bool p_ok = (g_otos_left.getPosition(pl) == ksfTkErrOk);
        const bool v_ok = (g_otos_left.getVelocity(vl) == ksfTkErrOk);
        left_ok = p_ok && v_ok;
    }
    if (g_right_health.alive) {
        const bool p_ok = (g_otos_right.getPosition(pr) == ksfTkErrOk);
        const bool v_ok = (g_otos_right.getVelocity(vr) == ksfTkErrOk);
        right_ok = p_ok && v_ok;
    }
#endif
    const bool left_alive  = oh_update(g_left_health, left_ok);
    const bool right_alive = oh_update(g_right_health, right_ok);

    // Conversión al contrato del firmware (mm, deg, rad/s). Sólo actualizamos las
    // poses de un OTOS si su lectura de ESTE tick fue OK — una lectura fallida deja
    // (0,0,0) en pl/pr y NO la propagamos (evita el teleport-al-origen del audit #6).
    if (left_ok) {
        g_left_x  = pl.x * 1000.0f;  g_left_y  = pl.y * 1000.0f;  g_left_h  = pl.h;
        g_left_vx = vl.x * 1000.0f;  g_left_vy = vl.y * 1000.0f;  g_left_w  = vl.h * (M_PI / 180.0f);
    }
    if (right_ok) {
        g_right_x  = pr.x * 1000.0f; g_right_y  = pr.y * 1000.0f; g_right_h  = pr.h;
        g_right_vx = vr.x * 1000.0f; g_right_vy = vr.y * 1000.0f; g_right_w  = vr.h * (M_PI / 180.0f);
    }

    // === Fusión === (usa la salud ACTUAL, no los flags de boot)
    if (left_alive && right_alive) {
        // Pose del centro = promedio de los 2.
        g_x_mm = (g_left_x + g_right_x) * 0.5f;
        g_y_mm = (g_left_y + g_right_y) * 0.5f;

        // Heading: promedio VECTORIAL de los headings absolutos de las 2 IMUs
        // (audit #7). El cálculo viejo atan2(dy_acumulado, separación) saturaba en
        // ±90° y sólo valía para giros chicos; el OTOS ya integra heading absoluto.
        g_heading_deg = fuse_dual_heading_deg(g_left_h, g_right_h);

        // Velocidad del centro = promedio de los 2 (la lib SparkFun ya está
        // activa, así que estos son valores reales de getVelocity()).
        g_vx = (g_left_vx + g_right_vx) * 0.5f;
        g_vy = (g_left_vy + g_right_vy) * 0.5f;
        g_omega = (g_left_w + g_right_w) * 0.5f;

        // Slip = exceso de diferencia de vx entre los OTOS por encima de lo
        // esperable por la rotación (audit #20). Sobre VELOCIDADES, no posiciones.
        g_slip = otos_slip_estimate(g_left_vx, g_right_vx, g_omega,
                                    OTOS_SEPARATION_MM * 0.5f);
    } else if (left_alive) {
        g_x_mm = g_left_x; g_y_mm = g_left_y; g_heading_deg = g_left_h;
        g_vx = g_left_vx; g_vy = g_left_vy; g_omega = g_left_w;
        g_slip = 0.0f;
    } else if (right_alive) {
        g_x_mm = g_right_x; g_y_mm = g_right_y; g_heading_deg = g_right_h;
        g_vx = g_right_vx; g_vy = g_right_vy; g_omega = g_right_w;
        g_slip = 0.0f;
    } else {
        // Ningún OTOS vivo — degradación: pose no se actualiza, slip 0.
        g_slip = 0.0f;
    }
}

float otos_get_x_mm()           { return g_x_mm; }
float otos_get_y_mm()           { return g_y_mm; }
float otos_get_heading_deg()    { return g_heading_deg; }
float otos_get_vx_mm_s()        { return g_vx; }
float otos_get_vy_mm_s()        { return g_vy; }
float otos_get_omega_rad_s()    { return g_omega; }
float otos_get_slip_estimate()  { return g_slip; }

void otos_reset() {
    g_x_mm = g_y_mm = g_heading_deg = 0.0f;
    g_vx = g_vy = g_omega = g_slip = 0.0f;

    // Recuperación en caliente (audit #13): si un OTOS que se detectó al boot
    // está caído (alive=false por fallos I²C), intentamos re-detectarlo con
    // begin() — es un ping + lectura de product ID (NO bloqueante, sin recalibrar
    // IMU). Si responde, re-armamos su salud y reseteamos tracking. Esto permite
    // recuperar un OTOS que se desconectó/volvió SIN power-cycle del firmware.
    if (g_left_health.present && !g_left_health.alive) {
        if (g_otos_left.begin(Wire)) {
            otos_setup_units(g_otos_left, /*calibrate_imu=*/false);
            oh_set_present(g_left_health, true);
        }
    } else if (g_left_health.alive) {
        g_otos_left.resetTracking();
    }

    if (g_right_health.present && !g_right_health.alive) {
        if (g_otos_right.begin(Wire1)) {
            otos_setup_units(g_otos_right, /*calibrate_imu=*/false);
            oh_set_present(g_right_health, true);
        }
    } else if (g_right_health.alive) {
        g_otos_right.resetTracking();
    }
}

bool     otos_is_left_ready()   { return g_left_health.alive; }
bool     otos_is_right_ready()  { return g_right_health.alive; }
uint32_t otos_get_tick_count()  { return g_tick_count; }

#ifdef DOWN_USB_MONITOR
// Lecturas por-OTOS (sin fusionar) — exponen los globals que otos_tick() ya
// mantiene para el análisis diferencial. Solo en telemetría/monitor USB.
float otos_get_left_x_mm()         { return g_left_x; }
float otos_get_left_y_mm()         { return g_left_y; }
float otos_get_left_heading_deg()  { return g_left_h; }
float otos_get_right_x_mm()        { return g_right_x; }
float otos_get_right_y_mm()        { return g_right_y; }
float otos_get_right_heading_deg() { return g_right_h; }
#endif

}  // namespace iitasoccer
