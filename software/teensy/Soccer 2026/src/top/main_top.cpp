// main_top.cpp — Firmware del cerebro sensorial (Teensy 4.0 sobre placa TOP).
//
// Responsabilidad: percibir el mundo y entregar un WORLD_SNAPSHOT al CENTRAL.
// TOP NO toma decisiones tácticas ni controla motores — sólo percibe + fusiona.
//
// Inputs:
//   • 2 cámaras OpenMV (Serial3 + Serial5)
//   • 2 BNO055 IMU (I2C Wire + Wire1 remap 24/25)
//   • 4 ToF + 1 HC-SR04 (I2C dual + GPIO)
//   • Odometría OTOS desde ABAJO (Serial1)
//   • Comm árbitros + partner ESP-NOW (Serial2 7/8 → placa COMM)
//
// Outputs:
//   • WORLD_SNAPSHOT a CENTRAL (Serial4 16/17) a 100 Hz.
//
// Build:
//   pio run -e top_robot1 / top_robot2  (el env exige -DROBOT1 / -DROBOT2)

#include <Arduino.h>

#include "config_top.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"
#include "cameras_runtime.h"   // drena Serial3+Serial5, fusiona front+back
#include "comm_down.h"         // recibe odometría OTOS desde ABAJO
#include "comm_arbiter.h"      // bridge a placa COMM
#include "comm_central.h"      // envía snapshot al CENTRAL
#include "localization_runtime.h"  // fusión TOF+IMU → pose absoluta en cancha
#include "types.h"
#ifdef TOP_DEBUG_TELEMETRY
#include "top_telemetry_serial.h"
#endif

using namespace iitasoccer;

