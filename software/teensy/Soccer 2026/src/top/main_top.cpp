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
#include "top_eeprom_config.h" // config persistente de sensores (A2.1, g_top_cfg)
#include "localization_runtime.h"  // fusión TOF+IMU → pose absoluta en cancha
#include "types.h"
#include "goal_polarity.h"         // autodetección color arco rival/propio
#include "snapshot_emitter.h"      // emisor @100Hz desacoplado por timer (GATEADO -DTOP_ENABLE_SNAPSHOT_TIMER)

// === Estimador de pose fusionada (B3 RT 2026-06-15, GATEADO off-by-default) ===
// Cablea pose_fusion (complementario ToF+OTOS) + pose_filter (suavizado + gate de
// salto) detrás de TOP_ENABLE_POSE_FUSION. Default OFF → build_snapshot usa la pose
// de localization tal cual hoy (byte-idéntico). Ver doc ESTIMACION-FUSION-TOP.md.
//
// PRE-REQUISITO DURO (pedido explícito de Gustavo): el HEADING es la RAÍZ. Un rumbo
// congelado/malo rota TODO el mapa, y la corrección ToF de pose_fusion ancla en el
// lugar equivocado. Por eso pose_fusion NO COMPILA sin el detector de BNO congelado
// (TOP_ENABLE_BNO_FREEZE_DETECT) activo. Si ves este #error: activá primero el freeze
// detector y validalo en banco (que no dé falsos-DEAD con el robot quieto) ANTES de
// prender la fusión de pose.
#ifdef TOP_ENABLE_POSE_FUSION
#if !defined(TOP_ENABLE_BNO_FREEZE_DETECT)
#error "TOP_ENABLE_POSE_FUSION requiere TOP_ENABLE_BNO_FREEZE_DETECT: el heading es la raiz; un rumbo congelado rota todo el mapa. Activa primero el freeze-detector (y validalo en banco)."
#endif
#include "pose_fusion.h"
#include "pose_filter.h"
#include "pose_age.h"      // gating fino de frescura del OTOS (INC-2)
#endif

