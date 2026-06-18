// sensors_tof.cpp — Implementacion VIVA del modulo ToF (4 VL53L7CX) + HC-SR04 del TOP.
//
// Lee los 4 ToF VL53L7CX (bus unico Wire, enumerados a 0x2A..0x2D por LP) + el
// HC-SR04 frontal, y expone la distancia minima como obstaculo mas cercano. Cada
// ToF lleva una marca de frescura (P1-TOF-STALE): tras TOF_STALE_TIMEOUT_MS sin
// lectura buena, el getter devuelve TOF_NO_READING en vez del ultimo valor viejo.
//
// ============================================================================
// MIGRACION 2026-05-24 — stub TODO_TOF_LIB -> Adafruit_VL53L7CX
// ----------------------------------------------------------------------------
// Hasta hoy este modulo era un stub: HC-SR04 funcionaba real pero los 4 ToF
// retornaban TOF_NO_READING en bloque (placeholder TODO_TOF_LIB).
//
// La lib STMicroelectronics (STM32duino_VL53L7CX) que probamos primero NO
// inicializa en Teensy 4.0 — bug raiz en `vl53l7cx_platform.h:49-60`,
// `DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH (256)` desborda en 2 bytes el
// buffer del Wire al cargar el firmware blob del sensor (cada chunk son 2
// bytes header + 256 bytes payload). La lib Adafruit_VL53L7CX usa
// `maxBufferSize() - 2` (reserva el header) y funciona out of the box.
//
// Decision: migrar a Adafruit. Ver journal completo:
//   journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md
//
// Estado del hardware (al 2026-05-30, banco):
//   • 4 ToF activos enumerados a 0x2A-0x2D via LP 9/10/11/12 (bus unico Wire);
//     get_distance_mm(0..3) e is_ready(0..3) devuelven lecturas reales.
//   • HC-SR04 frontal (TRIG/ECHO) funciona como antes.
// ============================================================================

#include "sensors_tof.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>
#include "top_eeprom_config.h"   // g_top_cfg.ultrasonic_en / tof[i].enabled (A2.1)

// --- Modulos PUROS del rediseno sensorial no-bloqueante (host-testeados); glue Arduino abajo ---
#include "tof_zone_mask.h"       // A2.2: mascara de zonas (anular superiores) — ungated, no-op por default
#include "tof_zone_mask_orient.h" // A2.2: rotacion/flip de la mascara (firmware dueno de la rotacion; tras -DTOP_ENABLE_TOF_ROT)

// Umbrales del reductor ROBUSTO (TOP_ENABLE_TOF_ROBUST), -D-overrideables:
#ifndef TOF_ROBUST_FIELD_MAX_MM
#define TOF_ROBUST_FIELD_MAX_MM 2430   // dimension mas larga de la cancha (mm): > esto = rayo fuera/sin retorno
#endif
#ifndef TOF_ROBUST_LOW_KEEP_PCT
#define TOF_ROBUST_LOW_KEEP_PCT 70     // descarta zonas < 70% de la mediana (rebote en otro robot)
#endif
#if defined(TOP_ENABLE_TOF_SCHED)
#include "tof_schedule.h"        // turnero round-robin + SKIP del ToF caido (gateado)
#endif
#if defined(TOP_ENABLE_HCSR04_ASYNC) && defined(TOP_ENABLE_HCSR04)
#include "hcsr04_async.h"        // FSM no-bloqueante del HC-SR04 (reemplaza el pulseIn de 12 ms)
#endif

