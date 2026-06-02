// sensors_tof.cpp — Implementacion VIVA del modulo ToF + HC-SR04 del TOP.
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
// Estado del hardware (al 2026-05-24):
//   • Solo el ToF frontal U2 esta fisicamente soldado (Wire / I2C0 / addr 0x29).
//   • U3, U5, U17 son slots vacios -> get_distance_mm(1..3) retorna
//     TOF_NO_READING permanente, is_ready(1..3) = false.
//   • HC-SR04 frontal (TRIG/ECHO) funciona como antes.
// ============================================================================

#include "sensors_tof.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>

namespace iitasoccer {

namespace {

// ----- Estado del modulo -----
uint16_t g_distances_mm[NUM_TOF];
bool     g_ready[NUM_TOF];
uint16_t g_hcsr04_mm = TOF_NO_READING;
uint32_t g_tick_count = 0;

// ----- Sensor real (solo el frontal U2 esta instalado) -----
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
// HC-SR04 — DESHABILITADO POR DEFAULT (pulseIn bloqueante; el ToF frontal lo cubre).
// ----------------------------------------------------------------------------
// Cableado en banco 2026-05-31: TRIG=pin 4, ECHO=pin 3 (pines ex-XSHUT ToF, hoy
// libres; NO son UART). Esto RESUELVE el viejo "conflicto de pin 7" (antes el ECHO
// estaba en pin 7 = Serial2 RX2): el HC-SR04 ya no comparte pin con ningun Serial.
// Aun asi queda gateado OFF por default por la OTRA razon, que sigue vigente:
//   pulseIn() BLOQUEA hasta 25 ms esperando el echo. A ~90 ms de cadencia eso
//   roba 25 ms al loop y degrada el uplink de 100 Hz (P0: TASK-014).
// Ademas el ToF frontal ya aporta "distancia frontal", asi que el HC-SR04 es
// redundante hoy. Para reactivarlo (ya sin riesgo de pin): compilar con
// -DTOP_ENABLE_HCSR04, idealmente despues de hacerlo NO bloqueante. Sin ese flag,
// el modulo no toca los pines ni llama a pulseIn, y devuelve TOF_NO_READING.
#ifdef TOP_ENABLE_HCSR04
// HC-SR04 — lectura bloqueante, simple (sin cambios desde el stub).
uint16_t read_hcsr04() {
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // pulseIn con timeout — si no llega echo, retorna 0.
    const uint32_t duration_us = pulseIn(PIN_HCSR04_ECHO, HIGH, 25000UL);  // 25ms = ~4m
    if (duration_us == 0) return TOF_NO_READING;
    // Velocidad del sonido: 343 m/s = 0.343 mm/µs. Duracion es ida + vuelta.
    return static_cast<uint16_t>((duration_us * 343UL) / 2000UL);
}
#endif  // TOP_ENABLE_HCSR04

// Promedia las zonas validas del frame 4x4 del L7CX. status==5/6/9 son
// "valid range" segun convencion ST. Devuelve TOF_NO_READING si NINGUNA
// zona del frame es valida (sensor mirando al vacio, fuera de rango, etc).
uint16_t mean_valid_zones(const VL53L7CX_ResultsData& r, uint8_t n_zones) {
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

}  // namespace

// Duerme los 4 ToF (LP low) para dejar el bus I2C limpio ANTES de iniciar el BNO.
// Receta validada en diag_pose_live: (1) dim ToF -> (2) init BNO -> (3) enumerar ToF.
// Sin esto, el BNO se inicia con los ToF DESPIERTOS en 0x29 (misma dir que el BNO
// derecho) -> el/los BNO no aparecen. Llamar en setup() ANTES de sensors_imu_init().
void sensors_tof_predim_lp() {
#ifdef TOP_ENABLE_MULTI_TOF
    Wire.begin();
    Wire.setClock(400000);  // 400 kHz: bus sano sin el 2do BNO fallado (ver sensors_imu.cpp).
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
    Wire.setClock(400000);   // idempotente; 400 kHz (bus sano, ver predim/imu).
#ifdef TOP_ENABLE_HCSR04
    // HC-SR04 frontal — solo si se reactivo explicitamente (ver nota arriba).
    // Pines 4/3 (libres); el conflicto de pin 7 ya no aplica.
    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    pinMode(PIN_HCSR04_ECHO, INPUT);
#endif

    for (int i = 0; i < NUM_TOF; ++i) {
        g_distances_mm[i] = TOF_NO_READING;
        g_ready[i] = false;
    }

    // NOTA: NO tocamos los pines XSHUT (PIN_TOF_XSHUT[]). Validado el
    // 2026-05-24: la lib Adafruit no los necesita (maneja el chip por I2C),
    // y en la placa actual esos pines probablemente ni siquiera estan
    // ruteados al modulo Pololu. Cuando lleguen los otros 3 ToF y haya que
    // enumerar 4 sensores en la misma direccion default, se agrega la
    // logica de XSHUT + setAddress() aca.

    // NOTA: NO tocamos `Wire.setClock()`. El main_top o cualquier otro
    // consumidor del bus puede setear el clock una vez al boot (default
    // 400 kHz = Adafruit default tambien). Si se setea aca y otro modulo
    // del TOP necesita otro clock, se pisan.

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
        if (!g_tof_multi[i].begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, 400000)) continue;  // 400 kHz: bus sano (quad_live = 4/4)
        if (!g_tof_multi[i].setAddress(TOF_I2C_ADDR_ASSIGNED[i]))           continue;
        g_tof_multi[i].setResolution(TOF_RESOLUTION_ZONES);
        g_tof_multi[i].setRangingFrequency(TOF_RANGING_FREQ_HZ);
        if (!g_tof_multi[i].startRanging())                                 continue;
        g_ready[i] = true;              // queda despierto (retiene dir + rangea)
    }
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
    if (!g_tof_frontal.begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, 400000)) {
        if (!g_tof_init_logged) {
            Serial.println(F("[sensors_tof] WARN: VL53L7CX U2 begin() fallo; "
                             "se sigue sin ToF frontal."));
            g_tof_init_logged = true;
        }
        // No retornamos false — el HC-SR04 puede seguir funcionando y el
        // resto del firmware del TOP necesita seguir corriendo.
        return true;
    }
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
    // Poll de los 4 ToF enumerados (no bloqueante).
    for (int i = 0; i < NUM_TOF; ++i) {
        if (!g_ready[i]) continue;
        if (g_tof_multi[i].isDataReady() &&
            g_tof_multi[i].getRangingData(&g_tof_results)) {
            g_distances_mm[i] = mean_valid_zones(g_tof_results, TOF_RESOLUTION_ZONES);
        }
        // si getRangingData() devuelve false, mantenemos el ultimo valor cacheado.
    }