#ifdef TOP_ENABLE_HEADING_XVAL
#include "vel_lateral.h"   // gate anti-strafe de la cámara para la cross-validación (TASK-213)
#endif
#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)
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
// [[maybe_unused]]: con -DTOP_ENABLE_SNAPSHOT_TIMER el emit lo hace la ISR del timer
// (snapshot_emitter), así que el loop NO llama build_snapshot() y queda sin uso bajo ese
// flag. El atributo evita el -Wunused-function sin tocar el comportamiento por defecto.
[[maybe_unused]] WorldSnapshot build_snapshot() {
    WorldSnapshot s{};

    // Pose propia — trilateración TOF+IMU del módulo localization (Sprint 1).
    // El runtime cachea el último cómputo; si valid=false (p.ej. <2 TOFs útiles),
    // x/y caen a 0 y confidence=0 para que el CENTRAL sepa ignorar la POSICIÓN.
    auto pose = iitasoccer::localization_runtime_get_pose();
#ifdef TOP_ENABLE_POSE_FUSION
    // Fusión complementaria ToF(absoluto) + OTOS(odometría) + suavizado/gate de salto.
    // SEGURIDAD POR DISEÑO: mientras la fusión NO esté anclada (initialized=false →
    // pfo.valid=false), que es el caso de HOY (el ToF casi nunca da valid: sólo hay
    // ToF en el eje Y), caemos al comportamiento EXACTO de localization → byte-idéntico
    // hasta que el ToF ancle de verdad, que es justo cuando la fusión empieza a aportar.
    // Heading: NO se fusiona acá (sigue saliendo del BNO en s.my_heading_centideg abajo).
    {
        static PoseFusionState  s_pf{};
        static PoseFilterState  s_pflt{};
        static PoseFusionConfig s_pfcfg   = pose_fusion_default_config();
        static PoseFilterConfig s_pfltcfg = pose_filter_default_config();
        static bool             s_pf_ready = false;
        static elapsedMillis    s_pf_dt;
        if (!s_pf_ready) {
            pose_fusion_reset(s_pf);
            pose_filter_init(s_pflt);
            s_pf_ready = true;
        }

        const Pose2D& otos = comm_down_get_pose();
        PoseFusionInputs in{};
        in.tof_x_mm  = pose.x_mm;
        in.tof_y_mm  = pose.y_mm;
        in.tof_valid = pose.valid;
        in.otos_x_mm = otos.x_mm;   // OTOS absoluto acumulado; el módulo usa el DELTA internamente
        in.otos_y_mm = otos.y_mm;   // ⚠️ BANCO: validar signo/eje del delta OTOS vs marco de cancha
        // Gating FINO (INC-2): rechaza OTOS más viejo que otos_stale_ms (≈60 ms) — a 100 Hz
        // un OTOS sano llega cada ~10 ms; predecir contra un delta de 200 ms es basura. El
        // booleano is_pose_fresh sólo corta a 500 ms. Nunca-recibido => edad MAX => no fresco.
        in.otos_fresh = pose_age_is_fresh(comm_down_pose_age_ms(), s_pfcfg.otos_stale_ms);
        in.bno_heading_centideg = sensors_imu_get_heading_centideg();  // pass-through (no fusiona heading)
        in.otos_heading_centideg = otos.heading_centideg;   // H1: des-rotar el delta OTOS al marco de cancha
        in.heading_valid = sensors_imu_get_heading_valid();  // H3: no anclar/corregir con heading muerto/congelado
        in.dt_ms = static_cast<uint16_t>(static_cast<uint32_t>(s_pf_dt));
        s_pf_dt = 0;

        PoseFusionOutput pfo = pose_fusion_update(s_pf, in, s_pfcfg);
        if (pfo.valid) {
            // pose_filter como red de salto/suavizado sobre la pose ya fusionada.
            FilterPose meas{};
            meas.x_mm             = pfo.x_mm;
            meas.y_mm             = pfo.y_mm;
            meas.heading_centideg = pfo.heading_centideg;
            meas.confidence       = pfo.confidence;
            FilterPose filt = pose_filter_update(s_pflt, s_pfltcfg, meas, true);
            s.my_x_mm            = filt.x_mm;
            s.my_y_mm            = filt.y_mm;
            s.my_pose_confidence = filt.confidence;
        } else {
            // Fusión aún no anclada → comportamiento IDÉNTICO a hoy.
            s.my_x_mm            = pose.x_mm;
            s.my_y_mm            = pose.y_mm;
            s.my_pose_confidence = pose.valid ? 70 : 0;
        }
    }
#else
    s.my_x_mm             = pose.x_mm;
    s.my_y_mm             = pose.y_mm;
    s.my_pose_confidence  = pose.valid ? 70 : 0;
#endif

    // Heading: SIEMPRE del IMU, desacoplado de la validez de la POSICIÓN.
    // localization_compute() solo escribe pose.heading_centideg dentro del bloque
    // valid (necesita ≥2 TOFs útiles, 1 por eje). HISTÓRICO: en mayo/junio el HW
    // tenía solo 2 ToFs en eje Y → pose nunca válida. ESTADO 2026-06-17 (Gustavo):
    // los 4 ToFs (frente/atrás/derecha/izquierda) están soldados y operativos en R1
    // y R2 → cubren X+Y → localization PUEDE anclar. Encender la fusión completa con
    // -DTOP_ENABLE_POSE_FUSION (env top_robot2_pri_posefusion). Banco pendiente:
    // validar signo/eje OTOS vs cancha + ruido ToF + heading_valid sin falsos.
    // El robot SÍ conoce su orientación (2 BNO055) AUNQUE la trilateración no haya
    // anclado, por eso el heading del BNO se envía siempre, independiente de pose.
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

    // Arcos — mapping de colores → opp/own, AUTODETECTADO por las cámaras.
    // El arco que el robot tiene AL FRENTE es el RIVAL; el de ATRÁS, el PROPIO
    // (goal_polarity.h). Se infiere de la visibilidad+ángulo fusionado y se FIJA
    // con un latch anti-rebote (estable todo el partido).
    // FAIL-SAFE: hasta que el latch confirme con certeza, se usa el default previo
    // (YELLOW_IS_OPP) → comportamiento IDÉNTICO al mapeo hardcoded de antes.
    // El robot DEBE arrancar mirando a la cancha (frente al arco rival).
    static GoalPolarityLatch g_goal_pol{};   // value-init = UNKNOWN (= sin fijar)
    const bool   yv = cameras_goal_yellow_visible();
    const bool   bv = cameras_goal_blue_visible();
    const float  ya = cameras_get_goal_yellow_angle_centideg() / 100.0f;
    const float  ba = cameras_get_goal_blue_angle_centideg() / 100.0f;
    goal_polarity_latch_update(g_goal_pol, goal_polarity_infer(yv, ya, bv, ba));
    const bool blue_is_opp =
        goal_polarity_effective(g_goal_pol, GoalPolarity::YELLOW_IS_OPP)
            == GoalPolarity::BLUE_IS_OPP;

    // RIVAL (al que atacamos) y PROPIO (el que defendemos), según la polaridad.
    if (blue_is_opp) {
        s.goal_opp_visible        = cameras_goal_blue_visible() ? 1 : 0;
        s.goal_opp_angle_centideg = cameras_get_goal_blue_angle_centideg();
        s.goal_opp_distance_mm    = cameras_get_goal_blue_distance_mm();
        s.goal_own_visible        = cameras_goal_yellow_visible() ? 1 : 0;
        s.goal_own_angle_centideg = cameras_get_goal_yellow_angle_centideg();
        s.goal_own_distance_mm    = cameras_get_goal_yellow_distance_mm();
    } else {
        // Default / fail-safe: amarillo=rival, azul=propio (mapeo previo).
        s.goal_opp_visible        = cameras_goal_yellow_visible() ? 1 : 0;
        s.goal_opp_angle_centideg = cameras_get_goal_yellow_angle_centideg();
        s.goal_opp_distance_mm    = cameras_get_goal_yellow_distance_mm();
        s.goal_own_visible        = cameras_goal_blue_visible() ? 1 : 0;
        s.goal_own_angle_centideg = cameras_get_goal_blue_angle_centideg();
        s.goal_own_distance_mm    = cameras_get_goal_blue_distance_mm();
    }

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

#ifdef TOP_ENABLE_HEADING_XVAL
    // Cross-validación de salud del heading (TASK-213): alimentar las refs INDEPENDIENTES
    // con la polaridad del arco ya resuelta. xval_update NO corre acá (lo hace 1×/tick
    // sensors_imu_tick); acá sólo se guardan números O(1) → no pesa en el snapshot @100Hz.
    {
        const uint32_t now2 = millis();
        // Cámara: tasa de rotación del bearing del arco RIVAL (goal_rate). Gate anti-strafe:
        // si el robot TRASLADA (|vel_lateral| no ~0), d(bearing)/dt se contamina → invalidar.
        const GoalRateResult gr = cameras_goal_rate_update(s.goal_opp_angle_centideg,
                                                           s.goal_opp_visible != 0, now2);
        const Velocity2D& vel = comm_down_get_velocity();
#if defined(OTOS_VEL_FRAME_IS_FIELD)
        const int16_t vl_heading = s.my_heading_centideg;   // OTOS en marco cancha → des-rotar
#else
        const int16_t vl_heading = 0;   // hipótesis conservadora: marco robot (lateral = vy)
#endif
        const VelLateral vl = vel_lateral_from_otos(vel.vx_mm_s, vel.vy_mm_s, vl_heading);
        const bool have_trans = comm_down_is_vel_fresh();
        // Sin vel fresca (R2 sin OTOS) NO se puede descartar strafe → conservador: cam_valid=gr.valid
        // (el anti-falso-veto con <2 refs indep nunca da MALO cubre ese riesgo).
        const bool cam_valid = gr.valid &&
            (!have_trans || !vel_lateral_is_strafing(vl.lateral_mm_s, 50));
        XvalState& xs = sensors_imu_xval_state();
        xval_feed_camera(xs, gr.rate_dps, cam_valid, now2);
        xval_feed_otos(xs, comm_down_get_omega_dps(), comm_down_omega_valid(), now2);
    }
#endif

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

    // CONFIG PERSISTENTE (A2.1): cargar TopConfig de EEPROM ANTES de los inits de
    // sensores (sus enables gobiernan qué se inicializa). Carga rapida (lectura +
    // CRC). EEPROM en blanco/invalida -> defaults = competencia byte-identica.
    if (top_config_load(&g_top_cfg)) {
        Serial.println("[TOP] config cargada de EEPROM");
    } else {
        Serial.println("[TOP] config EEPROM vacia/invalida -> defaults (todo habilitado)");
    }

    // ORDEN CRITICO (fix 2026-06-02, receta validada en diag_pose_live): los 4 VL53L7CX
    // arrancan en su dir de FÁBRICA 0x29. El BNO SECUNDARIO de este bus Wire vive en 0x28
    // (ya NO en 0x29 — corrección 2026-06-15: no hay ningún BNO en 0x29), pero los ToF en
    // 0x29 igual ensucian el bus si se enumeran a destiempo. Para un boot limpio:
    //   (1) dormir los ToF (LP low) -> bus limpio;
    //   (2) iniciar los BNO (ambos @ 0x28: primario en Wire2, secundario en Wire, sin ToF aún);
    //   (3) recien ahi enumerar los ToF (despertar de a uno -> 0x2A..0x2D).
    // Bug anterior: el BNO se iniciaba con los ToF DESPIERTOS -> imu fallaba
    // + enumeracion ToF confundida -> min_obst=65535.
    // BOOT TIMING (TA-1, 2026-06-14, TASK-210): medir cuánto tarda cada init para
    // VALIDAR el ahorro de cargar el firmware de los ToF a 400 kHz. Se imprime al
    // final del setup ("[boot] ..."). Cero costo en runtime (solo setup). Comparar
    // el "tof_init" ANTES (carga a 100 kHz) y DESPUÉS (carga a 400 kHz) del cambio.
    const uint32_t t_boot0 = millis();
    sensors_tof_predim_lp();  // (1) dormir ToF (LP low) -> bus limpio para el BNO
    const uint32_t t_imu0 = millis();
    sensors_imu_init();       // (2) BNO(s) @ 0x28 (Wire2 + Wire) con los ToF dormidos
    const uint32_t t_imu_ms = millis() - t_imu0;
    sensors_tof_scan_wire();  // DIAG 2026-06-02: con ToF dormidos, que hay en el bus Wire? (BNO @ 0x28)
    const uint32_t t_tof0 = millis();
    sensors_tof_init();       // (3) enumerar ToF a 0x2A..0x2D
    const uint32_t t_tof_ms = millis() - t_tof0;
    // OJO: el robot DEBE apuntar al arco rival (+Y) al boot — esta llamada
    // calibra bno_offset_centideg leyendo el heading actual.
    iitasoccer::localization_runtime_init();
    cameras_init();      // Serial3 + Serial5 ← OpenMV front + back
    comm_down_init();    // Serial1 ← odometría desde ABAJO
    comm_arbiter_init(); // Serial2 (7/8) ↔ placa COMM
    comm_central_init(); // Serial4 (16/17) → snapshot a CENTRAL

#ifdef TOP_ENABLE_SNAPSHOT_TIMER
    // Emisor desacoplado @100Hz por IntervalTimer (GATEADO, BANCO). Arma el timer DESPUES
    // de comm_central_init (Serial4 listo). El WDT (abajo) sigue siendo del LOOP, no de la ISR.
    iitasoccer::snapshot_emitter_init();
#endif

    // WATCHDOG (P1-WDT): instalar AL FINAL del setup, DESPUES de todos los begin()
    // lentos (los 4 ToF cargan ~85 KB c/u por I2C -> boot ~40 s). Si lo armaramos
    // antes, esa carga legitima dispararia el WDT en pleno arranque. Una vez
    // armado, el loop debe alimentarlo cada ciclo (watchdog_feed) o el robot se
    // reinicia a 1 s. Ver el bloque de notas arriba (timeout holgado + Teensy).
    watchdog_init_1s();

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[TOP] cerebro sensorial listo, enviando snapshots a CENTRAL (WDT 1s armado)");

    // BOOT TIMING (TA-1/TA-2, TASK-210/211): resumen de tiempos del arranque. Referencia:
    // original ~40 s (4 ToF a 100 kHz) → ~14,4 s (TA-1, 400 kHz) → ~9,6 s (TA-2, 1 MHz, default).
    Serial.print("[boot] imu_init=");
    Serial.print(t_imu_ms);
    Serial.print(" ms  tof_init=");
    Serial.print(t_tof_ms);
    Serial.print(" ms  setup_total=");
    Serial.print(millis() - t_boot0);
    Serial.println(" ms  (carga ToF a 1 MHz, fallback 400 kHz — TA-2)");

#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)
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
    // Ventana bus-quiet compartida por R1 (deconflict del primario) y R2 (centinela del
    // 2º BNO en Wire): el read del BNO sólo ocurre si pasaron ≥ TOP_BNO_TOF_GAP_MS desde el
    // último read de ToF. Definido ARRIBA del #if/#else para que lo vean las DOS ramas
    // (antes vivía dentro del #if R1 → el #else del centinela R2 no lo veía: no compilaba).
#ifndef TOP_BNO_TOF_GAP_MS
#define TOP_BNO_TOF_GAP_MS 8
#endif
#ifdef TOP_BNO_TOF_DECONFLICT
    // ROBOT1: el BNO comparte el bus `Wire` con los 4 ToF. Si el read multi-byte del
    // BNO cae PEGADO a un read de ToF, el yaw se CONGELA (banco 2026-06-08; mismo
    // mecanismo que el band-aid de 20 Hz en sensors_imu.cpp:259). DECONFLICT: corremos
    // el ToF primero y, si corrió en esta pasada, SALTEAMOS el read del BNO hasta la
    // próxima pasada (donde no hay ToF) → el read del BNO queda AISLADO en el bus.
    // El BNO lee igual a 20 Hz (gate interno de sensors_imu_tick); sólo se corre la
    // FASE para que no choque. ROBOT2 usa el #else: su BNO PRIMARIO vive en Wire2
    // (24/25, bus propio sin ToF) — pero ojo: su SECUNDARIO sí comparte Wire con los
    // ToF y NO tiene este deconflict (tema-a-analizar, revisión 2026-06-10).
    // FIX 2026-06-11 (banco demo): el deconflict POR-PASADA quedó ROTO cuando el
    // loop pasó de ~6 Hz a ~200k pasadas/s (round-robin + payload recortado): la
    // "próxima pasada" ahora llega ~5 µs después del read de ToF → el read del BNO
    // quedaba PEGADO igual. Evidencia: el BNO trasplantado desde robot2 congeló
    // IDÉNTICO al original → el problema es el ENTORNO del bus, no los chips.
    // Ahora la separación es TEMPORAL: el BNO solo se lee si pasaron
    // ≥ TOP_BNO_TOF_GAP_MS desde el final del último read de ToF. Con ToF cada
    // 30 ms y gap de 8 ms queda una ventana limpia de ~7-20 ms por ciclo — el BNO
    // mantiene su cadencia interna de 20 Hz sin tocar al ToF.
    static uint32_t s_last_tof_end_ms = 0;
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
        s_last_tof_end_ms = millis();
    }
    const bool bus_quiet =
        (millis() - s_last_tof_end_ms) >= (uint32_t)TOP_BNO_TOF_GAP_MS;
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS && bus_quiet) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
    }