namespace iitasoccer {

namespace {

// ----- Estado del modulo -----
uint16_t g_distances_mm[NUM_TOF];
uint16_t g_zones_mm[NUM_TOF][16];  // zonas crudas 4x4 por sensor (campo "z" de la telemetría)
bool     g_ready[NUM_TOF];
// Frescura por sensor (P1-TOF-STALE 2026-06-03): g_last_ok_ms[i] = millis() de la
// ultima lectura BUENA; g_ever_ok[i] = si alguna vez hubo una. La decision
// fresco/stale se delega a tof_fresh_or_no_reading() (pura, host-testeada).
uint32_t g_last_ok_ms[NUM_TOF] = {0};
bool     g_ever_ok[NUM_TOF]    = {false};
uint16_t g_hcsr04_mm = TOF_NO_READING;
uint32_t g_tick_count = 0;

#if defined(TOP_ENABLE_TOF_SCHED)
// Turnero round-robin (tof_schedule.h). Single-writer: SOLO el loop (sensors_tof_tick)
// lo toca -> sin race. Byte-equivalente con los 4 ToF ready; saltea el caido.
TofSchedState g_tof_sched;
#endif

#if defined(TOP_ENABLE_HCSR04_ASYNC) && defined(TOP_ENABLE_HCSR04)
// FSM no-bloqueante del HC-SR04. La ISR de ECHO la ESCRIBE (hcsr04_on_edge); el loop la
// LEE/AVANZA (due/on_trig_sent/poll). NO es volatile a proposito: TODO acceso del loop
// va dentro de noInterrupts()/interrupts(), que (a) impiden que la ISR de ECHO corra
// durante un RMW del loop y (b) son barreras de memoria (asm volatile memory) -> el loop
// ve siempre el ultimo valor escrito por la ISR. La ISR es atomica (el NVIC no la re-entra
// y ningun otro ISR toca g_hc_fsm). Asi se cierra la race loop<->ISR sin volatile/const_cast.
Hcsr04Async    g_hc_fsm;
Hcsr04AsyncCfg g_hc_cfg = hcsr04_async_default_cfg();

// ISR de ECHO (enganchada a CHANGE = ambos flancos). Trabajo MINIMO: una transicion de
// estado de la FSM, SIN bus I2C, SIN Serial, SIN bloqueo (regla load-bearing: las ISR
// hacen lo minimo). Mide el ancho del eco (subida->bajada) para la distancia.
void hcsr04_echo_isr() {
    hcsr04_on_edge(g_hc_fsm, digitalRead(PIN_HCSR04_ECHO) == HIGH, micros());
}
#endif

// ----- Sensor frontal (indice 0); los 4 ToF activos se enumeran en g_tof_multi -----
// Ver bloque TOP_ENABLE_MULTI_TOF: 4 ToF 0x2A-0x2D via LP 9/10/11/12 (banco 2026-05-30).
constexpr uint8_t TOF_FRONTAL_IDX = 0;  // indice U2 en la abstraccion
Adafruit_VL53L7CX     g_tof_frontal;
VL53L7CX_ResultsData  g_tof_results;
bool                  g_tof_init_logged = false;  // anti-spam del log de init

#ifdef TOP_ENABLE_MULTI_TOF
// ----- Enumeracion de los 4 ToF (bus unico Wire) — ACTIVO POR DEFAULT (2026-06-01) -----
// Portado de diag_top_tof_quad_live (validado en banco 2026-05-30). LP pins y
// direcciones vienen de pinout_robotN.h: PIN_TOF_XSHUT[]={9,10,11,12} (ACTIVO-ALTO),
// TOF_I2C_ADDR_ASSIGNED[]={0x2A..0x2D}.
// >>> ACTIVADO POR DEFAULT 2026-06-01 (top_robot1/2). Condiciones cumplidas: <<<
//   1) PIN 10 (LP ToF[1]) LIBRE: el rol va por #define ROBOT1/ROBOT2; el TOP NO lee
//      dipswitch en pin 10 -> sin conflicto (no conectar un dipswitch fisico ahi).
//   2) Validado en banco (diag_top_tof_quad_live: los 4 enumeran a 0x2A..0x2D, posicion
//      + orientacion mapeadas). Recordar POWER-CYCLE (las dir I2C persisten con 3V3).
//   3) HEADS-UP boot: begin() carga ~85KB por ToF (~10s c/u) -> ~40s de arranque del TOP
//      (vs ~10s con 1 ToF). Tolerable en power-on; conviene medirlo en cancha.
Adafruit_VL53L7CX  g_tof_multi[NUM_TOF];
constexpr uint8_t  LP_WAKE_LEVEL  = HIGH;   // activo-alto (banco 2026-05-30)
constexpr uint8_t  LP_SLEEP_LEVEL = LOW;
constexpr uint32_t LP_SETTLE_MS   = 120;

inline bool tof_i2c_acks(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}
inline void tof_set_all_lp(uint8_t level) {
    for (int i = 0; i < NUM_TOF; ++i) digitalWrite(PIN_TOF_XSHUT[i], level);
}
// Cambio de direccion de bajo nivel (no requiere firmware cargado) — para recover.
bool tof_raw_change_addr(uint8_t cur, uint8_t next) {
    Wire.beginTransmission(cur);
    Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(cur);
    Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission() != 0) return false;
    delay(5);
    return tof_i2c_acks(next);
}
// Junta cualquier ToF disperso (de corridas previas) de vuelta a 0x29.
void tof_recover_to_default() {
    tof_set_all_lp(LP_WAKE_LEVEL);
    delay(60);
    const uint8_t dispersed[] = {0x2A, 0x2B, 0x2C, 0x2D, 0x60};
    for (uint8_t k = 0; k < sizeof(dispersed); ++k)
        for (uint8_t t = 0; t < 4 && tof_i2c_acks(dispersed[k]); ++t)
            tof_raw_change_addr(dispersed[k], VL53L7CX_DEFAULT_ADDRESS);
}
#endif  // TOP_ENABLE_MULTI_TOF

// Resolucion 4x4 = 16 zonas. Mas liviano que 8x8 y suficiente para un solo
// sensor que aporta "distancia frontal promedio" al firmware del TOP. Si en
// el futuro se quiere usar el array completo para evasion fina, subir a 64.
constexpr uint8_t TOF_RESOLUTION_ZONES = 16;  // 4x4
constexpr uint8_t TOF_RANGING_FREQ_HZ  = 15;

// ----------------------------------------------------------------------------
// CLOCKS I2C — DOS regímenes distintos (TA-1 + TA-2, 2026-06-14). Ver TASK-210/211.
//  • Carga del firmware (fase "ToF-solo": BNO iniciado pero nadie lo lee, el loop no
//    arrancó): se hace a ALTA velocidad para acortar el boot. DEFAULT 1 MHz
//    (TOF_INIT_CLOCK_FAST_HZ, TA-2), con FALLBACK a 400 kHz (TOF_INIT_CLOCK_HZ, TA-1)
//    si algún sensor no carga. Ambos validados en banco (ver constantes abajo).
//  • TOF_RUN_CLOCK_HZ: para el RUNTIME (loop). A >100 kHz el read multi-byte del
//    BNO055 se corrompe CUANDO los ToF rangean en el mismo bus y el yaw se
//    CONGELA (banco 2026-06-02/06-08). Por eso el bus VUELVE a 100 kHz al final
//    de sensors_tof_init(), ANTES de que arranque el loop.
// ⚠️ El restore a TOF_RUN_CLOCK_HZ al final del init es OBLIGATORIO: sin él se
//    reintroduce el freeze del heading. Verificable en banco (girar el robot).
constexpr uint32_t TOF_INIT_CLOCK_HZ = 400000;  // FALLBACK de carga si 1 MHz falla (validado TA-1)
constexpr uint32_t TOF_RUN_CLOCK_HZ  = 100000;  // runtime (coexistencia BNO+ToF)
// TA-2 (TASK-211): carga a 1 MHz (Fast Mode Plus) = DEFAULT de producción desde 2026-06-14.
// VALIDADO en banco por Gustavo + Virginia (>15 power-cycles en top_robot2_pri: 4/4 ToF a
// 1 MHz, 0 fallbacks, boot ~9,6 s vs ~14,4 s a 400 kHz vs ~40 s original). Si algún ToF no
// cargara a 1 MHz (bus marginal), el init RECAE a TOF_INIT_CLOCK_HZ (400 kHz) reseteando el
// sensor por LP → arranque siempre robusto. Para forzar 400 kHz: bajar esta constante a 400000.
constexpr uint32_t TOF_INIT_CLOCK_FAST_HZ = 1000000;

// ----------------------------------------------------------------------------
// HC-SR04 ultrasonido frontal — ACTIVO en top_robot1/2 (flag -DTOP_ENABLE_HCSR04).
// ----------------------------------------------------------------------------
// Cableado CONFIRMADO en banco (Gustavo 2026-06-02): TRIG=pin 4, ECHO=pin 3 (pines
// ex-XSHUT ToF, libres; NO son UART) -> sin conflicto con ningun Serial (el viejo lio
// del pin 7 ya no aplica). Aporta "distancia frontal" al min_obstacle del snapshot
// (redundante con el ToF frontal, util como respaldo).
// TRADE-OFF (aceptado): pulseIn() es BLOQUEANTE. Para acotar el impacto en el uplink de
// 100 Hz: (a) timeout reducido a 12 ms (~2 m, cubre la cancha) en vez de 25 ms; (b) se
// lee solo cada 3 ticks de ToF (~90 ms) en sensors_tof_tick(). Mejora futura: hacerlo NO
// bloqueante (trigger + medir echo por interrupcion). Sin el flag, el modulo no toca los
// pines ni llama a pulseIn y devuelve TOF_NO_READING.
#if defined(TOP_ENABLE_HCSR04) && !defined(TOP_ENABLE_HCSR04_ASYNC)
// HC-SR04 — lectura BLOQUEANTE (default historico). Solo se compila cuando el async NO
// esta activo; con -DTOP_ENABLE_HCSR04_ASYNC la reemplaza la FSM no-bloqueante (sin pulseIn).
uint16_t read_hcsr04() {
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // pulseIn con timeout — si no llega echo (nada a <~2 m), retorna 0.
    const uint32_t duration_us = pulseIn(PIN_HCSR04_ECHO, HIGH, 12000UL);  // 12ms = ~2m (cubre la cancha)
    if (duration_us == 0) return TOF_NO_READING;
    // Velocidad del sonido: 343 m/s = 0.343 mm/µs. Duracion es ida + vuelta.
    return static_cast<uint16_t>((duration_us * 343UL) / 2000UL);
}
#endif  // TOP_ENABLE_HCSR04 && !TOP_ENABLE_HCSR04_ASYNC

// Promedia las zonas validas del frame 4x4 del L7CX. status==5/6/9 son
// "valid range" segun convencion ST. Devuelve TOF_NO_READING si NINGUNA
// zona del frame es valida (sensor mirando al vacio, fuera de rango, etc).
[[maybe_unused]] uint16_t mean_valid_zones(const VL53L7CX_ResultsData& r, uint8_t n_zones) {
    uint32_t sum = 0;
    uint16_t count = 0;
    for (uint8_t i = 0; i < n_zones; ++i) {
        const uint8_t s = r.target_status[i];
        const bool valid = (s == 5 || s == 6 || s == 9);
        const int16_t mm = r.distance_mm[i];
        if (valid && mm >= 0 && mm <= static_cast<int16_t>(TOF_MAX_RANGE_MM)) {
            sum += static_cast<uint32_t>(mm);
            ++count;
        }
    }
    if (count == 0) return TOF_NO_READING;
    return static_cast<uint16_t>(sum / count);
}

// Copia las n_zones crudas del frame al buffer dst: distancia mm si la zona es
// valida (status 5/6/9 y en rango), o TOF_NO_READING si no. Hermano de
// mean_valid_zones pero SIN promediar — para exponer la grilla por telemetría.
void fill_zones(const VL53L7CX_ResultsData& r, uint16_t* dst, uint8_t n_zones) {
    for (uint8_t i = 0; i < n_zones; ++i) {
        const uint8_t s = r.target_status[i];
        const bool valid = (s == 5 || s == 6 || s == 9);
        const int16_t mm = r.distance_mm[i];
        dst[i] = (valid && mm >= 0 && mm <= static_cast<int16_t>(TOF_MAX_RANGE_MM))
                 ? static_cast<uint16_t>(mm) : TOF_NO_READING;
    }
}

}  // namespace

