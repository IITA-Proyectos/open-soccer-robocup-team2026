// imu_fusion.cpp — Implementación de la fusión inteligente de 2 BNO055.
// Ver imu_fusion.h para el diseño y qué hace / qué NO hace.

#include "imu_fusion.h"
#include <cmath>

namespace iitasoccer {

ImuFusionCfg imu_fusion_default_cfg() {
    ImuFusionCfg c;
    c.disagree_max_deg   = 30.0f;   // > 30° => impacto/falla, no promediar
    c.rest_gyro_dps      = 1.5f;    // |gyro| < 1.5°/s en ambos => quieto
    c.glitch_factor      = 4.0f;    // salto 4x lo predicho por el gyro => glitch
    c.glitch_margin_deg  = 6.0f;    // + 6° de margen fijo (ruido normal)
    c.glitch_max_streak  = 3;       // 3 glitches seguidos => era real, aceptar
    c.drift_reset_dps    = 0.8f;    // drift en reposo > 0.8°/s sostenido => mal
    c.drift_reset_ms     = 4000;    // ...por 4 s => pedir reset del sensor
    c.dead_after_misses  = 5;       // 5 ciclos sin ACK => DEAD
    c.degraded_weight    = 0.25f;   // un DEGRADED pesa 1/4 de un OK
    return c;
}

ImuSensorCfg imu_fusion_default_sensor_cfg() {
    ImuSensorCfg c;
    c.enabled          = true;
    c.mount_offset_deg = 0.0f;
    c.min_calib_gyro   = 1;     // calib gyro >= 1 para ser OK (0 = DEGRADED)
    c.base_weight      = 1.0f;
    return c;
}

float imu_norm180(float deg) {
    while (deg > 180.0f)  deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

float imu_diff(float a_deg, float b_deg) { return imu_norm180(a_deg - b_deg); }

// Promedio circular pesado de 2 ángulos (suma de vectores unitarios pesados).
// Maneja el wraparound: el promedio de 179 y -179 da ±180, no 0.
float imu_circular_mean(float a, float b, float wa, float wb) {
    const float ar = a * static_cast<float>(M_PI) / 180.0f;
    const float br = b * static_cast<float>(M_PI) / 180.0f;
    const float x = wa * std::cos(ar) + wb * std::cos(br);
    const float y = wa * std::sin(ar) + wb * std::sin(br);
    if (x == 0.0f && y == 0.0f) return a;  // degenerado (opuestos, mismo peso)
    return std::atan2(y, x) * 180.0f / static_cast<float>(M_PI);
}

void imu_fusion_init(ImuFusion& f) {
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        ImuSensorState& s = f.s[i];
        s.health        = ImuHealth::DEAD;
        s.heading_deg   = 0.0f;
        s.drift_dps     = 0.0f;
        s.drift_accum_ms = 0;
        s.miss_count    = 0;
        s.glitch_streak = 0;
        s.glitched      = false;
        s.request_reset = false;
        s.reseed_heading = 0.0f;
        s.seen          = false;
    }
    f.fused_heading_deg = 0.0f;
    f.fused_valid       = false;
    f.disagreement_deg  = 0.0f;
    f.impact_detected   = false;
}

namespace {

// Peso de un sensor según salud + calibración + peso base de su config.
float sensor_weight(const ImuSensorState& s, const ImuSample& smp,
                    const ImuSensorCfg& cfg, const ImuFusionCfg& fcfg) {
    float w = cfg.base_weight;
    if (s.health == ImuHealth::DEGRADED) w *= fcfg.degraded_weight;
    // (calib 0..3) -> factor 0.25..1.0; un sensor mejor calibrado pesa más.
    w *= (static_cast<float>(smp.calib_gyro) + 1.0f) / 4.0f;
    if (w < 0.0001f) w = 0.0001f;
    return w;
}

}  // namespace

void imu_fusion_update(ImuFusion& f,
                       const ImuFusionCfg& cfg,
                       const ImuSensorCfg sensor_cfg[IMU_FUSION_N],
                       const ImuSample in[IMU_FUSION_N],
                       float dt_s) {
    const uint32_t dt_ms = (dt_s > 0.0f) ? static_cast<uint32_t>(dt_s * 1000.0f + 0.5f) : 0;

    // ---- Paso A: por sensor (salud, glitch) ----
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        ImuSensorState& s = f.s[i];
        const ImuSample& smp = in[i];
        const ImuSensorCfg& scfg = sensor_cfg[i];
        s.glitched = false;

        if (!scfg.enabled) { s.health = ImuHealth::DEAD; continue; }

        if (!smp.present) {
            if (s.miss_count < 0xFFFF) ++s.miss_count;
            if (s.miss_count >= cfg.dead_after_misses) s.health = ImuHealth::DEAD;
            continue;  // sin dato: no tocar heading
        }
        s.miss_count = 0;

        const float h = imu_norm180(smp.heading_deg + scfg.mount_offset_deg);

        // Glitch: ¿el salto de heading concuerda con lo que dice el giroscopio?
        if (s.seen) {
            const float predicted = smp.gyro_z_dps * dt_s;
            const float actual    = imu_diff(h, s.heading_deg);
            const float tol = cfg.glitch_factor * std::fabs(predicted) + cfg.glitch_margin_deg;
            if (std::fabs(actual - predicted) > tol) {
                if (s.glitch_streak < 0xFF) ++s.glitch_streak;
                if (s.glitch_streak < cfg.glitch_max_streak) {
                    s.glitched = true;          // transitorio: descartar este ciclo
                } else {
                    s.glitch_streak = 0;        // persistente: era real, aceptar
                }
            } else {
                s.glitch_streak = 0;
            }
        }

        // Salud por calibración (si no quedó glitcheado fuera).
        s.health = (smp.calib_gyro >= scfg.min_calib_gyro)
                       ? ImuHealth::OK : ImuHealth::DEGRADED;

        // Adoptar el heading (incluso si glitched, para "ponerse al día" y no
        // rechazar para siempre una reubicación real; el ciclo glitched igual
        // se excluye de la fusión).
        s.heading_deg = h;
        s.seen = true;
    }