#else
    if (g_ready[TOF_FRONTAL_IDX]) {
        if (g_tof_frontal.isDataReady()) {
            if (g_tof_frontal.getRangingData(&g_tof_results)) {
                g_distances_mm[TOF_FRONTAL_IDX] =
                    mean_valid_zones(g_tof_results, TOF_RESOLUTION_ZONES);
            }
            // Si getRangingData() devuelve false, dejamos el ultimo valor
            // valido cacheado — preferimos "ultimo dato bueno" a un sentinel
            // que dispare evasion espuria por un frame perdido.
        }
    }
#endif  // TOP_ENABLE_MULTI_TOF

#ifdef TOP_ENABLE_HCSR04
    // HC-SR04 — lectura bloqueante. Corremos solo cada N ticks para no
    // saturar el loop (cada lectura puede tomar hasta 25ms).
    // OJO: deshabilitado por default — conflicto pin 7 con Serial2 (ver arriba).
    static uint32_t last_hc = 0;
    if (g_tick_count - last_hc >= 3) {
        g_hcsr04_mm = read_hcsr04();
        last_hc = g_tick_count;
    }
#endif
}

uint16_t sensors_tof_get_distance_mm(uint8_t idx) {
    if (idx >= NUM_TOF) return TOF_NO_READING;
    return g_distances_mm[idx];
}

uint16_t sensors_hcsr04_get_distance_mm() { return g_hcsr04_mm; }

uint16_t sensors_tof_get_min_distance_mm() {
    uint16_t min_d = TOF_NO_READING;
    for (int i = 0; i < NUM_TOF; ++i) {
        if (g_distances_mm[i] != TOF_NO_READING && g_distances_mm[i] < min_d) {
            min_d = g_distances_mm[i];
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
