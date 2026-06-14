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
//   pio run -e central_robot1 -t upload  (arquero)
//   pio run -e central_robot2 -t upload  (delantero)


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

namespace iitasoccer { uint32_t comm_down_line_schema_rejects(); }

namespace {

// es un tipo especial de Arduino que cuenta milisegundos automáticamente desde que 
// se creó o se puso en cero. Se usan para controlar cuándo ejecutar la estrategia 
//(cada 10 ms) y cuándo imprimir el debug (cada 500 ms).
elapsedMillis g_since_strategy_tick;
elapsedMillis g_since_debug;


// Un contador que suma 1 cada vez que el loop() se ejecuta. 
// Sirve para saber qué tan rápido está corriendo el programa. 
uint32_t g_loop_count = 0;


// Mide cuánto tiempo tarda cada vuelta del loop.
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

// Se fija que robot y que rol tiene en tiempo de compilacion, dependiendo del comando en la terminal 
// Se fija en build por el flag -DROBOT1 (arquero) / -DROBOT2 (delantero); ver build_flags del env.
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

// Configura el LED de estado como salida y lo apaga al inicio.
void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

#ifdef CENTRAL_ENABLE_MANUAL_START
    pinMode(PIN_MANUAL_START_BUTTON, INPUT_PULLUP);  // F3 fail-safe (solo banco)
#endif

    Serial.begin(115200);
    delay(100);
    Serial.println("\n=========================================");
    Serial.println("  IITA Soccer Open — CENTRAL firmware");
    Serial.println("  Zircon Rev v15 + Teensy 4.1, master");
    Serial.println("=========================================");

    // Determina el rol 
    apply_role_from_dipswitch();

    // inicializa los motores
    motors_init();
    Serial.println("[CENTRAL] motors init OK");

    world_model_init(); // El modelo del mundo (estructura de datos donde se guarda todo lo que el robot "sabe")
    strategy_init();    // La estrategia (la FSM)

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

    // Al terminar, el LED parpadea = "listo" 
    digitalWrite(PIN_LED_STATUS, HIGH); 
    delay(200);
    digitalWrite(PIN_LED_STATUS, LOW);
    delay(200);

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

    // Cuenta las iteraciones del loop 
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
    // Mide cuánto tardó esta vuelta del loop usando micros() hora actual en microsegundos 
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
#ifndef CENTRAL_MANUAL_START_NO_BUTTON
        // CENTRAL_MANUAL_START_NO_BUTTON (práctica 2026-06-12, banco R2): el pulsador
        // ONBOARD del Zircon (pin 9) quedó CLAVADO en LOW → GO permanente: el arquero
        // patrullaba solo al prender y el STOP del teclado se re-disparaba al tick
        // siguiente. Con el flag, el GO/STOP queda SOLO en el teclado ('g'/'s').
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

    // Si la placa DOWN detecta que el robot está por salirse de la cancha 
    // Y ese dato es reciente (menos de 500 ms), se llama a motors_brake() (freno activo, no solo parar)
    // y se sale del loop inmediatamente con return sin pasar por la estrategia. 
    // Esto garantiza que el freno llegue en menos de 15 ms, mucho más rápido que si esperara al ciclo 
    // de estrategia.
    if (world_model_imminent_exit() && world_model_line_is_fresh()) {
        motors_brake();                       // freno activo (corto en H-bridge), no solo PWM=0
        digitalWrite(PIN_LED_STATUS, HIGH);   // LED fijo = alerta visual 

#ifdef CENTRAL_BLACKBOX
        // Auditoría 2026-06-11: sin esto la caja negra quedaba CIEGA exactamente
        // durante el freno de borde (el return saltea el tick) — el momento que
        // más se quiere analizar. Graba con flag de emergencia (bit7/columna emerg).
        blackbox_tick_emergency();
#endif
        // No salimos del loop: seguimos leyendo los UARTs para enterarnos cuándo
        // ABAJO baja la alerta y recuperar el control en el próximo tick.
        return;
    }

    // === Strategy + motores ===
    if (g_since_strategy_tick >= 10) {  // Cada 10 ms (100 veces por segundo) 
        g_since_strategy_tick = 0;

        if (!world_model_snapshot_is_fresh()) 
        {
            // Si el dato de la placa TOP tiene más de 500 ms sin actualizarse 
            //(el TOP se cayó o desconectó), para los motores y hace parpadear el LED como alerta.
            motors_stop();
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);

            #ifdef CENTRAL_BLACKBOX
                        blackbox_tick(MotorCommand{});   // timeline completa aun sin snapshot
            #endif
        } 

        else 
        {   
            // Si el dato es fresco, llama a strategy_tick() 
            // que devuelve un MotorCommand (las velocidades para cada motor), y lo aplica.
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