#else
    // ROBOT2: el primario vive en Wire2 (sin contención). El CENTINELA (2º BNO en Wire)
    // SÍ comparte bus con los ToF → su read @1Hz debe quedar AISLADO temporalmente de los
    // reads de ToF (≥ TOP_BNO_TOF_GAP_MS), igual que el band-aid del primario de R1; sin
    // ese aislamiento se reintroduce el freeze por contención. ⚠️ BANCO (TASK-213): medir
    // con analizador lógico que el read del secundario (<10 ms) queda solo en el bus.
    // Gateado: con TOP_ENABLE_BNO_SENTINEL OFF, este #else queda EXACTO (byte-idéntico).
#ifdef TOP_ENABLE_BNO_SENTINEL
    static uint32_t s_last_tof_end_ms2 = 0;
#endif
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
#ifdef TOP_ENABLE_BNO_SENTINEL
        // Centinela: leer el 2º BNO sólo si pasó GAP_MS desde el último read de ToF en Wire
        // (ventana bus-quiet). El scheduler @1Hz + el read viven en sensors_imu_sentinel_step;
        // casi siempre retorna sin tocar el bus (sólo lee ~1×/s).
        if ((millis() - s_last_tof_end_ms2) >= (uint32_t)TOP_BNO_TOF_GAP_MS) {
            sensors_imu_sentinel_step(millis());
        }