// Duerme los 4 ToF (LP low) para dejar el bus I2C limpio ANTES de iniciar el BNO.
// Receta validada en diag_pose_live: (1) dim ToF -> (2) init BNO -> (3) enumerar ToF.
// Sin esto, el BNO secundario (Wire @ 0x28) se inicia con los ToF DESPIERTOS en su dir
// de fábrica 0x29 ensuciando el bus -> el/los BNO no aparecen. (Ya NO hay BNO en 0x29 —
// corrección 2026-06-15.) Llamar en setup() ANTES de sensors_imu_init().
void sensors_tof_predim_lp() {
#ifdef TOP_ENABLE_MULTI_TOF
    Wire.begin();
    Wire.setClock(TOF_RUN_CLOCK_HZ);  // 100 kHz: coexistencia BNO055 + VL53L7CX (a 400 kHz el
                            // yaw del BNO se congela con los ToF activos). Ver sensors_imu.cpp.
    for (int i = 0; i < NUM_TOF; ++i) {
        pinMode(PIN_TOF_XSHUT[i], OUTPUT);
        digitalWrite(PIN_TOF_XSHUT[i], LP_SLEEP_LEVEL);
    }
    delay(LP_SETTLE_MS);
#endif
}

void sensors_tof_scan_wire() {
    // OJO: NO llamar Wire.begin() aca. El bus ya viene levantado por sensors_imu_init;
    // un Wire.begin() extra resetea el periferico y hace FALLAR el primer probe -> falso
    // "nada responde". Probamos sobre el bus YA operativo (igual que i2c_present del loop).
    Serial.print(F("[i2c-scan Wire, ToF dormidos] ACK en:"));
    int n = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { Serial.print(F(" 0x")); Serial.print(a, HEX); ++n; }
    }
    if (n == 0) Serial.print(F(" (nada responde!)"));
    Serial.println();
}

