// main_central.cpp — Firmware master del robot (Teensy 4.1 sobre Zircon Rev v15).
//
// Responsabilidad: decidir qué hacer y ejecutarlo.
//   • Recibe WORLD_SNAPSHOT desde ARRIBA (Serial7, pin 28) a 100 Hz.
//   • Recibe LINE_URGENT desde ABAJO (Serial1, pin 0) a 100-200 Hz.
//   • Corre la FSM principal (strategy).
//   • Corre todos los PIDs (heading + lateral arquero + approach).
//   • Aplica cinemática inversa omni-3 y PWM directo a los 3 motores del Zircon.
//   • BNO055 local del Zircon: gated por -DCENTRAL_HAS_LOCAL_BNO (default OFF);
//     el heading viene del TOP (snapshot). La CENTRAL no lo usa salvo ese flag.
//
// Watchdog:
//   • Si ARRIBA timeout 500 ms → motors_stop() + LED parpadea (no hay mundo).
//   • Si ABAJO timeout 500 ms → strategy ignora línea (modo ciego de borde).
//
// Build:
//   pio run -e central_robot1   (arquero)
//   pio run -e central_robot2   (delantero)

#include <Arduino.h>

#include "config_central.h"
#include "world_model.h"
#include "strategy.h"
#include "motors_zircon.h"
#include "imu_zircon.h"
#include "comm_top.h"
#include "comm_down.h"
#include "loop_monitor.h"   // supervisor de loop-time (PURO, host-testeable)
#ifdef CENTRAL_BLACKBOX
#include "blackbox.h"       // caja negra de corridas (gateada, envs *_bb)
#endif

using namespace iitasoccer;

// Getter del contador de rechazos por schema de línea (espejo CC-01). Definido en
// comm_down.cpp dentro de namespace iitasoccer; se declara acá (NO en comm_down.h,
// fuera del alcance de este track) en el MISMO namespace para que linkee. Igual
// patrón que comm_top_get_snapshot_size_rejects() en la telemetría DIAG.
namespace iitasoccer { uint32_t comm_down_line_schema_rejects(); }

