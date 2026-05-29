// test_sensor_health — tests unitarios para sensor_health.{h,cpp}.
// Corre con: pio test -e test_native -f test_sensor_health

#include <unity.h>
#include <cstring>
#include "sensor_health.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// INIT
// ============================================================================

void test_sh_init_all_healthy(void) {
    SensorHealth s;
    sh_init(s);
    for (int i = 0; i < SH_MAX_SENSORS; ++i) {
        TEST_ASSERT_TRUE(sh_is_healthy(s, i));
    }
    TEST_ASSERT_FALSE(sh_any_unhealthy(s, 32));
    TEST_ASSERT_EQUAL_UINT8(32, sh_healthy_count(s, 32));
    TEST_ASSERT_EQUAL_UINT32(0u, sh_get_unhealthy_bitmap(s, 32));
}

void test_sh_oob_index_returns_healthy(void) {
    // Indices fuera de rango devuelven true (asume healthy) para no
    // bloquear sensores válidos por error de indexado.
    SensorHealth s;
    sh_init(s);
    TEST_ASSERT_TRUE(sh_is_healthy(s, -1));
    TEST_ASSERT_TRUE(sh_is_healthy(s, 32));
    TEST_ASSERT_TRUE(sh_is_healthy(s, 999));
}

// ============================================================================
// NORMAL OPERATION (sin marcar unhealthy)
// ============================================================================