namespace {

elapsedMillis g_since_imu_tick;
elapsedMillis g_since_tof_tick;
elapsedMillis g_since_loc_tick;
elapsedMillis g_since_snapshot;
elapsedMillis g_since_debug;

uint32_t g_loop_count = 0;

// ============================================================================
// WATCHDOG de hardware (P1-WDT, 2026-06-03) — ARDUINO-ONLY, no host-testeable.
// ----------------------------------------------------------------------------
// Problema: sin watchdog, un cuelgue de I2C (BNO o ToF que se traba en una
// transaccion) deja el loop mudo el resto del partido — el robot queda parado
// sin auto-reset (CENTRAL frena por fail-safe, pero el robot no se recupera).
//
// Implementacion: WDOG1 del i.MX RT1062 por registros (imxrt.h, siempre
// disponible via Arduino.h). NO usamos la lib externa WDT_T4 (tonton81) porque
// NO esta vendoreada en lib/ y agregarla via lib_deps forzaria una descarga del
// registry que Avast bloquea (TASK-025). Los registros WDOG1_WCR/WSR del core
// son suficientes y sin dependencias.
//
//   • Timeout = (WT+1) * 0.5 s. WT=1 -> 1.0 s (HOLGADO a proposito, ver abajo).
//   • Service ("feed"): escribir 0x5555 y luego 0xAAAA a WDOG1_WSR.
//   • WDZST=1: el WDT se SUSPENDE en modos low-power/debug (no resetea al depurar).
//
// POR QUE EL TIMEOUT ES HOLGADO (1 s, no menos):
//   El loop normal corre a >100 Hz, pero sensores BLOQUEANTES legitimos pueden
//   robar hasta ~12 ms por ciclo cada tanto: el HC-SR04 (pulseIn, ~12 ms) y el
//   read multi-byte del BNO055 a 100 kHz. Un timeout corto (p.ej. 50-100 ms)
//   resetearia el robot en stalls NORMALES -> reset-loop en pleno partido. 1 s
//   da margen de sobra para cualquier stall legitimo y aun asi reacciona rapido
//   ante un cuelgue REAL de I2C (que es indefinido). El minimo que permite el
//   WDOG1 es 0.5 s; elegimos 1 s para mas holgura.
//
// NOTA Teensy: NO existe Wire.setWireTimeout() en el core IMXRT (es API de AVR/
// ESP; ver WireIMXRT.h — solo setClock). En Teensy 4.0, las transacciones del
// Wire ya tienen un timeout HW interno; el WATCHDOG es el cinturon de seguridad
// real ante un cuelgue del bus que igual trabe el loop. Por eso el WDT es la
// pieza central de P1-WDT (y reemplaza el pedido de setWireTimeout del backlog).
//
// needs_bench: confirmar (1) 0 resets espurios en 30 min de marcha normal con
// los 4 ToF + HC-SR04 + BNO activos; (2) auto-reset al colgar el I2C
// (desconectar un sensor en caliente) — WDOG1_WRSR debe indicar reset por WDT.
constexpr uint8_t WDT_WT_FIELD = 1;  // (1+1)*0.5 s = 1.0 s

void watchdog_init_1s() {
    // Secuencia de configuracion del WDOG1 (una sola escritura al WCR; el WT y los
    // bits de control solo se pueden fijar una vez tras el reset).
    //   WT(1)  -> timeout 1.0 s
    //   WDE    -> habilita el watchdog
    //   WDZST  -> suspende en low-power/debug (no resetea al depurar/dormir)
    // SRS se deja en 1 (no forzamos software reset). WDA idem.
    WDOG1_WCR = WDOG_WCR_WT(WDT_WT_FIELD) | WDOG_WCR_WDE | WDOG_WCR_WDZST |
                WDOG_WCR_SRS | WDOG_WCR_WDA;
    asm volatile("dsb");
}

inline void watchdog_feed() {
    // Refresh ("kick"): la secuencia magica 0x5555 -> 0xAAAA reinicia el contador.
    WDOG1_WSR = 0x5555;
    WDOG1_WSR = 0xAAAA;
}

// Construye el WorldSnapshot a partir de todas las fuentes percibidas.
WorldSnapshot build_snapshot() {
    WorldSnapshot s{};

    // Pose propia — trilateración TOF+IMU del módulo localization (Sprint 1).
    // El runtime cachea el último cómputo; si valid=false (p.ej. <2 TOFs útiles),
    // x/y caen a 0 y confidence=0 para que el CENTRAL sepa ignorar la POSICIÓN.
    auto pose = iitasoccer::localization_runtime_get_pose();
    s.my_x_mm             = pose.x_mm;
    s.my_y_mm             = pose.y_mm;
    s.my_pose_confidence  = pose.valid ? 70 : 0;

    // Heading: SIEMPRE del IMU, desacoplado de la validez de la POSICIÓN.
    // localization_compute() solo escribe pose.heading_centideg dentro del bloque
    // valid (necesita un TOF de eje X y otro de eje Y a la vez); con el hardware
    // actual (TOFs solo en el eje Y) la pose nunca es válida, así que
    // pose.heading_centideg quedaba en 0 y el CENTRAL navegaba con heading=0 fijo.
    // El robot SÍ conoce su orientación (2 BNO055), aunque no su posición. CENTRAL
    // consume el heading sin gatearlo por confidence (ver world_model.cpp), por lo
    // que enviarlo directo del IMU es seguro y estrictamente más correcto.
    s.my_heading_centideg = sensors_imu_get_heading_centideg();

    // Pelota — fusión front+back desde cameras_runtime (sección 7.2 de
    // FIRMWARE-PLACA-ARRIBA.md). Coords relativas al robot en mm.
    s.ball_visible    = cameras_ball_visible() ? 1 : 0;
    s.ball_x_mm       = cameras_get_ball_x_mm();
    s.ball_y_mm       = cameras_get_ball_y_mm();
    s.ball_confidence = cameras_get_ball_confidence();
    // Velocidad de la pelota (mm/s, marco robot). Ahora viaja en el snapshot a
    // CENTRAL (antes entraba 0 fijo). 0 = sin estimación válida.
    // ⚠️ PENDIENTE: CENTRAL todavía NO la consume — falta getter en world_model
    // + llamar bt_classify en strategy. El dato está listo para cablear.
    s.ball_vx_mm_s    = cameras_get_ball_vx_mm_s();
    s.ball_vy_mm_s    = cameras_get_ball_vy_mm_s();

    // Arcos — mapping de colores → opp/own.
    // TODO: este mapping (yellow=opp, blue=own) está hardcoded. La polaridad
    // real depende del lado de cancha asignado por árbitro al inicio del
    // partido. Cuando se integre el comando del árbitro (referee_cmd) con
    // mensaje "play side", revisar y posiblemente invertir. Pendiente Enzo.
    s.goal_opp_visible        = cameras_goal_yellow_visible() ? 1 : 0;
    s.goal_opp_angle_centideg = cameras_get_goal_yellow_angle_centideg();
    s.goal_opp_distance_mm    = cameras_get_goal_yellow_distance_mm();
    // Arco propio (azul) — schema v3. Antes sólo se mandaba la visibilidad y se
    // DESCARTABA el ángulo/distancia que cameras_fusion (fuse_goal_dual) YA computa.
    // Ahora viajan los 3 campos, EXACTAMENTE como el arco rival (mismo patrón):
    // los getters de fusión ya devuelven 0/0 cuando el arco no está visible
    // (ver fuse_goal_dual → else: angle=0, distance=0), que es el MISMO sentinel
    // que goal_opp. El consumidor (CENTRAL) gatea por goal_own_visible.
    s.goal_own_visible        = cameras_goal_blue_visible() ? 1 : 0;
    s.goal_own_angle_centideg = cameras_get_goal_blue_angle_centideg();
    s.goal_own_distance_mm    = cameras_get_goal_blue_distance_mm();

    // Obstáculo más cercano (de ToFs + HC-SR04).
    s.min_obstacle_mm = sensors_tof_get_min_distance_mm();

    // Comando árbitro y flags.
    s.referee_cmd = static_cast<uint8_t>(comm_arbiter_get_last_command());
    uint8_t flags = 0;
    if (comm_arbiter_is_match_running())    flags |= (1 << 3);
    if (comm_arbiter_partner_is_fresh())    flags |= (1 << 1);
    // bit 4 = heading_valid (schema v3, máscara 0x10). El heading SIEMPRE se manda
    // (s.my_heading_centideg viene del IMU), pero este bit le dice al CENTRAL si
    // confiar en él. Criterio: validez de la fusión EN VIVO (no el readiness al boot).
    // Byte-idéntico a (_left_ready||_right_ready) en operación normal, pero si el
    // único BNO sano se cae/congela en runtime (n_use→0; lo dispara el freeze-detector
    // o un miss-counter), fused_valid→false y CENTRAL deja de navegar con un heading
    // muerto. (Audit 2026-06-05 R1.)
    if (sensors_imu_get_heading_valid()) flags |= (1 << 4);
    // bit 0 (in_own_penalty_area) requiere pose absoluta — Nivel 2.
    // bit 2 (partner_sees_ball) requiere parseo del partner snapshot — futuro.
    s.flags = flags;

    return s;
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    delay(100);
    Serial.println("\n=========================================");
    Serial.println("  IITA Soccer Open — TOP firmware");
    Serial.println("  Cerebro sensorial (Teensy 4.0)");
    Serial.println("=========================================");

    // ORDEN CRITICO (fix 2026-06-02, receta validada en diag_pose_live): los 4 VL53L7CX
    // arrancan en 0x29 = misma dir que el BNO DERECHO en el bus Wire. Para que NO choquen:
    //   (1) dormir los ToF (LP low) -> bus limpio;
    //   (2) iniciar los BNO (0x28 + 0x29, sin ToF en el bus);
    //   (3) recien ahi enumerar los ToF (despertar de a uno -> 0x2A..0x2D).
    // Bug anterior: el BNO se iniciaba con los ToF DESPIERTOS en 0x29 -> imu_R=N (o ambos)
    // + enumeracion ToF confundida -> min_obst=65535.
    sensors_tof_predim_lp();  // (1) dormir ToF (LP low) -> bus limpio para el BNO
    sensors_imu_init();       // (2) BNO 0x28 + 0x29 con los ToF dormidos
    sensors_tof_scan_wire();  // DIAG 2026-06-02: con ToF dormidos, que BNO responde? 0x28? 0x29?
    sensors_tof_init();       // (3) enumerar ToF a 0x2A..0x2D
    // OJO: el robot DEBE apuntar al arco rival (+Y) al boot — esta llamada
    // calibra bno_offset_centideg leyendo el heading actual.
    iitasoccer::localization_runtime_init();
    cameras_init();      // Serial3 + Serial5 ← OpenMV front + back
    comm_down_init();    // Serial1 ← odometría desde ABAJO
    comm_arbiter_init(); // Serial2 (7/8) ↔ placa COMM
    comm_central_init(); // Serial4 (16/17) → snapshot a CENTRAL

    // WATCHDOG (P1-WDT): instalar AL FINAL del setup, DESPUES de todos los begin()
    // lentos (los 4 ToF cargan ~85 KB c/u por I2C -> boot ~40 s). Si lo armaramos
    // antes, esa carga legitima dispararia el WDT en pleno arranque. Una vez
    // armado, el loop debe alimentarlo cada ciclo (watchdog_feed) o el robot se
    // reinicia a 1 s. Ver el bloque de notas arriba (timeout holgado + Teensy).
    watchdog_init_1s();

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[TOP] cerebro sensorial listo, enviando snapshots a CENTRAL (WDT 1s armado)");

#ifdef TOP_DEBUG_TELEMETRY
    top_telemetry_init();
#endif
}

void loop() {
    g_loop_count++;

    // === WATCHDOG (P1-WDT): alimentar cada ciclo ===
    // Mientras el loop GIRE, refrescamos el WDOG1. Si el loop se cuelga (p.ej. una
    // transaccion I2C trabada de un sensor), deja de alimentarse y el WDOG1
    // resetea el Teensy a 1 s -> el robot se recupera solo en vez de quedar mudo.
    watchdog_feed();

    // === RX: drenar UARTs (no bloquea) ===
    comm_down_tick();      // odometría OTOS desde ABAJO
    comm_arbiter_tick();   // comm con placa COMM (árbitros + partner)
    comm_central_tick();   // comandos desde CENTRAL (reset, calib)
    cameras_tick();        // OpenMV front (Serial3) + back (Serial5)

    // === Sensores periódicos ===
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
    }
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
    }
    if (g_since_loc_tick >= 33) {  // ~30 Hz — matchea cadencia de los TOFs
        g_since_loc_tick = 0;
        iitasoccer::localization_runtime_tick();
    }

    // === Snapshot → CENTRAL ===
    if (g_since_snapshot >= 10) {  // 100 Hz
        g_since_snapshot = 0;
        WorldSnapshot snap = build_snapshot();
        comm_central_send_snapshot(snap);
    }

