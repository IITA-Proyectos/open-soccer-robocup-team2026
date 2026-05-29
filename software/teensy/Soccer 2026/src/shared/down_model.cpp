#include "down_model.h"
#include "sensor_geometry.h"   // TEMA 3 P1 — SENSOR_POS[32] geometría real
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

    // TEMA P1.5 — rechazo de saturación "todo blanco" (audit 2026-05-29).
    // Si >= 7/8 del anillo lee blanco NO es una línea real (una franja
    // enciende a lo sumo ~15/32 sensores). Es falla: calib rota, superficie
    // toda-brillante o luz ambiente saturando. Zeroeamos validated[] para que
    // la geometría produzca line_present=0 de forma natural (sin sprinkling de
    // !saturated por toda la salida) y NO adaptamos calib en este tick (no
    // queremos "aprender" valores saturados como baseline). Umbral 7/8 espeja
    // al detector de lifted (su opuesto: lifted = ~todo OSCURO).
    const bool saturated = lf_all_white(white, n, (n * 7) / 8);
    if (saturated) {
        for (int i = 0; i < n; ++i) validated[i] = false;
    }

    // TEMA 3 P1 — geometría REAL del PCB cuando n == SENSOR_COUNT (32).
    // Cuando n == 32 usamos lg_compute_xy con las coordenadas (x, y) reales
    // del schematic (validadas empíricamente 2026-05-24). Es más correcto
    // que el centroide angular uniforme porque los 32 sensores del PCB DOWN
    // viven en 3 anillos con radios distintos (R ≈ 37, 54, 80-87 mm), NO en
    // un anillo perfecto equidistante.
    //
    // Cuando n < 32 (anillo parcial: 1-3 muxes conectados o mux muerto),
    // SENSOR_POS[0..n-1] NO refleja la geometría correcta (la LUT asume los
    // 32 sensores físicos en su orden de PCB; con n<32 los sensores que
    // faltan no son los del FINAL sino los del medio/principio según qué
    // muxes estén conectados). Fallback al centroide angular uniforme, que
    // al menos no introduce sesgos sistemáticos.
    //
    // lg_compute_xy NO computa corner detection (la lógica de "2 clusters
    // separados ~90°" requiere ángulos discretos). Llamamos también
    // lg_compute para tomar PRESTADO su flag corner.
    GeomResult g_ang = lg_compute(validated, ang, n);
    GeomResult g;
    if (n == SENSOR_COUNT) {
        g = lg_compute_xy(validated, SENSOR_POS, n);
        g.corner = g_ang.corner;
    } else {
        g = g_ang;
    }

    // No adaptar calib en ticks saturados: validated[] ya está en cero y
    // adaptar nudgearía el carpet hacia los valores blancos (corrompería el
    // baseline). Saltamos el drift mientras dura la falla.
    if (!saturated) {
        for(int i=0;i<n;++i)
            lc_adapt_carpet(m.calib[i], filt[i], validated[i], cfg.adapt_alpha);
    }
    bool suspect = lc_is_suspect(m.calib, n, cfg.calib_min_margin);
    bool lifted  = sm_update(m.surface, filt, carpet, n, now_ms,
                             cfg.lifted_debounce_ms,
                             cfg.lifted_min_sensors, cfg.lifted_delta_below);
    bool line_end = lt_update(m.tracker, g.line_present, now_ms,
                              cfg.line_end_min_track_ms);

    // Fix audit 2026-05-29: data_valid también debe ser false si hay mux muerto.
    // Si un mux entero (8 sensores) está colgado, el centroide está sesgado
    // sistemáticamente — no confiar en el ángulo. EV_SENSOR_NOISY individual NO
    // invalida data_valid porque el sensor ruidoso ya se EXCLUYÓ del centroide
    // (sólo afecta a UN sensor, no a 8 como mux_dead).
    const bool any_mux_dead = mw_any_dead(m.mux_watchdog);

    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = (sm_data_valid(lifted, suspect) && !any_mux_dead && !saturated) ? 1 : 0;
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
    if(suspect || saturated) ev|=EV_CALIB_SUSPECT;  // saturación todo-blanco reusa este flag (sin bit libre en el contrato de 16 bytes)
    if(any_mux_dead) ev|=EV_MUX_DEAD;   // TEMA 1 P0 — 2026-05-29 (cacheado arriba)
    if(sh_any_unhealthy(m.sensor_health, n)) ev|=EV_SENSOR_NOISY;  // TEMA 4 P1 — 2026-05-29
    if(n<32)       ev|=EV_DEGRADED_GEOMETRY;  // anillo parcial: mux muerto o rig reducido
    s.event_flags=ev;
    s.quality = s.data_valid ? (uint8_t)(g.line_present? 85 : 95) : 0; // placeholder: metrica real (SNR) diferida a Plan 3
    s.sample_age_ms = 0;   // lo setea el glue HW (comm) con el delta real antes de framing
    s.reserved = 0;
    return s;
}
}  // namespace iitasoccer