bool sensors_tof_init() {
    Wire.begin();
    Wire.setClock(TOF_RUN_CLOCK_HZ);   // idempotente; 100 kHz (coexistencia BNO+ToF). La carga
                             // de los ToF bajará y subirá esto sola (TA-1, ver constantes arriba).
#ifdef TOP_ENABLE_HCSR04
    // HC-SR04 frontal — solo si se reactivo explicitamente (ver nota arriba).
    // Pines 4/3 (libres); el conflicto de pin 7 ya no aplica.
    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    pinMode(PIN_HCSR04_ECHO, INPUT);
#if defined(TOP_ENABLE_HCSR04_ASYNC)
    // FSM no-bloqueante: enganchar la ISR de ECHO a AMBOS flancos (CHANGE). La FSM mide el
    // ancho del eco sin que el loop espere. Se engancha al final del init (TRIG ya en LOW,
    // g_hc_fsm en IDLE por .bss). La ISR no toca el bus -> no interfiere con la carga de ToF.
    attachInterrupt(digitalPinToInterrupt(PIN_HCSR04_ECHO), hcsr04_echo_isr, CHANGE);
#endif
#endif

    for (int i = 0; i < NUM_TOF; ++i) {
        g_distances_mm[i] = TOF_NO_READING;
        g_ready[i] = false;
        g_last_ok_ms[i] = 0;
        g_ever_ok[i] = false;   // sin lectura buena todavia -> getter da NO_READING
        for (int z = 0; z < 16; ++z) g_zones_mm[i][z] = TOF_NO_READING;
    }

    // NOTA: NO tocamos los pines XSHUT (PIN_TOF_XSHUT[]). Validado el
    // 2026-05-24: la lib Adafruit no los necesita (maneja el chip por I2C),
    // y en la placa actual esos pines probablemente ni siquiera estan
    // ruteados al modulo Pololu. Cuando lleguen los otros 3 ToF y haya que
    // enumerar 4 sensores en la misma direccion default, se agrega la
    // logica de XSHUT + setAddress() aca.

    // NOTA (actualizada TA-1 2026-06-14): este init SÍ maneja el clock, pero de
    // forma acotada y auto-contenida: sube a TOF_INIT_CLOCK_HZ (400 kHz) SOLO para
    // la carga del firmware y LO DEJA SIEMPRE en TOF_RUN_CLOCK_HZ (100 kHz) al salir
    // (todos los paths de return). Así ningún otro módulo del TOP hereda un clock
    // peligroso: cuando arranca el loop, el bus está garantizado en 100 kHz.

#ifdef TOP_ENABLE_MULTI_TOF
    // === Enumeracion de los 4 ToF (bus unico Wire, LP por bodge) ===
    // Secuencia validada en banco (diag_top_tof_quad_live, 2026-05-30):
    // dormir todos los LP -> despertar de a uno -> begin() en 0x29 ->
    // setAddress(0x2A..0x2D) -> configurar -> rangear. Tolerante a fallos:
    // si un ToF no responde, se saltea y el resto sigue.
    for (int i = 0; i < NUM_TOF; ++i) {
        pinMode(PIN_TOF_XSHUT[i], OUTPUT);
        digitalWrite(PIN_TOF_XSHUT[i], LP_SLEEP_LEVEL);
    }
    tof_recover_to_default();           // limpiar direcciones de corridas previas
    tof_set_all_lp(LP_SLEEP_LEVEL);
    delay(LP_SETTLE_MS);
    for (int i = 0; i < NUM_TOF; ++i) {
        digitalWrite(PIN_TOF_XSHUT[i], LP_WAKE_LEVEL);
        delay(LP_SETTLE_MS);
        if (!tof_i2c_acks(VL53L7CX_DEFAULT_ADDRESS)) {
            digitalWrite(PIN_TOF_XSHUT[i], LP_SLEEP_LEVEL);  // LP no controla este ToF
            continue;
        }
        // Carga del firmware a 1 MHz (Fast Mode Plus) = DEFAULT de producción desde 2026-06-14
        // (TA-2, TASK-211; validado por Gustavo + Virginia: >15 power-cycles, 4/4 a 1 MHz, 0 fallbacks).
        // Si algún ToF no cargara a 1 MHz (bus marginal), RESETEA por LP (pudo quedar a medio cargar)
        // y reintenta a 400 kHz (TA-1, validado) → red de seguridad: el arranque queda robusto pase
        // lo que pase. El log avisa cada fallback (si aparece, ese ToF es marginal a 1 MHz).
        if (!g_tof_multi[i].begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, TOF_INIT_CLOCK_FAST_HZ)) {
            Serial.print(F("[sensors_tof] ToF ")); Serial.print(i);
            Serial.println(F(": carga 1 MHz fallo -> reset LP + fallback 400 kHz"));
            digitalWrite(PIN_TOF_XSHUT[i], LP_SLEEP_LEVEL);
            delay(LP_SETTLE_MS);
            digitalWrite(PIN_TOF_XSHUT[i], LP_WAKE_LEVEL);
            delay(LP_SETTLE_MS);
            if (!tof_i2c_acks(VL53L7CX_DEFAULT_ADDRESS) ||
                !g_tof_multi[i].begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, TOF_INIT_CLOCK_HZ)) {
                digitalWrite(PIN_TOF_XSHUT[i], LP_SLEEP_LEVEL);  // ni a 400 kHz -> saltear
                continue;
            }
        }
        if (!g_tof_multi[i].setAddress(TOF_I2C_ADDR_ASSIGNED[i]))           continue;
        g_tof_multi[i].setResolution(TOF_RESOLUTION_ZONES);
        g_tof_multi[i].setRangingFrequency(TOF_RANGING_FREQ_HZ);