#endif
    }
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
#ifdef TOP_ENABLE_BNO_SENTINEL
        s_last_tof_end_ms2 = millis();   // marca el fin del read de ToF para la ventana bus-quiet
#endif
    }
#endif
    if (g_since_loc_tick >= 33) {  // ~30 Hz — matchea cadencia de los TOFs
        g_since_loc_tick = 0;
        iitasoccer::localization_runtime_tick();
    }

    // === Snapshot → CENTRAL ===
#ifdef TOP_ENABLE_SNAPSHOT_TIMER
    // GATEADO (BANCO): el EMIT lo hace la ISR del IntervalTimer @100Hz desde la pizarra
    // (snapshot_emitter_init en setup). El loop solo PUBLICA los reads cacheados a los
    // slots (single-writer). El emit-en-loop de abajo NO corre (lo reemplaza el timer).
    // El build_snapshot() del loop queda sin usar bajo este flag (lo arma la ISR).
    iitasoccer::snapshot_emitter_publish();
#else
    if (g_since_snapshot >= 10) {  // 100 Hz
        g_since_snapshot = 0;
        WorldSnapshot snap = build_snapshot();
        comm_central_send_snapshot(snap);
    }
#endif

#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)
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
            // cNN = confianza de la fusión. Clásica: 80 una cámara / 95 ambas.
            // Pegajosa (TOP_CAM_STICKY): 80 una / 60 CONFLICTO (ambas reportan
            // pelota = hay un naranja espurio en escena; la titular manda).
            Serial.print(")c");
            Serial.print(cameras_get_ball_confidence());
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
        // Modo de reloj del BNO en el panel (banco 2026-06-10: el banner de boot
        // se pierde antes de que el monitor conecte → imposible saber qué build
        // corre; con el token en el panel no hay ambigüedad nunca más).
        Serial.print(" osc=");
#ifdef TOP_BNO_INTERNAL_OSC
        Serial.print("INT");
#else
        Serial.print("EXT");
#endif
        Serial.print(" resync=");
        Serial.println(cameras_resyncs_total());
    }
}
