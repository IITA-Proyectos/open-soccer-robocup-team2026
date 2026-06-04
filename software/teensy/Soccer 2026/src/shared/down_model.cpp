#include "down_model.h"
#include "sensor_geometry.h"   // TEMA 3 P1 — SENSOR_POS[32] geometría real
#include <math.h>              // sqrtf, lroundf (WP-3-DOWN — cross_track/penetration mm)
namespace iitasoccer {

// ============================================================================
// WP-3-DOWN (Capa 3) — cross_track_mm y penetration_mm REALES desde geometría.
//
// Ambos se derivan del centroide y los radios de los sensores en blanco usando
// las posiciones físicas reales SENSOR_POS[] (sensor_geometry.h). Por eso SÓLO
// se computan cuando n == SENSOR_COUNT (32): con un anillo parcial (n<32) la LUT
// no mapea a los sensores presentes (ver la nota larga sobre lg_compute_xy más
// abajo) y emitir un número sería mentir → quedan en N/A.
//
// Convención de signo (fija, espeja al seguidor diag_central_line_sweep.cpp y a
// la glue DOWN comm_central.cpp que ya calculaba el centroide Y bajo debug):
//
//   cross_track_mm = centroide Y (mm) de los sensores en blanco.
//       +  => línea ADELANTE del centro del robot (+Y).
//       -  => línea ATRÁS (-Y).
//   Es la distancia perpendicular FIRMADA del centro (0,0) a la recta de la
//   línea en el caso de uso del arquero (línea lateral/de fondo que corre
//   left-right bajo el robot): ahí la recta es ~horizontal en el marco robot y
//   su distancia perpendicular al origen es exactamente su offset en Y. En
//   general es la proyección del centroide sobre el eje delantero/trasero, que
//   es la señal que el PID lateral del arquero necesita (setpoint=0 ⇒ mantiene
//   la línea bajo su eje). NOTA: la "recta" geométrica pura a través del
//   centroide tiene como normal la dirección del centroide mismo (= line_angle),
//   por lo que su distancia al origen degenera a |centroide| (sin signo); por
//   eso fijamos el eje de referencia (Y, delantero/trasero) para obtener una
//   señal firmada útil, en línea con el contrato §3.4 (referencia = borde/centro
//   del robot, no la propia normal de la línea).
//
//   penetration_mm = R_OUTER - min(radio de los sensores en blanco), clamp >=0.
//       R_OUTER = radio máximo del anillo (≈89.8 mm, sensor más externo).
//       0  => recién tocando (sólo el anillo externo ve blanco).
//       crece cuando sensores más internos (R menor) ven blanco ⇒ el robot está
//       más adentro de la zona de línea. Es mm reales derivados de la geometría,
//       NO el conteo de sensores (proxy viejo).
//
// `validated[]` que recibe esta función es EXACTAMENTE el mismo array (post
// salud + saturación) que alimenta lg_compute_xy, así el centroide aquí coincide
// con el que produjo line_angle (consistencia interna del frame).
// ============================================================================
namespace {

// Radio máximo del anillo (sensor más externo). Cacheado en la primera llamada;
// se deriva de SENSOR_POS[] para no hardcodear y seguir la geometría si cambia.
float dm_outer_radius_mm() {
    static float cached = -1.0f;
    if (cached < 0.0f) {
        float mx = 0.0f;
        for (int i = 0; i < SENSOR_COUNT; ++i) {
            const float x = SENSOR_POS[i].x_mm, y = SENSOR_POS[i].y_mm;
            const float r = sqrtf(x*x + y*y);
            if (r > mx) mx = r;
        }
        cached = mx;
    }
    return cached;
}

struct LineMetrics {
    bool     have;            // true si hubo >=1 sensor validado en blanco
    int16_t  cross_track_mm;  // centroide Y (mm), firmado (+adelante/-atrás)
    uint16_t penetration_mm;  // R_OUTER - K-ésimo-menor radio blanco, clamp >=0
};

// TEMA #21 (audit 2026-06-03) — penetration ROBUSTO a outliers.
// La penetración mide cuán adentro de la zona de línea está el robot. El proxy
// ingenuo (R_OUTER - min_radio_de_cualquier_validado) es sensible a 1-2 sensores
// internos espurios: bastan 2 internos adyacentes-por-índice que pasen el filtro
// espacial para llevar min_r de ~75mm (anillo externo) a ~35mm (anillo interno)
// y saltar penetration de ~15mm a ~55mm aunque el grueso de la línea esté afuera.
// En vez del mínimo crudo, exigimos que al menos PEN_MIN_INNER_SENSORS sensores
// validados estén a ese radio o más adentro: tomamos el K-ésimo radio MÁS CHICO
// (K = PEN_MIN_INNER_SENSORS). Así una línea genuina que atraviesa al centro
// (que enciende varios internos) sigue leyendo profundo, pero 1-2 internos
// ruidosos caen de vuelta al anillo externo. Si hay menos de K validados (línea
// mínima de 1-2 sensores), no se puede robustecer: caemos al mínimo real.
// Espeja la robustez de cross_track (promedio, no extremo).
constexpr int PEN_MIN_INNER_SENSORS = 3;

// Calcula cross_track/penetration sobre las posiciones reales. Requiere n==32
// (las posiciones de SENSOR_POS[i] sólo corresponden con el anillo completo).
LineMetrics dm_line_metrics(const bool* validated, int n) {
    LineMetrics out{false, LSV2_NA_I16, LSV2_NA_U16};
    if (n != SENSOR_COUNT) return out;   // anillo parcial: geometría no mapea
    double sum_y = 0.0;
    float  radii[SENSOR_COUNT];   // radios de los validados (para el K-ésimo menor)
    int    cnt = 0;
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (!validated[i]) continue;
        sum_y += SENSOR_POS[i].y_mm;
        const float x = SENSOR_POS[i].x_mm, y = SENSOR_POS[i].y_mm;
        radii[cnt] = sqrtf(x*x + y*y);
        ++cnt;
    }
    if (cnt == 0) return out;            // sin blancos validados → N/A
    out.have = true;
    const double cy = sum_y / (double)cnt;
    out.cross_track_mm = (int16_t)lroundf((float)cy);