namespace {

elapsedMillis g_since_strategy_tick;
elapsedMillis g_since_debug;

uint32_t g_loop_count = 0;

// Supervisor de loop-time (audit 2026-06-05, R2). Mide cuánto tarda cada vuelta del
// loop para detectar DEGRADACIÓN (loop lento) antes de que se convierta en cuelgue.
// Barato y sin efectos → corre SIEMPRE (no necesita flag). Zero-init = no primed.
LoopMonitor g_loop_monitor{};

// ============================================================================
// WATCHDOG de hardware en CENTRAL (R2, 2026-06-05) — ARDUINO-ONLY, no host-testeable.
// GATEADO por CENTRAL_ENABLE_WDT (DEFAULT OFF). Sin el flag: CERO cambio de binario.
// ----------------------------------------------------------------------------
// Por qué la CENTRAL lo necesita: la CENTRAL es el MASTER DE MOTORES — es la única
// placa que pone PWM en los 3 H-bridge. Si SU loop se cuelga (p.ej. un ring de UART
// que se traba, o una espera infinita), los motores se quedan con el ÚLTIMO comando
// aplicado y el robot sigue corriendo a ciegas sin auto-reset. El timeout de 500 ms
// del snapshot (world_model) NO cubre esto: ese código vive DENTRO del mismo loop
// colgado, así que motors_stop() nunca llega a ejecutarse. El TOP ya tiene este
// mismo WDOG1 (ver main_top.cpp); acá lo portamos idéntico para la placa de motores.
//
// Implementación: WDOG1 del i.MX RT1062 por registros (imxrt.h, vía Arduino.h),
// igual que el TOP. Timeout HOLGADO de 1 s (mismo criterio): el loop normal corre a
// ~100 Hz, pero stalls legítimos cortos no deben resetear; 1 s da margen y aun así
// reacciona rápido ante un cuelgue REAL (que es indefinido). WDZST=1 suspende el WDT
// en debug/low-power (no resetea al depurar). El mínimo del WDOG1 es 0.5 s; 1 s da holgura.
//
// DEFAULT OFF a propósito: needs_bench antes de prenderlo en competencia —
//   (1) 0 resets espurios en marcha normal (loop drenando ambos UARTs + strategy);
//   (2) auto-reset al colgar el loop a propósito; WDOG1_WRSR debe indicar reset WDT.
// Se activa con -DCENTRAL_ENABLE_WDT en el build_flags del env tras validar en banco.
#ifdef CENTRAL_ENABLE_WDT
constexpr uint8_t CENTRAL_WDT_WT_FIELD = 1;  // (1+1)*0.5 s = 1.0 s (idéntico al TOP)

void watchdog_init_1s() {
    // Una sola escritura al WCR (WT y los bits de control solo se fijan una vez tras
    // el reset). WT(1)->1.0 s; WDE habilita; WDZST suspende en debug/low-power.
    WDOG1_WCR = WDOG_WCR_WT(CENTRAL_WDT_WT_FIELD) | WDOG_WCR_WDE | WDOG_WCR_WDZST |
                WDOG_WCR_SRS | WDOG_WCR_WDA;
    asm volatile("dsb");
}

inline void watchdog_feed() {
    // Refresh ("kick"): la secuencia mágica 0x5555 -> 0xAAAA reinicia el contador.
    WDOG1_WSR = 0x5555;
    WDOG1_WSR = 0xAAAA;
}
#endif  // CENTRAL_ENABLE_WDT

// NOTA: el rol NO se lee de un dipswitch (nombre historico). Se fija en build por
// el flag -DROBOT1 (arquero) / -DROBOT2 (delantero); ver build_flags del env.
//
// OVERRIDE DE ROL (2026-06-09, robot1 en reparación): -DCENTRAL_FORCE_ROLE_GOALKEEPER
// fuerza la FSM del ARQUERO sin cambiar el HARDWARE del build. Caso real: probar el
// arquero en el CUERPO de robot2 → env central_robot2_arquero = -DROBOT2 (pines y
// pisos PROPIOS de robot2) + este flag (rol GK). ⚠️ Cada robot SIEMPRE con su env
// central_robotN (configs por-robot). Nota histórica: hasta la reparación del
// 2026-06-10/11 cruzar envs además invertía el M2 de R1 ({+1,-1,+1}); desde el
// RECABLEADO del M2, MOTOR_INVERT={+1,+1,+1} en ambos (validado piso 8d5fc90).
// Default OFF → binarios de competencia idénticos.
void apply_role_from_dipswitch() {
#if defined(CENTRAL_FORCE_ROLE_GOALKEEPER)
    strategy_set_role(RobotRole::GOALKEEPER);
    Serial.println("[CENTRAL] Role: GOALKEEPER (FORZADO por flag de banco — hardware del build)");
#elif defined(CENTRAL_FORCE_ROLE_ATTACKER)
    // Espejo del override GK (práctica 2026-06-12): la FSM del DELANTERO sobre el
    // hardware que diga el build. Caso real: delantero en ROBOT1 (cuyo default es
    // arquero) sin tocar pines/pisos/inversiones propios de R1.
    strategy_set_role(RobotRole::ATTACKER);
    Serial.println("[CENTRAL] Role: ATTACKER (FORZADO por flag de banco — hardware del build)");
#elif defined(ROBOT1)
    strategy_set_role(RobotRole::GOALKEEPER);
    Serial.println("[CENTRAL] Role: GOALKEEPER (ROBOT1)");
#elif defined(ROBOT2)
    strategy_set_role(RobotRole::ATTACKER);
    Serial.println("[CENTRAL] Role: ATTACKER (ROBOT2)");
#else
    #error "Define ROBOT1 (arquero) o ROBOT2 (delantero) en build_flags"
#endif
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

#ifdef CENTRAL_ENABLE_PHYSICAL_BUTTON
    // Botón físico de arranque (pin 9): DESHABILITADO POR DEFAULT (fail-safe, 2026-06-14).
    // Solo se configura el pin si se opta explícitamente con -DCENTRAL_ENABLE_PHYSICAL_BUTTON.
    pinMode(PIN_MANUAL_START_BUTTON, INPUT_PULLUP);
#endif

    Serial.begin(115200);
    delay(100);
    Serial.println("\n=========================================");
    Serial.println("  IITA Soccer Open — CENTRAL firmware");
    Serial.println("  Zircon Rev v15 + Teensy 4.1, master");
    Serial.println("=========================================");

    apply_role_from_dipswitch();

    motors_init();
    Serial.println("[CENTRAL] motors init OK");

    // BNO055 local: la CENTRAL ya NO lleva BNO (2026-05-31). Los 2 BNO están en el
    // TOP; el heading llega por WORLD_SNAPSHOT de ARRIBA. El módulo imu_zircon queda
    // como compat — solo se inicializa con -DCENTRAL_HAS_LOCAL_BNO. Sin el flag no se
    // toca el bus I2C ni se pierden ~3 s buscando un sensor ausente.
#ifdef CENTRAL_HAS_LOCAL_BNO
    const bool imu_ok = imu_init();
    Serial.print("[CENTRAL] BNO055 local: ");
    Serial.println(imu_ok ? "OK (respaldo del heading de ARRIBA)"
                          : "FAIL (sigue con el heading de ARRIBA por snapshot)");
#else
    Serial.println("[CENTRAL] BNO055 local: N/A (no instalado; heading viene de ARRIBA)");
#endif

    world_model_init();
    strategy_init();

    comm_top_init();    // recibe WORLD_SNAPSHOT (Serial7, pin 28)
    comm_down_init();   // recibe LINE_URGENT (Serial1, pin 0)
    Serial.println("[CENTRAL] UARTs ARRIBA y ABAJO OK");

#ifdef CENTRAL_ENABLE_WDT
    // WATCHDOG (R2): armar AL FINAL del setup, DESPUÉS de motors_init y los comm_*_init
    // (igual que el TOP arma tras sus begin() lentos). Una vez armado, el loop DEBE
    // alimentarlo cada vuelta (watchdog_feed, primera línea de loop) o el Teensy se
    // reinicia a 1 s. DEFAULT OFF — validar en banco antes de prenderlo (ver notas arriba).
    watchdog_init_1s();
    Serial.println("[CENTRAL] WDT de hardware ARMADO (CENTRAL_ENABLE_WDT, 1 s)");
#endif

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[CENTRAL] master listo. Esperando snapshots.");
}

void loop() {
#ifdef CENTRAL_ENABLE_WDT
    // === WATCHDOG (R2): alimentar cada ciclo, PRIMERA línea ===
    // Mientras el loop GIRE, refrescamos el WDOG1. Si el loop se cuelga (ring de UART
    // trabado, espera infinita), deja de alimentarse y el WDOG1 resetea el Teensy a
    // 1 s → el robot (master de motores) se recupera solo en vez de correr a ciegas.
    watchdog_feed();
#endif

    g_loop_count++;

#ifdef CENTRAL_WDT_HANG_TEST
    // === Test de BANCO: colgar el loop a propósito para validar el auto-reset del WDOG1 ===
    // Tras ~5 s de marcha normal (alimentando el WDT cada vuelta), dejamos de alimentarlo
    // (bucle infinito). Con CENTRAL_ENABLE_WDT, el WDOG1 debe RESETEAR el Teensy a ~1 s →
    // se ve en el Serial el reboot (vuelve a imprimir el banner de setup). SOLO banco:
    // gateado OFF por default → binario de competencia byte-idéntico. Env: central_robot1_wdt_hangtest.
    {
        static uint32_t s_boot_ms = millis();
        if (millis() - s_boot_ms > 5000) {
            Serial.println("[CENTRAL] *** WDT HANG TEST: colgando el loop AHORA. El WDOG1 debe resetear en ~1 s. ***");
            Serial.flush();
            while (true) { /* sin watchdog_feed -> reset esperado del WDOG1 */ }
        }
    }
#endif

    // === Supervisor de loop-time (R2) — barato, SIEMPRE activo ===
    // micros() al inicio de la vuelta; loop_monitor_update mide el dt contra la vuelta
    // anterior (wrap-safe) y guarda el peor caso + un promedio suave. Solo diagnóstico:
    // no toma ninguna acción; se imprime en el debug de abajo (loop_us max/avg).
    loop_monitor_update(g_loop_monitor, micros());

    // === RX: drenar ambos UARTs (no bloquea) ===
    comm_top_tick();    // aplica WorldSnapshot al world_model
    comm_down_tick();   // aplica LineStatusV2 al world_model

#ifdef CENTRAL_ENABLE_MANUAL_START
    // === F3 — JUEZ DESDE LA PC (SOLO banco, gateado) ===
    // Para cuando la app del juez no funciona (banco 2026-06-09). Con el monitor
    // serie de la CENTRAL abierto (pio device monitor), desde el teclado de la PC:
    //   ENTER o 'g'  ->  GO   (fuerza match_running=true; el arquero arranca su
    //                          delay de 2 s y corre la secuencia completa)
    //   's'          ->  STOP (suelta el override -> sin juez real la FSM vuelve a
    //                          WAIT_START y los motores paran; re-acomodas el robot
    //                          y mandas 'g' de nuevo = ciclo completo desde cero)
    // Tambien: pulsador en pin 9 a GND = GO (si esta cableado).
    // En COMPETENCIA este flag NO se define: el GO/STOP real llega de la app del
    // juez por los pines 5/6 del TOP -> snapshot -> world_model_match_running().
    {
        static bool g_manual_running = false;
        bool cmd_go = false, cmd_stop = false;
        while (Serial.available()) {
            const int c = Serial.read();
            if (c == '\n' || c == '\r' || c == 'g' || c == 'G') cmd_go = true;
            else if (c == 's' || c == 'S')                      cmd_stop = true;
#ifdef CENTRAL_BLACKBOX
            // Caja negra (envs *_bb): 'd' = volcar CSV ahora · 'x' = borrar/re-armar.
            // (El volcado AUTOMÁTICO ocurre solo en cada RUN→STOP.)
            else if (c == 'd' || c == 'D') blackbox_dump();
            else if (c == 'x' || c == 'X') blackbox_reset();
#endif
        }
        // Ruido de motor en el pin 9 solo puede dar GO espurio (no-op si ya corre);
        // el STOP es exclusivo del teclado -> no hay parada fantasma en pleno test.
#ifdef CENTRAL_ENABLE_PHYSICAL_BUTTON
        // BOTÓN FÍSICO DESHABILITADO POR DEFAULT (fail-safe, 2026-06-14, pedido Gustavo).
        // En AMBOS robots el pulsador ONBOARD del Zircon (pin 9) dio problemas: quedaba
        // CLAVADO en LOW → GO permanente (incidente práctica 2026-06-12) y se sospecha
        // lógica/polaridad invertida. Por seguridad NO se lee a menos que se compile con
        // -DCENTRAL_ENABLE_PHYSICAL_BUTTON. El arranque va por el ÁRBITRO (competencia,
        // pines 5/6 del TOP → world_model) o por el TECLADO serie ('g'/'s'/ENTER, arriba),
        // que NO dependen de este flag. (El gate viejo CENTRAL_MANUAL_START_NO_BUTTON quedó
        // MUERTO: ahora el default ya es seguro; los envs *_nobtn son redundantes.)
        if (digitalRead(PIN_MANUAL_START_BUTTON) == LOW) cmd_go = true;
#endif
        if (cmd_stop) {
            g_manual_running = false;
            world_model_set_force_match_running(false);
            Serial.println("[CENTRAL] *** STOP manual (juez PC) — 'g' o ENTER para GO ***");
        } else if (cmd_go && !g_manual_running) {
            g_manual_running = true;
            world_model_set_force_match_running(true);
            Serial.println("[CENTRAL] *** GO manual (juez PC) — 's' para STOP ***");
        }
    }
#endif

    // === FRENO DE BORDE (EMERGENCY_LINE) — tiene PRIORIDAD sobre la FSM ===
    // En simple: si la placa de ABAJO ve que el robot está por salirse de la
    // cancha (línea inminente) y ese dato es fresco, frenamos YA, sin esperar al
    // ciclo normal de decisión.
    // Por qué va acá: se chequea en CADA vuelta del loop para que el freno llegue
    // en <15 ms desde que ABAJO detecta el borde (no al tick de 100 Hz de strategy).
    // ⚠️ BANCO (audit 2026-06-04): motors_brake() hace freno ACTIVO (corto HIGH/HIGH
    // en el H-bridge), no solo PWM=0 — pero FALTA CONFIRMAR en el Zircon que de
    // verdad frena y no queda en COAST (rueda libre). Medirlo antes de confiar el borde.
    // ANTI-LATCH (banco María 2026-06-14, caja negra v1.2 — root cause): el freno de
    // borde LATCHEABA y CONGELABA al arquero sobre su línea IZQUIERDA. Mecanismo (probado
    // con el log): imminent_exit quedaba en 1 → se frenaba y se hacía `return` salteando
    // strategy_tick() en CADA vuelta → la huida (fase ESCAPE) NUNCA corría → el robot no se
    // movía → la línea seguía inminente → imminent seguía en 1 = DEADLOCK (5+ s trabado,
    // emerg=1 todo el tramo en el CSV). En la línea DERECHA imminent NO disparaba (geometría)
    // y por eso ahí escapaba bien. No era el motor ni el GKS_ESCAPE_MS.
    // Causa de raíz: el freno está para matar el MOMENTO de un roll-out RÁPIDO, pero el
    // arquero VIVE pegado a su línea por diseño → pasados ~350 ms seguir frenando solo lo
    // CONGELA. Fix: frenar el momento y luego SOLTAR (devolver control a la FSM, cuyo ESCAPE
    // lo despega hacia adentro = lado contrario). Se mantiene soltado hasta que imminent
    // baje (sin stutter). Sigue fail-safe ante un borde real: frena de verdad primero; lo
    // único que se elimina es el congelamiento permanente (que era estrictamente peor).
    constexpr uint32_t GK_EDGE_BRAKE_MAX_MS = 350;
    static uint32_t s_edge_brake_since    = 0;
    static bool     s_edge_brake_released = false;
    const bool edge_now = world_model_imminent_exit() && world_model_line_is_fresh();
    if (edge_now) {
        if (s_edge_brake_since == 0) { s_edge_brake_since = millis(); s_edge_brake_released = false; }
        if (!s_edge_brake_released && (millis() - s_edge_brake_since) >= GK_EDGE_BRAKE_MAX_MS) {
            s_edge_brake_released = true;   // momento ya frenado → soltar y NO re-frenar hasta que baje
        }
        if (!s_edge_brake_released) {
            motors_brake();                       // freno activo (corto en H-bridge), no solo PWM=0
            digitalWrite(PIN_LED_STATUS, HIGH);   // LED fijo = alerta visual
#ifdef CENTRAL_BLACKBOX
            // Auditoría 2026-06-11: sin esto la caja negra quedaba CIEGA exactamente durante el
            // freno (el return saltea el tick). Graba con flag de emergencia (columna emerg).
            blackbox_tick_emergency();
#endif
            // No salimos del loop: seguimos leyendo los UARTs para enterarnos cuándo ABAJO baja la alerta.
            return;
        }
        // soltado: caemos al strategy → la FSM (ESCAPE) despega al robot de su línea.
    } else {
        s_edge_brake_since    = 0;
        s_edge_brake_released = false;   // imminent bajó → re-armar el freno para el próximo borde
    }

    // === Strategy + motores ===
    if (g_since_strategy_tick >= 10) {  // 100 Hz
        g_since_strategy_tick = 0;

        if (!world_model_snapshot_is_fresh()) {
            // ARRIBA caído > 500 ms → SAFE_NO_TOP. Parar motores, parpadear LED.
            motors_stop();
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);
#ifdef CENTRAL_BLACKBOX
            blackbox_tick(MotorCommand{});   // timeline completa aun sin snapshot
#endif
        } else {
            MotorCommand cmd = strategy_tick();
            motors_apply_command(cmd);
            digitalWrite(PIN_LED_STATUS, HIGH);  // OK
#ifdef CENTRAL_BLACKBOX
            blackbox_tick(cmd);              // graba lo decidido + lo aplicado
#endif
        }
    }

