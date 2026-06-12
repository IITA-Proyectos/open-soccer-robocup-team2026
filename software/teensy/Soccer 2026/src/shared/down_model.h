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
    // Tolerancia de sensores DÉBILES (banco 2026-06-12): hasta este número de
    // sensores con margen < calib_min_margin se EXCLUYEN individualmente del
    // centroide (patrón EV_SENSOR_NOISY) y el frame sigue data_valid=1 con
    // EV_CALIB_SUSPECT prendido ("geometría degradada", contrato §3.2). Con MÁS
    // débiles que esto → data_valid=0 (la calib no sirve). El default `= 0`
    // (inicializador de miembro, C++14) preserva la semántica HISTÓRICA
    // (cualquier débil invalida todo) en TODO inicializador viejo — incluso
    // `DownModelCfg cfg;` sin llaves (los fixtures de tests hacen eso).
    int      max_weak_sensors = 0;
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