    // ---- Paso B: detección de drift en reposo ----
    // Robot quieto = todos los sensores presentes tienen |gyro| < umbral.
    bool at_rest = false;
    {
        int present_cnt = 0; int still_cnt = 0;
        for (int i = 0; i < IMU_FUSION_N; ++i) {
            if (in[i].present && sensor_cfg[i].enabled) {
                ++present_cnt;
                if (std::fabs(in[i].gyro_z_dps) < cfg.rest_gyro_dps) ++still_cnt;
            }
        }
        at_rest = (present_cnt > 0 && present_cnt == still_cnt);
    }

    // En reposo medimos cuánto se aparta cada sensor del consenso (el otro, si
    // está sano). Un desvío sostenido = ese sensor está driftando => reset.
    static_assert(IMU_FUSION_N == 2, "drift loop asume 2 sensores");
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        ImuSensorState& s = f.s[i];
        if (!at_rest || !sensor_cfg[i].enabled || !in[i].present
            || s.health == ImuHealth::DEAD) {
            // fuera de reposo: el acumulador de drift se enfría de a poco.
            if (s.drift_accum_ms > dt_ms) s.drift_accum_ms -= dt_ms; else s.drift_accum_ms = 0;
            continue;
        }
        // velocidad de cambio del heading en reposo = |drift| instantáneo
        const int other = 1 - i;
        // referencia: el heading del OTRO sensor si está sano; si no, su propio histórico
        // (acá medimos cuánto se aparta este sensor del consenso a lo largo del reposo)
        float drift_rate = 0.0f;
        if (f.s[other].health == ImuHealth::OK && in[other].present) {
            drift_rate = imu_diff(s.heading_deg, f.s[other].heading_deg);  // desvío vs sano
            // lo expresamos como °/s dividiendo por una ventana nominal de 1 s
            // (no integramos: usamos la magnitud del desvío sostenido)
        }
        // EMA del desvío
        s.drift_dps = 0.9f * s.drift_dps + 0.1f * std::fabs(drift_rate);
        if (s.drift_dps > cfg.drift_reset_dps && f.s[other].health == ImuHealth::OK) {
            if (s.drift_accum_ms < 0xFFFFFFFF - dt_ms) s.drift_accum_ms += dt_ms;
            if (s.drift_accum_ms >= cfg.drift_reset_ms) {
                s.request_reset  = true;
                s.reseed_heading = f.s[other].heading_deg;  // re-sembrar con el sano
            }
        } else {
            if (s.drift_accum_ms > dt_ms) s.drift_accum_ms -= dt_ms; else s.drift_accum_ms = 0;
        }
    }

    // ---- Paso C: fusión ----
    int   usable[IMU_FUSION_N]; int n_use = 0;
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (sensor_cfg[i].enabled && in[i].present && !f.s[i].glitched
            && f.s[i].health != ImuHealth::DEAD) {
            usable[n_use++] = i;
        }
    }

    // Desacuerdo diagnóstico: si ambos están presentes (aunque uno sea degraded).
    f.disagreement_deg = 0.0f;
    f.impact_detected  = false;
    if (in[0].present && in[1].present && sensor_cfg[0].enabled && sensor_cfg[1].enabled) {
        f.disagreement_deg = std::fabs(imu_diff(f.s[0].heading_deg, f.s[1].heading_deg));
    }

    if (n_use == 0) {
        f.fused_valid = false;   // se conserva fused_heading_deg anterior
        return;
    }
    if (n_use == 1) {
        f.fused_heading_deg = f.s[usable[0]].heading_deg;
        f.fused_valid = true;
        return;
    }

    // 2 utilizables.
    const int a = usable[0], b = usable[1];
    const float dis = std::fabs(imu_diff(f.s[a].heading_deg, f.s[b].heading_deg));
    if (dis > cfg.disagree_max_deg) {
        // Impacto/falla: NO promediar (daría un valor intermedio falso).
        // Elegir el de mayor calidad: OK > DEGRADED; empate -> mayor calib;
        // empate -> sensor 0 (LEFT) como referencia histórica.
        f.impact_detected = true;
        int pick = a;
        const bool a_ok = f.s[a].health == ImuHealth::OK;
        const bool b_ok = f.s[b].health == ImuHealth::OK;
        if (b_ok && !a_ok) pick = b;
        else if (a_ok == b_ok) {
            if (in[b].calib_gyro > in[a].calib_gyro) pick = b;
            else pick = (a == 0) ? a : ((b == 0) ? b : a);
        }
        f.fused_heading_deg = f.s[pick].heading_deg;
        f.fused_valid = true;
        return;
    }

    const float wa = sensor_weight(f.s[a], in[a], sensor_cfg[a], cfg);
    const float wb = sensor_weight(f.s[b], in[b], sensor_cfg[b], cfg);
    f.fused_heading_deg = imu_norm180(
        imu_circular_mean(f.s[a].heading_deg, f.s[b].heading_deg, wa, wb));
    f.fused_valid = true;
}

void imu_fusion_clear_reset(ImuFusion& f, int i) {
    if (i < 0 || i >= IMU_FUSION_N) return;
    ImuSensorState& s = f.s[i];
    s.request_reset  = false;
    s.drift_dps      = 0.0f;
    s.drift_accum_ms = 0;
    s.glitch_streak  = 0;
    s.glitched       = false;
    s.heading_deg    = s.reseed_heading;  // arranca alineado con el sano
    s.seen           = false;             // re-ceba el glitch-test
    s.miss_count     = 0;
    s.health         = ImuHealth::DEGRADED;  // sube a OK cuando recalibre
}

}  // namespace iitasoccer
