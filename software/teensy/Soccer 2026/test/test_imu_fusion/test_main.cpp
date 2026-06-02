// test_imu_fusion — tests host-native del módulo puro de fusión dual-BNO.
#include <unity.h>
#include "imu_fusion.h"
#include <cmath>

using namespace iitasoccer;

void setUp() {}
void tearDown() {}

// ---- helpers de armado ----
static ImuSample mk(bool present, float hdg, float gyro, uint8_t calib) {
    ImuSample s; s.present = present; s.heading_deg = hdg;
    s.gyro_z_dps = gyro; s.calib_gyro = calib; return s;
}

// ============================ Helpers puros ============================

void test_norm180_wraps() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f,  imu_norm180(370.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -170.0f, imu_norm180(190.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   imu_norm180(360.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, imu_norm180(-90.0f));
}

void test_diff_circular() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, imu_diff(179.0f, 177.0f));
    // 179 - (-179) = 358 -> debe dar -2 (camino corto), no 358
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -2.0f, imu_diff(179.0f, -179.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, imu_diff(-180.0f, 180.0f));
}

void test_circular_mean_basic() {
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 15.0f, imu_circular_mean(10.0f, 20.0f, 1.0f, 1.0f));
}

void test_circular_mean_wraparound() {
    // promedio de 179 y -179 debe ser ±180, NO 0 (el aritmético daría 0).
    float m = imu_circular_mean(179.0f, -179.0f, 1.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 180.0f, std::fabs(m));
}

void test_circular_mean_weighted() {
    // peso 3:1 hacia 0 -> resultado cerca de 0, no de 40.
    float m = imu_circular_mean(0.0f, 40.0f, 3.0f, 1.0f);
    TEST_ASSERT_TRUE(m > 5.0f && m < 15.0f);
}

// ============================ Estado / fusión ============================

void test_init_all_dead() {
    ImuFusion f; imu_fusion_init(f);
    TEST_ASSERT_FALSE(f.fused_valid);
    TEST_ASSERT_EQUAL(static_cast<int>(ImuHealth::DEAD), static_cast<int>(f.s[0].health));
    TEST_ASSERT_EQUAL(static_cast<int>(ImuHealth::DEAD), static_cast<int>(f.s[1].health));
}

void test_single_sensor_drives_fusion() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    ImuSample in[2] = { mk(true, 42.0f, 0.0f, 3), mk(false, 0.0f, 0.0f, 0) };
    imu_fusion_update(f, cfg, sc, in, 0.01f);
    TEST_ASSERT_TRUE(f.fused_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 42.0f, f.fused_heading_deg);
    TEST_ASSERT_EQUAL(static_cast<int>(ImuHealth::OK), static_cast<int>(f.s[0].health));
}

void test_two_agree_average() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    ImuSample in[2] = { mk(true, 10.0f, 0.0f, 3), mk(true, 20.0f, 0.0f, 3) };
    imu_fusion_update(f, cfg, sc, in, 0.01f);
    TEST_ASSERT_TRUE(f.fused_valid);
    TEST_ASSERT_FALSE(f.impact_detected);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.0f, f.fused_heading_deg);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, f.disagreement_deg);
}

void test_failover_when_one_dies() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    // sensor1 ausente varios ciclos -> DEAD; fusión = sensor0.
    for (uint16_t i = 0; i < cfg.dead_after_misses + 1; ++i) {
        ImuSample in[2] = { mk(true, 33.0f, 0.0f, 3), mk(false, 0.0f, 0.0f, 0) };
        imu_fusion_update(f, cfg, sc, in, 0.01f);
    }
    TEST_ASSERT_EQUAL(static_cast<int>(ImuHealth::DEAD), static_cast<int>(f.s[1].health));
    TEST_ASSERT_TRUE(f.fused_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 33.0f, f.fused_heading_deg);
}

void test_disagreement_triggers_impact_no_average() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    // sensor0 OK (calib 3) a 0°, sensor1 DEGRADED (calib 0) a 100° -> desacuerdo 100 > 30.
    ImuSample in[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, 100.0f, 0.0f, 0) };
    imu_fusion_update(f, cfg, sc, in, 0.01f);
    TEST_ASSERT_TRUE(f.impact_detected);
    // NO promedia (no daría ~50): usa el OK (sensor0 = 0°).
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, f.fused_heading_deg);
}

void test_glitch_rejected_one_cycle() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    // cebar: ambos a 0, gyro 0.
    ImuSample warm[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, 0.0f, 0.0f, 3) };
    imu_fusion_update(f, cfg, sc, warm, 0.01f);
    // sensor1 pega un salto a 80° SIN que el gyro lo justifique (gyro 0) -> glitch.
    ImuSample jump[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, 80.0f, 0.0f, 3) };
    imu_fusion_update(f, cfg, sc, jump, 0.01f);
    TEST_ASSERT_TRUE(f.s[1].glitched);
    // fusión ignora el glitch -> queda con sensor0 = 0, no ~40.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, f.fused_heading_deg);
}

