// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
#include "types.h"
#include "line_filters.h"
#include "line_geometry.h"
#include "line_tracker.h"
#include "line_calib.h"
#include "surface_monitor.h"
#include "sensor_health.h"   // TEMA 4 P1 — 2026-05-29
namespace iitasoccer {
constexpr int DM_MAX_SENSORS = 32;
struct DownModelCfg {
    int      imminent_depth;
    float    adapt_alpha;
    uint16_t calib_min_margin;
    uint32_t lifted_debounce_ms;
    int      lifted_min_sensors;
    uint16_t lifted_delta_below;
    uint32_t line_end_min_track_ms;
};
struct DownModel {
    SensorCalib    calib[DM_MAX_SENSORS];
    FilterBuffer   filt[DM_MAX_SENSORS];
    bool           was_white[DM_MAX_SENSORS];
    SurfaceMonitor surface;
    LineTracker    tracker;
    MuxWatchdog    mux_watchdog;     // TEMA 1 P0 — 2026-05-29 (EV_MUX_DEAD)
    SensorHealth   sensor_health;    // TEMA 4 P1 — 2026-05-29 (EV_SENSOR_NOISY)
};
LineStatusV2 dm_update(DownModel& m, const DownModelCfg& cfg,
                        const uint16_t* raw, int n, uint32_t now_ms);
}  // namespace iitasoccer
