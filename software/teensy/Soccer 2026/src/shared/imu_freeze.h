// imu_freeze.h — Detector de BNO CONGELADO (PURO, host-testeable).
//
// Por qué existe (audit 2026-06-04, único HIGH)
// ---------------------------------------------
// imu_fusion ya degrada un BNO a DEAD cuando deja de hacer ACK en el bus
// (`present=false` por N ciclos). PERO un BNO puede MORIR SIN dejar de ackear:
// el getVector() del Adafruit_BNO055 devuelve exactamente (0,0,0) ante un fallo
// de lectura I²C, y el chip puede quedar "vivo en el bus pero con el yaw clavado"
// (banco 2026-06-02: a 400 kHz, o a 100 kHz leyendo el BNO muy seguido, el read
// multi-byte choca con los ToF y el YAW SE CONGELA aunque el sensor siga
// ackeando). Hoy el robot corre con 1 SOLO BNO sano (0x28) — es el único sensor
// de heading. Si ese heading se congela a mitad de partido, imu_fusion lo sigue
// reportando present=true para siempre y el heading muerto pasa como VÁLIDO.
//
// Este módulo detecta ese caso: una secuencia de lecturas de heading cuyo valor
// es BIT-IDÉNTICO (mismo centideg exacto) por >= N lecturas consecutivas durante
// >= T ms = sospecha de congelamiento. La clave es la igualdad EXACTA: un BNO
// vivo, incluso con el robot perfectamente quieto, jitterea al menos ±1 LSB de
// centideg por ruido del sensor; un valor clavado al centidegrado EXACTO muchos
// ticks es el patrón del (0,0,0) por fallo I²C / yaw congelado, no el de un
// robot quieto.
//
// CÓMO SE USA: el wrapper hardware (sensors_imu.cpp) le pasa el heading en
// centideg + el timestamp de cada lectura; el módulo devuelve true cuando cree
// que ese sensor está congelado. La ACCIÓN (bajar present→false para que
// imu_fusion lo marque DEAD) la toma el wrapper, GATED detrás de
// TOP_ENABLE_BNO_FREEZE_DETECT (default OFF) hasta validar en banco que no da
// falsos-DEAD con el robot quieto.
//
// POR QUÉ centideg (int16) y no float: es la unidad de transporte del firmware
// (sensors_imu_get_heading_centideg) y la comparación de igualdad es EXACTA y
// barata (sin epsilon de float). Bit-idéntico = mismo entero.
//
// PURO: sin Arduino/Wire. POD + funciones libres → .bss zero-init y g++ host.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Defaults CONSERVADORES (a confirmar en banco — ver pending_bench).
// N grande y T largo => prefiere FALSOS NEGATIVOS (no detectar) antes que
// falsos positivos (matar un BNO sano con el robot quieto). Mejor tardar en
// reaccionar que tirar el único heading por un quieto prolongado legítimo.
//
// A 20 Hz de lectura del BNO (BNO_READ_INTERVAL_MS=50 en sensors_imu.cpp),
// 40 lecturas ≈ 2 s. Pedimos AMBOS umbrales (N y T) para que la tasa de lectura
// real no cambie la sensibilidad: si el wrapper lee más lento, N se alcanza en
// más tiempo igual; si lee más rápido, T sigue pidiendo el tiempo mínimo.
constexpr uint16_t IMU_FREEZE_MIN_SAMPLES = 40;     // N: lecturas idénticas consecutivas
constexpr uint32_t IMU_FREEZE_MIN_MS      = 1500;   // T: durante al menos este tiempo

// Config del detector (overridable; defaults arriba).
struct ImuFreezeCfg {
    uint16_t min_samples;   // N lecturas bit-idénticas consecutivas
    uint32_t min_ms;        // T ms que debe sostenerse el valor clavado
};

inline ImuFreezeCfg imu_freeze_default_cfg() {
    ImuFreezeCfg c;
    c.min_samples = IMU_FREEZE_MIN_SAMPLES;
    c.min_ms      = IMU_FREEZE_MIN_MS;
    return c;
}

// Deriva una config desde la tasa de lectura real del BNO y un umbral de tiempo
// (ms), en vez de un conteo crudo de lecturas. Definida en imu_freeze.cpp.
// Nunca baja de 2 lecturas ni por debajo del piso conservador del default.
ImuFreezeCfg imu_freeze_cfg_from_rate(uint32_t read_interval_ms, uint32_t min_ms);

// Estado por sensor (persiste entre lecturas). Zero-init (.bss) == estado
// inicial válido: seen=false → la primera lectura solo siembra la referencia.
struct ImuFreezeState {
    bool     seen;          // ¿ya hubo una primera lectura que sembró la referencia?
    int16_t  last_value;    // último heading observado (centideg)
    uint32_t first_ms;      // timestamp de cuándo el valor actual apareció por 1ª vez
    uint16_t streak;        // cuántas lecturas idénticas consecutivas llevamos (incl. la 1ª)
    bool     frozen;        // veredicto latcheado: el sensor está congelado
};

// Lo deja en estado limpio (no congelado, sin referencia). Llamar al boot y
// tras cualquier re-init/re-cero real del sensor (un re-cero cambia el valor a
// propósito y NO debe contar como descongelamiento espurio).
inline void imu_freeze_reset(ImuFreezeState& s) {
    s.seen = false;
    s.last_value = 0;
    s.first_ms = 0;
    s.streak = 0;
    s.frozen = false;
}

// Alimenta una lectura de heading (centideg) con su timestamp (ms).
// Devuelve true si el sensor se considera CONGELADO en este instante.
//
// Lógica:
//   • Valor distinto al anterior  => el sensor se movió: reinicia la racha,
//     limpia el veredicto (descongelado), siembra nueva referencia.
//   • Valor BIT-IDÉNTICO al anterior => extiende la racha. Si la racha llega a
//     >= N lecturas Y el valor lleva clavado >= T ms => CONGELADO (latch true).
//
// El latch se mantiene mientras el valor siga clavado; en cuanto cambie un solo
// centideg, se limpia. El wrapper puede actuar (bajar present) mientras esté true.
inline bool imu_freeze_update(ImuFreezeState& s, int16_t heading_centideg,
                              uint32_t now_ms, const ImuFreezeCfg& cfg) {
    if (!s.seen) {
        // Primera lectura: solo siembra la referencia (racha = 1).
        s.seen = true;
        s.last_value = heading_centideg;
        s.first_ms = now_ms;
        s.streak = 1;
        s.frozen = false;
        return false;
    }

    if (heading_centideg != s.last_value) {
        // El heading cambió => el sensor está vivo. Reinicia todo.
        s.last_value = heading_centideg;
        s.first_ms = now_ms;
        s.streak = 1;
        s.frozen = false;
        return false;
    }

    // Mismo valor exacto que la lectura anterior: extiende la racha (con
    // saturación para no wrappear el contador en quietos muy largos).
    if (s.streak < 0xFFFF) s.streak++;

    // Veredicto: hace falta N lecturas idénticas Y T ms sostenidos. Ambos.
    const uint32_t held_ms = now_ms - s.first_ms;   // wrap-safe (resta unsigned)
    if (s.streak >= cfg.min_samples && held_ms >= cfg.min_ms) {
        s.frozen = true;
    }
    return s.frozen;
}

}  // namespace iitasoccer