#ifdef TOP_ENABLE_TOF_CONTINUOUS
        // Modo CONTINUO en vez del autonomo por defecto (banco, desactivado por defecto):
        // segun el manual ST (UM3038) achica el bloque de resultados por lectura I2C ->
        // cada getRangingData() ocupa menos el bus -> menos jitter del loop. Con la bandera
        // apagada (competencia) queda el modo autonomo de hoy = sin cambio. Banco: TASK-219.
        g_tof_multi[i].setRangingMode(VL53L7CX_RANGING_MODE_CONTINUOUS);
#endif
        if (!g_tof_multi[i].startRanging())                                 continue;
        g_ready[i] = true;              // queda despierto (retiene dir + rangea)
    }
#if defined(TOP_ENABLE_TOF_SCHED)
    // Sembrar el turnero con quien quedo VIVO. Un ToF que fallo el init (g_ready=false)
    // queda fuera de la rotacion -> el turnero no le malgasta el tick (skip del caido).
    for (int i = 0; i < NUM_TOF; ++i)
        tof_sched_set_ready(g_tof_sched, static_cast<uint8_t>(i), g_ready[i]);
#endif
    // TA-1 (2026-06-14, TASK-210): RESTAURAR el clock de runtime ANTES de que arranque el
    // loop. La carga de arriba corrió a TOF_INIT_CLOCK_HZ (400 kHz); el runtime DEBE volver
    // a 100 kHz o el yaw del BNO se congela al chocar con los reads de ToF. OBLIGATORIO.
    Wire.setClock(TOF_RUN_CLOCK_HZ);
    {
        int n_ok = 0;
        for (int i = 0; i < NUM_TOF; ++i) if (g_ready[i]) ++n_ok;
        Serial.print(F("[sensors_tof] multi-ToF (TOP_ENABLE_MULTI_TOF): "));
        Serial.print(n_ok);
        Serial.println(F(" de 4 midiendo."));
    }
    g_tof_init_logged = true;
    return true;