void test_sh_normal_operation_stays_healthy(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    // Lectura típica de cancha: valores variantes, sin transiciones.
    for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(200 + (i * 7) % 200);
    // Simular 2 segundos de ticks. Para que NO sea stuck el raw debe
    // cambiar al menos una vez por sensor: rotamos los valores cada tick.
    for (uint32_t t = 0; t <= 2000; ++t) {
        for (int i = 0; i < 32; ++i) {
            raw[i] = (uint16_t)(200 + ((i + t) * 7) % 200);
        }
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_FALSE(sh_any_unhealthy(s, 32));
}

void test_sh_few_transitions_per_window_stays_healthy(void) {
    // 5 transiciones/segundo en un sensor = bajo el límite de 20.
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    for (int i = 0; i < 32; ++i) raw[i] = 300;
    // Alternar el sensor 5 únicamente, 5 veces en 1 segundo.
    for (uint32_t t = 0; t <= 1100; ++t) {
        // Cambio de raw cada tick para que stuck no salte (igualito al setup
        // normal): raw[5] alterna entre 300 y 305.
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        // Transiciones espaciadas cada ~200ms para llegar a 5 en la ventana.
        white[5] = ((t / 200) % 2) != 0;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_TRUE(sh_is_healthy(s, 5));
}

// ============================================================================
// RUIDOSO (muchas transiciones)
// ============================================================================

void test_sh_noisy_sensor_marked_unhealthy(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    // Sensor 10 oscila CADA tick → 1000 transiciones/seg si llamamos a 1kHz.
    // Mucho más del límite de 20.
    for (uint32_t t = 0; t <= 1100; ++t) {
        // raw varía para no disparar stuck (ese es otro test).
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        white[10] = (t % 2) != 0;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_FALSE(sh_is_healthy(s, 10));
    TEST_ASSERT_TRUE(sh_any_unhealthy(s, 32));
    // Bitmap: bit 10 = 0x400.
    TEST_ASSERT_EQUAL_UINT32(0x400u, sh_get_unhealthy_bitmap(s, 32));
    TEST_ASSERT_EQUAL_UINT8(31, sh_healthy_count(s, 32));
}

void test_sh_multiple_noisy_sensors(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    for (uint32_t t = 0; t <= 1100; ++t) {
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        // 3 sensores ruidosos: 0, 15, 31.
        white[0]  = (t % 2) != 0;
        white[15] = (t % 2) != 0;
        white[31] = (t % 2) != 0;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_FALSE(sh_is_healthy(s, 0));
    TEST_ASSERT_FALSE(sh_is_healthy(s, 15));
    TEST_ASSERT_FALSE(sh_is_healthy(s, 31));
    TEST_ASSERT_TRUE(sh_is_healthy(s, 5));  // sensor sano
    const uint32_t expected = (1u << 0) | (1u << 15) | (1u << 31);
    TEST_ASSERT_EQUAL_UINT32(expected, sh_get_unhealthy_bitmap(s, 32));
    TEST_ASSERT_EQUAL_UINT8(29, sh_healthy_count(s, 32));
}

// ============================================================================
// STUCK (mismo raw por mucho tiempo)
// ============================================================================

void test_sh_stuck_sensor_marked_unhealthy(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    // Sensor 7 stuck en 256 desde el inicio; los demás varían.
    for (uint32_t t = 0; t <= 6000; ++t) {  // 6s > SH_MAX_STUCK_MS (5s)
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        raw[7] = 256;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_FALSE(sh_is_healthy(s, 7));
    // El resto debería seguir healthy.
    for (int i = 0; i < 32; ++i) {
        if (i != 7) TEST_ASSERT_TRUE(sh_is_healthy(s, i));
    }
}

void test_sh_stuck_below_limit_stays_healthy(void) {
    // Stuck por menos de SH_MAX_STUCK_MS → NO marca unhealthy.
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    // Stuck por 3 segundos (3 ventanas) — bajo el límite de 5s.
    for (uint32_t t = 0; t <= 3000; ++t) {
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        raw[12] = 500;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_TRUE(sh_is_healthy(s, 12));
}

// ============================================================================
// AUTO-RECOVERY
// ============================================================================

void test_sh_noisy_then_clean_recovers(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[32];
    bool white[32] = {false};
    // Fase A: 1.1s de ruido → unhealthy.
    for (uint32_t t = 0; t <= 1100; ++t) {
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        white[3] = (t % 2) != 0;
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_FALSE(sh_is_healthy(s, 3));

    // Fase B: 2.2s de operación normal → recovery.
    // NOTA: requiere 2+ ventanas limpias porque la PRIMERA ventana cerrada
    // después de fase A todavía tiene transiciones residuales acumuladas
    // desde el último cierre intermedio dentro de fase A. La segunda
    // ventana sí está 100% limpia y dispara el recovery.
    for (uint32_t t = 1101; t <= 3200; ++t) {
        for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(300 + ((i + t) % 10));
        white[3] = false;  // sin transiciones
        sh_update(s, raw, white, 32, t);
    }
    TEST_ASSERT_TRUE(sh_is_healthy(s, 3));
}

// ============================================================================
// EDGE CASES
// ============================================================================

void test_sh_null_inputs_safe(void) {
    SensorHealth s;
    sh_init(s);
    // Llamar con punteros nulos no debe crashear.
    sh_update(s, nullptr, nullptr, 32, 100);
    // Estado debe quedar como estaba (sin ventana inicializada todavía).
    TEST_ASSERT_FALSE(s.window_initialized);
}

void test_sh_n_sensors_clamped_to_max(void) {
    SensorHealth s;
    sh_init(s);
    uint16_t raw[64] = {0};
    bool white[64] = {false};
    // Llamar con n_sensors=100 (más que SH_MAX_SENSORS) debe clamparse.
    sh_update(s, raw, white, 100, 0);
    sh_update(s, raw, white, 100, 1500);
    // No debe crashear. Comportamiento es como si fueran 32.
    TEST_ASSERT_EQUAL_UINT8(32, sh_healthy_count(s, 32));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sh_init_all_healthy);
    RUN_TEST(test_sh_oob_index_returns_healthy);
    RUN_TEST(test_sh_normal_operation_stays_healthy);
    RUN_TEST(test_sh_few_transitions_per_window_stays_healthy);
    RUN_TEST(test_sh_noisy_sensor_marked_unhealthy);
    RUN_TEST(test_sh_multiple_noisy_sensors);
    RUN_TEST(test_sh_stuck_sensor_marked_unhealthy);
    RUN_TEST(test_sh_stuck_below_limit_stays_healthy);
    RUN_TEST(test_sh_noisy_then_clean_recovers);
    RUN_TEST(test_sh_null_inputs_safe);
    RUN_TEST(test_sh_n_sensors_clamped_to_max);
    return UNITY_END();
}
