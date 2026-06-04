// imu_freeze.cpp — Detector de BNO congelado. Ver imu_freeze.h para el diseño.
//
// La lógica de detección vive INLINE en el header (POD + funciones libres, igual
// que otos_health.h): es barata, se ejecuta en el hot-path del wrapper y conviene
// que el compilador la inline. Esta unidad de traducción existe para:
//   • cumplir el contrato del módulo .{h,cpp},
//   • ofrecer un helper NO-inline para configurar el detector a una tasa de
//     lectura conocida (sirve al wrapper hardware y queda host-testeable),
//   • verificar de paso que el header compila solo, sin Arduino (host-safe).

#include "imu_freeze.h"

namespace iitasoccer {

// Construye una config a partir de la tasa de lectura real del BNO y dos límites
// expresados en TIEMPO, en vez de en cuentas crudas de lecturas. Útil porque el
// wrapper conoce su intervalo de lectura (BNO_READ_INTERVAL_MS=50 → 20 Hz) pero
// pensar el umbral en milisegundos es más intuitivo que en "N lecturas".
//
//   read_interval_ms : período entre lecturas del BNO (ms). 0 => usa defaults.
//   min_ms           : tiempo mínimo que el valor debe estar clavado (T).
//
// min_samples se deriva como ceil(min_ms / read_interval_ms) pero NUNCA baja de
// 2 (una sola lectura no puede probar "idéntico a la anterior") ni del piso
// conservador IMU_FREEZE_MIN_SAMPLES, para no volver el detector más sensible
// que el default por accidente. Mantiene la política: ante la duda, NO detectar.
ImuFreezeCfg imu_freeze_cfg_from_rate(uint32_t read_interval_ms, uint32_t min_ms) {
    ImuFreezeCfg c = imu_freeze_default_cfg();
    c.min_ms = min_ms;
    if (read_interval_ms > 0) {
        uint32_t n = (min_ms + read_interval_ms - 1) / read_interval_ms;  // ceil
        if (n < 2) n = 2;
        if (n > 0xFFFF) n = 0xFFFF;
        // No aflojar por debajo del piso conservador del default.
        if (n < c.min_samples) n = c.min_samples;
        c.min_samples = static_cast<uint16_t>(n);
    }
    return c;
}

}  // namespace iitasoccer