#else
    // Init del ToF frontal U2 (unico instalado fisicamente al 2026-05-24).
    // begin() devuelve bool. Internamente carga ~85 KB de firmware blob por
    // I2C, puede tardar hasta ~10 s.
    // TA-1 (2026-06-14, TASK-210): carga a 400 kHz (fase ToF-solo, segura) y restore
    // a 100 kHz antes de salir — IGUAL que el path MULTI. A 400 kHz en RUNTIME el BNO055
    // y los ToF se pisan y el yaw se CONGELA, por eso siempre se vuelve a TOF_RUN_CLOCK_HZ.
    if (!g_tof_frontal.begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, TOF_INIT_CLOCK_HZ)) {  // 400 kHz SOLO p/carga (TA-1)
        Wire.setClock(TOF_RUN_CLOCK_HZ);   // begin() dejó el bus a 400 kHz aunque falló → restaurar
        if (!g_tof_init_logged) {
            Serial.println(F("[sensors_tof] WARN: VL53L7CX U2 begin() fallo; "
                             "se sigue sin ToF frontal."));
            g_tof_init_logged = true;
        }
        // No retornamos false — el HC-SR04 puede seguir funcionando y el
        // resto del firmware del TOP necesita seguir corriendo.
        return true;
    }
    Wire.setClock(TOF_RUN_CLOCK_HZ);   // carga OK → volver a 100 kHz para el runtime (TA-1).
                                       // La config de abajo (set*/startRanging) es chica: a 100 kHz no pesa.
#ifdef TOP_ENABLE_TOF_CONTINUOUS
    g_tof_frontal.setRangingMode(VL53L7CX_RANGING_MODE_CONTINUOUS);  // banco (ver multi-ToF arriba)
#endif
    if (!g_tof_frontal.setResolution(TOF_RESOLUTION_ZONES) ||
        !g_tof_frontal.setRangingFrequency(TOF_RANGING_FREQ_HZ) ||
        !g_tof_frontal.startRanging()) {
        if (!g_tof_init_logged) {
            Serial.println(F("[sensors_tof] WARN: VL53L7CX U2 config/start fallo; "
                             "se sigue sin ToF frontal."));
            g_tof_init_logged = true;
        }
        return true;
    }

    g_ready[TOF_FRONTAL_IDX] = true;
    Serial.println(F("[sensors_tof] OK: VL53L7CX U2 frontal listo "
                     "(4x4 @ 15 Hz, lib Adafruit)."));
    g_tof_init_logged = true;
    return true;
#endif  // TOP_ENABLE_MULTI_TOF
}