#ifdef TOP_DEBUG_TELEMETRY
    top_telemetry_tick();
#endif

    // === Debug ===
    if (g_since_debug >= 500) {
        g_since_debug = 0;
        Serial.print("[TOP] loop=");
        Serial.print(g_loop_count);
        Serial.print(" hdg=");
        Serial.print(sensors_imu_get_heading_deg(), 1);
        Serial.print(" imu_L=");
        Serial.print(sensors_imu_left_ready() ? "Y" : "N");
        Serial.print(" imu_R=");
        Serial.print(sensors_imu_right_ready() ? "Y" : "N");
        Serial.print(" min_obst=");
        Serial.print(sensors_tof_get_min_distance_mm());
        Serial.print(" cam_F/B=");
        Serial.print(cameras_front_alive() ? "Y" : "N");
        Serial.print("/");
        Serial.print(cameras_back_alive() ? "Y" : "N");
        Serial.print(" ball=");
        if (cameras_ball_visible()) {
            Serial.print("(");
            Serial.print(cameras_get_ball_x_mm());
            Serial.print(",");
            Serial.print(cameras_get_ball_y_mm());
            Serial.print(")");
        } else {
            Serial.print("--");
        }
        Serial.print(" pkts_F/B=");
        Serial.print(cameras_packets_front());
        Serial.print("/");
        Serial.print(cameras_packets_back());
        Serial.print(" down_pose/vel=");
        Serial.print(comm_down_is_pose_fresh() ? "Y" : "N");
        Serial.print("/");
        Serial.print(comm_down_is_vel_fresh() ? "Y" : "N");
        // ARBITRO = NIVEL GPIO en pines 5/6 (0=parado, 1=jugando; TASK-039, banco 2026-06-02).
        // ref = comando derivado (0=STOP 1=START), match = habilitado a moverse,
        // p5/6 = lectura cruda de los 2 pines del arbitro, rx = frames partner por UART (Serial2).
        Serial.print(" arb[ref=");
        Serial.print(static_cast<int>(comm_arbiter_get_last_command()));
        Serial.print(" match=");
        Serial.print(comm_arbiter_is_match_running() ? "Y" : "N");
        Serial.print(" p5/6=");
        Serial.print(digitalRead(5));
        Serial.print("/");
        Serial.print(digitalRead(6));
        Serial.print(" rx=");
        Serial.print(comm_arbiter_get_frames_received());
        Serial.print("]");
        Serial.print(" resync=");
        Serial.println(cameras_resyncs_total());
    }
}