    // K-ésimo radio más chico (K = PEN_MIN_INNER_SENSORS, clamp a cnt). Selección
    // parcial por inserción: cnt<=32 y K<=3, costo trivial. k_idx 0-based.
    const int k_idx = (cnt < PEN_MIN_INNER_SENSORS ? cnt : PEN_MIN_INNER_SENSORS) - 1;
    float kth = 1e9f;
    for (int pick = 0; pick <= k_idx; ++pick) {
        // Encontrar el mínimo de los restantes y "sacarlo" subiéndolo a +inf.
        int best = -1;
        float bestv = 1e9f;
        for (int j = 0; j < cnt; ++j) {
            if (radii[j] < bestv) { bestv = radii[j]; best = j; }
        }
        kth = bestv;
        if (best >= 0) radii[best] = 1e9f;   // marcar usado
    }

    float pen = dm_outer_radius_mm() - kth;
    if (pen < 0.0f) pen = 0.0f;          // clamp: nunca negativo
    out.penetration_mm = (uint16_t)lroundf(pen);
    return out;
}

}  // namespace

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
        ang[i]=lg_sensor_angle_deg(i,n);   // aproximación uniforme; sobreescrita con geometría real si n==32 (TEMA #16)
        carpet[i]=m.calib[i].carpet;  // snapshot PRE-adapt: sm_update ve el baseline ANTES del drift de este tick (no reordenar)
    }
    // TEMA #16 (audit 2026-06-03) — corner detection con geometría REAL.
    // Cuando n == 32, ang[] se llena con los ángulos físicos del PCB
    // (sg_fill_angles_deg) en vez del anillo ideal equidistante. line_angle ya
    // usaba geometría real (lg_compute_xy con SENSOR_POS), pero el flag corner
    // se calculaba sobre ángulos uniformes — una incoherencia: los 32 sensores
    // viven en 3 anillos de radios 37/54/80-87mm, no en un anillo perfecto, así
    // que las separaciones angulares reales difieren de los 11.25°/sensor
    // uniformes y un cluster real podía clasificarse mal. Con n<32 la LUT no
    // mapea el anillo parcial (igual que lg_compute_xy), así que se mantiene el
    // fallback uniforme ya cargado en el loop de arriba.
    if (n == SENSOR_COUNT) {
        sg_fill_angles_deg(ang, n);
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
    // lg_compute para tomar PRESTADO su flag corner. Con n==32 ese corner ya
    // se calcula sobre la geometría REAL (ang[] llenado con sg_fill_angles_deg
    // arriba), coherente con line_angle de lg_compute_xy (TEMA #16).
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
    //
    // TEMA #4 (audit 2026-06-03) — el gate anti-deriva usa white[] (POST
    // hysteresis, PRE filtro espacial), NO validated[]. Razón: validated[] exige
    // un vecino-de-array blanco (lf_spatial_filter), y la geometría del PCB tiene
    // 6 pares de índices contiguos que están físicamente LEJOS (idx 7↔8: 141mm;
    // 23↔24: 116mm; 31↔0: 102mm; …). Un sensor que ve blanco REAL pero cuyo
    // único vecino-de-índice no encendió (línea tangente, borde de franja, anillo
    // interno sin vecino contiguo) queda validated=false aunque white=true.
    // Con validated[] como gate, lc_adapt_carpet lo trataría como carpet y
    // arrastraría su lectura brillante hacia c.carpet (alpha=0.02), subiendo el
    // threshold hasta cegarlo a la línea — justo el modo de fallo que el gate
    // pretende evitar (caso arquero parkeado sobre la línea de fondo). El filtro
    // espacial es para el CENTROIDE (qué sensores entran al ángulo), NO para
    // decidir qué sensor contamina el baseline de carpet.
    //
    // Interplay con las invalidaciones de validated[] de arriba (todas neutras o
    // mejores con white[]): sensor unhealthy stuck-HIGH queda gated OUT de la
    // adaptación (mejor: no arrastra el carpet); stuck-LOW (white=false) sigue
    // adaptando hacia carpet (correcto). El "& !saturated" del if externo ya
    // cubre la saturación, así que white[i] solo a sensor individual.
    if (!saturated) {
        for(int i=0;i<n;++i)
            lc_adapt_carpet(m.calib[i], filt[i], white[i], cfg.adapt_alpha);
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
    // TEMA #25 (audit 2026-06-03) — line_present es PER-TICK, sin debounce
    // temporal sobre la señal AGREGADA. La única histéresis es per-sensor de
    // AMPLITUD (lf_hysteresis_on_white, band=±20 counts) + el filtro espacial
    // (exige vecino). NO hay un anti-flicker de N ticks sobre line_present: en el
    // borde de la franja, el conteo de validados puede cruzar 1↔0 vía sensores
    // distintos tick-a-tick. Hoy es aceptable porque ningún consumidor de
    // seguridad de CENTRAL usa line_present crudo (LINE_AVOID gatea por
    // EV_IMMINENT_EXIT con umbral 6; el strafe del arquero usa cross_track con
    // fallback limpio). Si un futuro consumidor necesita line_present estable en
    // el borde, agregar un debounce de N ticks DEBE gatear TODO el bloque de
    // geometría coherentemente (no solo el bit) para no mezclar campos de épocas
    // distintas — es decisión de diseño + tradeoff de latencia del fail-safe, NO
    // un cambio trivial. NOTA: los comentarios "con histéresis" de types.h y
    // line_view.h se refieren a la histéresis per-sensor, no a un debounce
    // temporal agregado.
    s.line_present = g.line_present ? 1 : 0;
    s.sensors_on_line = g.sensors_on_line;
    if(g.line_present){
        s.line_angle_centideg   = g.line_angle_centideg;
        s.escape_angle_centideg = g.escape_angle_centideg;
        // WP-3-DOWN: cross_track/penetration REALES en mm desde la geometría
        // (mismo `validated[]` que alimentó lg_compute_xy). N/A si n!=32 (anillo
        // parcial) o si no quedó ningún sensor validado. Ver dm_line_metrics().
        const LineMetrics lm = dm_line_metrics(validated, n);
        s.penetration_mm = lm.have ? lm.penetration_mm : LSV2_NA_U16;
        s.cross_track_mm = lm.have ? lm.cross_track_mm : LSV2_NA_I16;
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
    if(n<SENSOR_COUNT) ev|=EV_DEGRADED_GEOMETRY;  // anillo parcial: mux muerto o rig reducido
    s.event_flags=ev;
    s.quality = s.data_valid ? (uint8_t)(g.line_present? 85 : 95) : 0; // placeholder: metrica real (SNR) diferida a Plan 3
    s.sample_age_ms = 0;   // lo setea el glue HW (comm) con el delta real antes de framing
    s.reserved = 0;
    return s;
}
}  // namespace iitasoccer