void sensors_tof_tick() {
    g_tick_count++;

    // ToF frontal U2: polling no bloqueante. isDataReady() es un read I2C
    // chico (~milisegundos en 400 kHz); si hay frame nuevo, getRangingData
    // lo trae y promediamos las zonas validas.
#ifdef TOP_ENABLE_MULTI_TOF
    // ROUND-ROBIN (banco 2026-06-10): UN ToF por tick, rotando — NO los 4 juntos.
    // MEDIDO en banco (panel [TOP], robot2): con los 4 getRangingData() en la misma
    // pasada el loop del TOP caia a ~6 Hz (Δloop = +3 por linea de panel de 500 ms)
    // → el WorldSnapshot llegaba a ~4 Hz a la CENTRAL → la FSM controlaba el rumbo
    // con heading de 250-500 ms de atraso (ping-pong de pulsos del arquero, J/U de
    // la reversa) y los RX de camaras rozaban overflow (resyncs). Causa: cada
    // getRangingData() del VL53L7CX trae un bloque grande de resultados por Wire a
    // 100 kHz (decenas de ms) — 4 por pasada ≈ 160 ms bloqueado.
    // Con 1 por tick (TOF_TICK_INTERVAL_MS=30) cada sensor se refresca cada ~120 ms:
    // su ranging interno es 15 Hz (~66 ms/frame) asi que casi siempre hay frame
    // listo, y la frescura P1-TOF-STALE (250 ms) queda con margen 2×.
    // ⚠️ ROBOT1 hereda este cambio (mismo codigo) — A VERIFICAR en su banco al volver.
    const uint32_t now = millis();
#if defined(TOP_ENABLE_TOF_SCHED)
    // Turnero puro: UN ToF por tick SALTEANDO el caido. Con los 4 ready devuelve
    // 0,1,2,3,0,... (byte-equivalente al s_rr de abajo). TOF_SCHED_NONE = 4 caidos ->
    // no se lee ToF este tick (i_valid=false), sin colgarse.
    const uint8_t i = tof_sched_next(g_tof_sched, now);
    const bool i_valid = (i != TOF_SCHED_NONE);
    if (i_valid) tof_sched_note_attempt(g_tof_sched, i, now);
#else
    static uint8_t s_rr = 0;
    const uint8_t i = s_rr;
    s_rr = static_cast<uint8_t>((s_rr + 1) % NUM_TOF);
    const bool i_valid = true;
#endif
    if (i_valid && g_ready[i] && g_tof_multi[i].isDataReady() &&
        g_tof_multi[i].getRangingData(&g_tof_results)) {
        // getRangingData() OK = el sensor responde por I2C -> lectura FRESCA
        // (aunque mean sea NO_READING = "nada en rango", es una respuesta
        // valida y reciente, no un dato colgado). Sellamos la frescura.
        // A2.2 — MASCARA DE ZONAS: primero llenamos las zonas crudas, luego reducimos a una
        // distancia aplicando la mascara del sensor (g_top_cfg.tof[i].zone_mask): las zonas
        // marcadas 0 (ej. filas superiores que ven la estructura/techo) NO entran al promedio.
        // Default mask=~0 -> promedia todas las validas = IDENTICO a mean_valid_zones (fail-safe
        // byte-neutro). g_zones_mm queda CRUDO (la telemetria muestra las 16; el monitor pinta
        // cuales estan anuladas). Costo: el mismo loop de 16 zonas que ya se recorria.
        fill_zones(g_tof_results, g_zones_mm[i], TOF_RESOLUTION_ZONES);
        uint64_t zmask = (i < TOP_CFG_NUM_TOF) ? g_top_cfg.tof[i].zone_mask : ~(uint64_t)0;
#if defined(TOP_ENABLE_TOF_ROT)
        // FIRMWARE dueno de la rotacion (A2.2): la mascara llega en marco CANONICO (la app deja
        // de plegar) y aca se rota al marco CRUDO del sensor segun su rot/flip de EEPROM, asi el
        // veto cae en las zonas fisicas correctas. Con el flag apagado (competencia) este bloque
        // desaparece -> binario byte-identico y la app sigue plegando (sin doble rotacion).
        if (i < TOP_CFG_NUM_TOF) {
            zmask = tof_zone_mask_orient((uint16_t)zmask,
                                         g_top_cfg.tof[i].zone_rotation_deg, g_top_cfg.tof[i].flip);
        }
#endif
#if defined(TOP_ENABLE_TOF_ROBUST)
        // Reduccion ROBUSTA (A2.2): descarta rayos fuera de cancha (> field_max) y outliers bajos
        // (rebote en otro robot). Apagado por defecto -> usa masked_mean = byte-identico.
        g_distances_mm[i] = tof_zone_masked_robust(g_zones_mm[i], TOF_RESOLUTION_ZONES, zmask,
                                                   TOF_NO_READING, TOF_ROBUST_FIELD_MAX_MM,
                                                   TOF_ROBUST_LOW_KEEP_PCT);
#else
        g_distances_mm[i] = tof_zone_masked_mean(g_zones_mm[i], TOF_RESOLUTION_ZONES, zmask, TOF_NO_READING);
#endif
        g_last_ok_ms[i]   = now;
        g_ever_ok[i]      = true;
    }
    // si getRangingData() devuelve false, NO tocamos g_last_ok_ms[i]: el valor
    // cacheado se mantiene SOLO mientras siga fresco; tras TOF_STALE_TIMEOUT_MS
    // sin un read bueno, el getter lo expira a TOF_NO_READING (P1-TOF-STALE).
#else
    if (g_ready[TOF_FRONTAL_IDX]) {
        if (g_tof_frontal.isDataReady()) {
            if (g_tof_frontal.getRangingData(&g_tof_results)) {
                // A2.2 mascara de zonas (ver nota en el path MULTI): zonas crudas -> masked_mean.
                fill_zones(g_tof_results, g_zones_mm[TOF_FRONTAL_IDX], TOF_RESOLUTION_ZONES);
                uint64_t zmask = (TOF_FRONTAL_IDX < TOP_CFG_NUM_TOF)
                                       ? g_top_cfg.tof[TOF_FRONTAL_IDX].zone_mask : ~(uint64_t)0;
#if defined(TOP_ENABLE_TOF_ROT)
                if (TOF_FRONTAL_IDX < TOP_CFG_NUM_TOF) {
                    zmask = tof_zone_mask_orient((uint16_t)zmask,
                                                 g_top_cfg.tof[TOF_FRONTAL_IDX].zone_rotation_deg,
                                                 g_top_cfg.tof[TOF_FRONTAL_IDX].flip);
                }
#endif
#if defined(TOP_ENABLE_TOF_ROBUST)
                g_distances_mm[TOF_FRONTAL_IDX] =
                    tof_zone_masked_robust(g_zones_mm[TOF_FRONTAL_IDX], TOF_RESOLUTION_ZONES, zmask,
                                           TOF_NO_READING, TOF_ROBUST_FIELD_MAX_MM, TOF_ROBUST_LOW_KEEP_PCT);
#else
                g_distances_mm[TOF_FRONTAL_IDX] =
                    tof_zone_masked_mean(g_zones_mm[TOF_FRONTAL_IDX], TOF_RESOLUTION_ZONES,
                                         zmask, TOF_NO_READING);
#endif
                g_last_ok_ms[TOF_FRONTAL_IDX] = millis();  // sello de frescura
                g_ever_ok[TOF_FRONTAL_IDX]    = true;
            }
            // Si getRangingData() devuelve false, dejamos el ultimo valor
            // cacheado — preferimos "ultimo dato bueno" a un sentinel que
            // dispare evasion espuria por UN frame perdido. Pero ya no para
            // siempre: tras TOF_STALE_TIMEOUT_MS el getter lo expira (P1-TOF-STALE).
        }
    }
#endif  // TOP_ENABLE_MULTI_TOF

#if defined(TOP_ENABLE_HCSR04) && defined(TOP_ENABLE_HCSR04_ASYNC)
    // HC-SR04 NO-BLOQUEANTE: dispara cuando toca y cosecha el resultado por poll. CERO espera
    // del eco en el loop (la ISR de ECHO mide el ancho) -> se elimina el spike de ~12 ms del
    // pulseIn que atrasaba el uplink @100Hz. Acceso a la FSM bajo seccion critica
    // (noInterrupts) porque la comparte la ISR de ECHO. A2.1: ultrasonido deshabilitado ->
    // ni dispara ni hace poll -> NO_READING.
    if (g_top_cfg.ultrasonic_en) {
        bool do_trig;
        noInterrupts();
        do_trig = hcsr04_due(g_hc_fsm, micros(), g_hc_cfg);
        interrupts();
        if (do_trig) {
            // Pulso de disparo: 10 us (NO el eco de 12 ms). Es lo unico "bloqueante" y es
            // 1000x mas corto que el pulseIn que reemplaza.
            digitalWrite(PIN_HCSR04_TRIG, HIGH);
            delayMicroseconds(10);
            digitalWrite(PIN_HCSR04_TRIG, LOW);
            noInterrupts();
            hcsr04_on_trig_sent(g_hc_fsm, micros());
            interrupts();
        }
        noInterrupts();
        g_hcsr04_mm = hcsr04_poll(g_hc_fsm, micros(), g_hc_cfg);  // distancia mm o NO_READING (timeout)
        interrupts();
    } else {
        g_hcsr04_mm = TOF_NO_READING;
    }
#elif defined(TOP_ENABLE_HCSR04)
    // HC-SR04 — lectura BLOQUEANTE (default historico). Corremos solo cada N ticks para no
    // saturar el loop (pulseIn puede tomar hasta ~12 ms). HC-SR04 ACTIVO en top_robot1/2
    // (TRIG=4 / ECHO=3, sin conflicto con UARTs).
    static uint32_t last_hc = 0;
    if (g_tick_count - last_hc >= 3) {
        // A2.1: si el ultrasonido está deshabilitado por config, NO_READING sin
        // leer (ahorra el pulseIn bloqueante de hasta 12 ms).
        g_hcsr04_mm = g_top_cfg.ultrasonic_en ? read_hcsr04() : TOF_NO_READING;
        last_hc = g_tick_count;
    }
#endif
}