void test_fast_rotation_not_glitch() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    ImuSample warm[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, 0.0f, 0.0f, 3) };
    imu_fusion_update(f, cfg, sc, warm, 0.1f);
    // giro real: heading salta 30° con gyro 300°/s * 0.1s = 30° predicho -> NO glitch.
    ImuSample rot[2] = { mk(true, 30.0f, 300.0f, 3), mk(true, 30.0f, 300.0f, 3) };
    imu_fusion_update(f, cfg, sc, rot, 0.1f);
    TEST_ASSERT_FALSE(f.s[0].glitched);
    TEST_ASSERT_FALSE(f.s[1].glitched);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.0f, f.fused_heading_deg);
}

void test_mount_offset_applied() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    sc[0].mount_offset_deg = 90.0f;   // sensor0 montado +90
    sc[1].enabled = false;
    ImuSample in[2] = { mk(true, 0.0f, 0.0f, 3), mk(false, 0.0f, 0.0f, 0) };
    imu_fusion_update(f, cfg, sc, in, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, f.fused_heading_deg);
}

void test_drift_requests_reset_and_reseed() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    // robot en REPOSO (gyro ~0). sensor0 estable a 0°; sensor1 DRIFTA: su heading
    // se va solo ~1°/s (drift real, no un offset fijo). Tras varios segundos
    // sostenidos, sensor1 se marca para reset, sembrado con el estable (~0).
    float h1 = 0.0f;
    for (int i = 0; i < 90; ++i) {        // dt 100ms x 90 = 9 s
        h1 += 0.1f;                       // +1 grado/s
        ImuSample in[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, h1, 0.0f, 3) };
        imu_fusion_update(f, cfg, sc, in, 0.1f);
    }
    TEST_ASSERT_TRUE(f.s[1].request_reset);
    TEST_ASSERT_FALSE(f.s[0].request_reset);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, f.s[1].reseed_heading);  // del estable
}

void test_constant_offset_is_not_drift() {
    // Un offset CONSTANTE (mismo valor cada ciclo) NO es drift: ninguno se mueve
    // en el tiempo. NO debe marcarse reset (eso es trabajo de mount_offset).
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    for (int i = 0; i < 90; ++i) {
        ImuSample in[2] = { mk(true, 0.0f, 0.0f, 3), mk(true, 5.0f, 0.0f, 3) };
        imu_fusion_update(f, cfg, sc, in, 0.1f);
    }
    TEST_ASSERT_FALSE(f.s[0].request_reset);
    TEST_ASSERT_FALSE(f.s[1].request_reset);
}

void test_clear_reset_realigns() {
    ImuFusion f; imu_fusion_init(f);
    f.s[1].request_reset = true;
    f.s[1].reseed_heading = 12.5f;
    f.s[1].drift_accum_ms = 9999;
    imu_fusion_clear_reset(f, 1);
    TEST_ASSERT_FALSE(f.s[1].request_reset);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, f.s[1].heading_deg);
    TEST_ASSERT_EQUAL(0, (int)f.s[1].drift_accum_ms);
}

void test_disabled_sensor_ignored() {
    ImuFusion f; imu_fusion_init(f);
    ImuFusionCfg cfg = imu_fusion_default_cfg();
    ImuSensorCfg sc[2] = { imu_fusion_default_sensor_cfg(), imu_fusion_default_sensor_cfg() };
    sc[1].enabled = false;
    ImuSample in[2] = { mk(true, 25.0f, 0.0f, 3), mk(true, 999.0f, 50.0f, 3) };
    imu_fusion_update(f, cfg, sc, in, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 25.0f, f.fused_heading_deg);  // ignora sensor1
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_norm180_wraps);
    RUN_TEST(test_diff_circular);
    RUN_TEST(test_circular_mean_basic);
    RUN_TEST(test_circular_mean_wraparound);
    RUN_TEST(test_circular_mean_weighted);
    RUN_TEST(test_init_all_dead);
    RUN_TEST(test_single_sensor_drives_fusion);
    RUN_TEST(test_two_agree_average);
    RUN_TEST(test_failover_when_one_dies);
    RUN_TEST(test_disagreement_triggers_impact_no_average);
    RUN_TEST(test_glitch_rejected_one_cycle);
    RUN_TEST(test_fast_rotation_not_glitch);
    RUN_TEST(test_mount_offset_applied);
    RUN_TEST(test_drift_requests_reset_and_reseed);
    RUN_TEST(test_constant_offset_is_not_drift);
    RUN_TEST(test_clear_reset_realigns);
    RUN_TEST(test_disabled_sensor_ignored);
    return UNITY_END();
}