    // === Debug print cada 500 ms ===
    if (g_since_debug >= 500) {
        g_since_debug = 0;
        Serial.print("[CENTRAL] loop=");
        Serial.print(g_loop_count);
        Serial.print(" role=");
        Serial.print(strategy_get_role() == RobotRole::ATTACKER ? "ATK" : "GK");
        Serial.print(" state=");
        Serial.print(strategy_get_state_name());
        Serial.print(" snap_fresh=");
        Serial.print(world_model_snapshot_is_fresh() ? "Y" : "N");
        // DIAG link TOP->CENTRAL (Serial7): rxB=bytes crudos, fr=frames OK, crc=errores CRC.
        //   rxB=0         -> no llega NADA por Serial7 (cable TOP pin17/TX4 -> 28/RX7 / GND / TOP apagado).
        //   rxB sube,fr=0 -> llegan bytes pero no forma frames (baud/ruido).
        //   fr sube       -> link OK (snap_fresh deberia ser Y).
        Serial.print(" top[rxB=");
        Serial.print(comm_top_get_bytes_received());
        Serial.print(" fr=");
        Serial.print(comm_top_get_frames_received());
        Serial.print(" crc=");
        Serial.print(comm_top_get_crc_errors());
        Serial.print(" rsy=");
        Serial.print(comm_top_get_resync_events());  // #25: resyncs = byte-slip/ruido del link
        Serial.print(" badsz=");
        Serial.print(comm_top_get_snapshot_size_rejects());  // CC-01: snapshots con tamano != schema (deploy v2/v3 desfasado)
        Serial.print("]");
        Serial.print(" line_fresh=");
        Serial.print(world_model_line_is_fresh() ? "Y" : "N");
        // Telemetria del link DOWN->CENTRAL: salud del enlace + estado del dato.
        Serial.print(" down[rx=");
        Serial.print(comm_down_get_frames_received());
        Serial.print(" crc=");
        Serial.print(comm_down_get_crc_errors());
        Serial.print(" lost=");
        Serial.print(comm_down_get_frames_lost());
        Serial.print(" rsy=");
        Serial.print(comm_down_get_resync_events());  // #25
        Serial.print(" badsch=");
        Serial.print(comm_down_line_schema_rejects());  // CC-01 espejo: línea 16 B con schema != 2 (deploy DOWN desfasado)
        Serial.print(" valid=");
        Serial.print(world_model_line_data_valid() ? "Y" : "N");
        Serial.print(" ev=0x");
        Serial.print(world_model_line_event_flags(), HEX);
        Serial.print("]");
        Serial.print(" match=");
        Serial.print(world_model_match_running() ? "RUN" : "STOP");
        Serial.print(" hdg=");
        Serial.print(world_model_get_my_heading_deg(), 1);
        // otos= yaw de la odometría de piso (DOWN, Capa 1) o N si no fluye.
        // Clave en R1 sin BNO: es EL rumbo del delantero de práctica. Chequeo de
        // signo en banco: girar el robot a mano ANTIHORARIO → otos debe SUBIR.
        Serial.print(" otos=");
        if (world_model_otos_is_fresh()) {
            Serial.print(world_model_get_otos_heading_deg(), 1);
        } else {
            Serial.print("N");
        }
        // loop_us(max/avg): salud del loop-time (R2). max = PEOR vuelta vista (no se
        // olvida); avg = EMA suave. Si max sube mucho sobre los ~10 ms del tick de
        // strategy, el loop se está degradando (sensor lento / ring desbordado) antes
        // de que sea un cuelgue. (En estado nominal: max y avg cerca de la cadencia real.)
        Serial.print(" loop_us(max/avg)=");
        Serial.print(g_loop_monitor.max_us);
        Serial.print("/");
        Serial.println(g_loop_monitor.ema_us, 0);
    }
}
