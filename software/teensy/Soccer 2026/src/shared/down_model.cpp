#include "down_model.h"
namespace iitasoccer {
LineStatusV2 dm_update(DownModel& m, const DownModelCfg& cfg,
                        const uint16_t* raw, int n, uint32_t now_ms){
    if(n>DM_MAX_SENSORS) n=DM_MAX_SENSORS;
    uint16_t filt[DM_MAX_SENSORS]; bool white[DM_MAX_SENSORS];
    float ang[DM_MAX_SENSORS]; uint16_t carpet[DM_MAX_SENSORS];
    for(int i=0;i<n;++i){
        filt[i]=lf_temporal_update(m.filt[i], raw[i]);
        white[i]=lf_hysteresis_on_white(filt[i], m.calib[i].threshold,
                                         m.was_white[i]);
        m.was_white[i]=white[i];
        ang[i]=lg_sensor_angle_deg(i,n);
        carpet[i]=m.calib[i].carpet;  // snapshot PRE-adapt: sm_update ve el baseline ANTES del drift de este tick (no reordenar)
    }
    bool validated[DM_MAX_SENSORS];
    lf_spatial_filter(white, validated, n);

    // TEMA 1 P0 — actualizar MuxWatchdog con valores RAW (no filtered).
    // Queremos detectar mux pegado a nivel hardware, antes del suavizado.
    // n_muxes derivado de n_sensores / MW_SENSORS_PER_MUX. Si n no es múltiplo
    // exacto, redondeamos hacia abajo (no monitoreamos mux parcial).
    const int n_muxes = (n / MW_SENSORS_PER_MUX);
    if (n_muxes > 0) {
        mw_update(m.mux_watchdog, raw, n_muxes, MW_SENSORS_PER_MUX, now_ms);
    }

    // TEMA 4 P1 — tracking de salud per-sensor (transiciones + stuck).
    // Usamos `white` (POST-hysteresis pre-spatial-filter) para contar
    // transiciones blanco↔no-blanco (no validated, que ya está filtrado
    // espacialmente y podría enmascarar oscilación real del sensor).
    sh_update(m.sensor_health, raw, white, n, now_ms);

    // Si un sensor está marcado unhealthy, lo excluimos del centroide.
    // Esto previene que un sensor ruidoso/stuck contamine el ángulo.
    for (int i = 0; i < n; ++i) {
        if (!sh_is_healthy(m.sensor_health, i)) validated[i] = false;
    }

    GeomResult g = lg_compute(validated, ang, n);

    for(int i=0;i<n;++i)
        lc_adapt_carpet(m.calib[i], filt[i], validated[i], cfg.adapt_alpha);
    bool suspect = lc_is_suspect(m.calib, n, cfg.calib_min_margin);
    bool lifted  = sm_update(m.surface, filt, carpet, n, now_ms,
                             cfg.lifted_debounce_ms,
                             cfg.lifted_min_sensors, cfg.lifted_delta_below);
    bool line_end = lt_update(m.tracker, g.line_present, now_ms,
                              cfg.line_end_min_track_ms);

    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = sm_data_valid(lifted, suspect);
    s.line_present = g.line_present ? 1 : 0;
    s.sensors_on_line = g.sensors_on_line;
    if(g.line_present){
        s.line_angle_centideg   = g.line_angle_centideg;
        s.escape_angle_centideg = g.escape_angle_centideg;
        s.penetration_mm = (uint16_t)(g.sensors_on_line); // PROXY: conteo, NO mm — geometria real diferida a Plan 3
        s.cross_track_mm = LSV2_NA_I16;                    // requiere ref-edge fisico — Plan 3
    } else {
        s.line_angle_centideg=LSV2_NA_I16; s.escape_angle_centideg=LSV2_NA_I16;
        s.penetration_mm=LSV2_NA_U16; s.cross_track_mm=LSV2_NA_I16;
    }
    uint8_t ev=0;
    if(g.line_present && g.sensors_on_line>=cfg.imminent_depth) ev|=EV_IMMINENT_EXIT;
    if(g.corner)   ev|=EV_CORNER;
    if(line_end)   ev|=EV_LINE_END;
    if(lifted)     ev|=EV_LIFTED;
    if(suspect)    ev|=EV_CALIB_SUSPECT;
    if(mw_any_dead(m.mux_watchdog)) ev|=EV_MUX_DEAD;   // TEMA 1 P0 — 2026-05-29
    if(sh_any_unhealthy(m.sensor_health, n)) ev|=EV_SENSOR_NOISY;  // TEMA 4 P1 — 2026-05-29
    if(n<32)       ev|=EV_DEGRADED_GEOMETRY;  // anillo parcial: mux muerto o rig reducido
    s.event_flags=ev;
    s.quality = s.data_valid ? (uint8_t)(g.line_present? 85 : 95) : 0; // placeholder: metrica real (SNR) diferida a Plan 3
    s.sample_age_ms = 0;   // lo setea el glue HW (comm) con el delta real antes de framing
    s.reserved = 0;
    return s;
}
}  // namespace iitasoccer