// Distancia del ToF idx, ya filtrada por frescura (P1-TOF-STALE): si la ultima
// lectura buena venció (TOF_STALE_TIMEOUT_MS), devuelve TOF_NO_READING en vez del
// valor viejo. La decision la toma la funcion PURA host-testeada; aca solo le
// pasamos el estado + millis().
uint16_t sensors_tof_get_distance_mm(uint8_t idx) {
    if (idx >= NUM_TOF) return TOF_NO_READING;
    // A2.1: un ToF deshabilitado por config no reporta (min_obst y localización
    // ya tratan NO_READING como inválido). Default = todos habilitados.
    if (idx < TOP_CFG_NUM_TOF && !g_top_cfg.tof[idx].enabled) return TOF_NO_READING;
    return tof_fresh_or_no_reading(g_distances_mm[idx], g_last_ok_ms[idx],
                                   g_ever_ok[idx], millis(),
                                   TOF_STALE_TIMEOUT_MS);
}

// Zona cruda (0..15, grilla 4x4) del ToF idx, en mm. TOF_NO_READING si la zona
// no es válida, el sensor está deshabilitado por config, o venció (P1-TOF-STALE):
// reusa el getter de distancia como compuerta de frescura/habilitación.
uint16_t sensors_tof_get_zone_mm(uint8_t idx, uint8_t zone) {
    if (idx >= NUM_TOF || zone >= 16) return TOF_NO_READING;
    if (sensors_tof_get_distance_mm(idx) == TOF_NO_READING) return TOF_NO_READING;
    return g_zones_mm[idx][zone];
}

uint16_t sensors_hcsr04_get_distance_mm() { return g_hcsr04_mm; }

uint16_t sensors_tof_get_min_distance_mm() {
    const uint32_t now = millis();
    uint16_t min_d = TOF_NO_READING;
    for (int i = 0; i < NUM_TOF; ++i) {
        // Aplica el filtro de frescura por sensor: un ToF colgado en 80 mm ya no
        // domina el min_obstacle para siempre — expira a NO_READING y se ignora.
        const uint16_t d = tof_fresh_or_no_reading(
            g_distances_mm[i], g_last_ok_ms[i], g_ever_ok[i], now,
            TOF_STALE_TIMEOUT_MS);
        if (d != TOF_NO_READING && d < min_d) {
            min_d = d;
        }
    }
    if (g_hcsr04_mm != TOF_NO_READING && g_hcsr04_mm < min_d) {
        min_d = g_hcsr04_mm;
    }
    return min_d;
}

bool sensors_tof_is_ready(uint8_t idx) {
    return idx < NUM_TOF && g_ready[idx];
}

uint32_t sensors_tof_get_tick_count() { return g_tick_count; }

}  // namespace iitasoccer
