// test_imu_freeze — detector de BNO congelado (audit 2026-06-04, único HIGH).
// Corre host con: bash scripts/run-host-tests.sh test_imu_freeze
//
// Cubre la lógica PURA que detecta un heading clavado al centidegrado EXACTO
// (patrón del getVector()=(0,0,0) por fallo I²C / yaw congelado banco 2026-06-02):
//   • un valor que cambia normalmente NUNCA dispara,
//   • un valor bit-idéntico por >= N lecturas Y >= T ms dispara,
//   • si el valor vuelve a cambiar => se limpia (descongelado),
//   • respeta AMBOS umbrales (no dispara solo por N si falta T, ni al revés),
//   • el contador de racha satura (no wrappea) en quietos larguísimos,
//   • imu_freeze_cfg_from_rate deriva N desde la tasa sin volverse más sensible.

#include <unity.h>
#include "imu_freeze.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: empuja `count` lecturas del MISMO valor, avanzando el reloj `step_ms`
// por lectura. Devuelve el último veredicto.
static bool feed_constant(ImuFreezeState& s, int16_t value, int count,
                          uint32_t step_ms, uint32_t& now_ms, const ImuFreezeCfg& cfg) {
    bool frozen = false;
    for (int i = 0; i < count; ++i) {
        frozen = imu_freeze_update(s, value, now_ms, cfg);
        now_ms += step_ms;
    }
    return frozen;
}

// Helper de la variante CON GUARDA DE GYRO: mismo heading con un gyro CONSTANTE.
static bool feed_constant_g(ImuFreezeState& s, int16_t value, float gyro_dps, int count,
                            uint32_t step_ms, uint32_t& now_ms, const ImuFreezeCfg& cfg) {
    bool frozen = false;
    for (int i = 0; i < count; ++i) {
        frozen = imu_freeze_update_g(s, value, gyro_dps, now_ms, cfg);
        now_ms += step_ms;
    }
    return frozen;
}

// Estado inicial: .bss zero-init == no visto, no congelado.
void test_zero_init_not_frozen(void) {
    ImuFreezeState s{};
    TEST_ASSERT_FALSE(s.seen);
    TEST_ASSERT_FALSE(s.frozen);
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    // Primera lectura solo siembra: nunca congela en el tick inicial.
    TEST_ASSERT_FALSE(imu_freeze_update(s, 1234, 0, cfg));
    TEST_ASSERT_TRUE(s.seen);
}

// Un heading que cambia normalmente (sensor vivo) NUNCA dispara.
void test_changing_value_never_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // Simula jitter normal de un BNO vivo: ±1..2 LSB de centideg cada lectura,
    // durante mucho más que N lecturas y T ms.
    int16_t base = 500;
    for (int i = 0; i < 500; ++i) {
        int16_t v = base + (int16_t)((i % 3) - 1);  // alterna -1,0,+1
        TEST_ASSERT_FALSE(imu_freeze_update(s, v, now, cfg));
        now += 50;
    }
    TEST_ASSERT_FALSE(s.frozen);
}

// Valor bit-idéntico por >= N lecturas y >= T ms => dispara.
void test_identical_value_for_n_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // Suficientes lecturas y tiempo (N=40 @ 50 ms = 2 s > T=1500 ms).
    bool frozen = feed_constant(s, 0, (int)cfg.min_samples + 5, 50, now, cfg);
    TEST_ASSERT_TRUE(frozen);
    TEST_ASSERT_TRUE(s.frozen);
}

// El caso de fallo real: getVector devuelve (0,0,0) clavado => heading 0 fijo.
void test_zero_heading_stuck_is_detected(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 100000;  // timestamp arbitrario (no arranca en 0)
    bool frozen = feed_constant(s, 0, 100, 50, now, cfg);
    TEST_ASSERT_TRUE(frozen);
}

// Vuelve a cambiar => se limpia (sensor revivió / se movió).
void test_change_after_freeze_clears(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant(s, 1500, 60, 50, now, cfg));
    TEST_ASSERT_TRUE(s.frozen);
    // Una sola lectura DISTINTA limpia el latch inmediatamente.
    TEST_ASSERT_FALSE(imu_freeze_update(s, 1501, now, cfg));
    TEST_ASSERT_FALSE(s.frozen);
    TEST_ASSERT_EQUAL_UINT16(1, s.streak);
}

// Respeta el umbral de TIEMPO: N lecturas idénticas pero en MENOS de T ms NO
// dispara (lecturas muy rápidas que aún no acumularon el tiempo mínimo).
void test_time_threshold_respected(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // N+10 lecturas pero a 1 ms cada una => held_ms ~ (N+9) ms << T=1500 ms.
    bool frozen = feed_constant(s, 7, (int)cfg.min_samples + 10, 1, now, cfg);
    TEST_ASSERT_FALSE(frozen);   // alcanzó N pero NO T
    TEST_ASSERT_FALSE(s.frozen);
    // Ahora sigue clavado hasta superar T: debe terminar disparando.
    // Acumulamos tiempo manteniendo el valor.
    for (int i = 0; i < 2000 && !s.frozen; ++i) {
        imu_freeze_update(s, 7, now, cfg);
        now += 1;
    }
    TEST_ASSERT_TRUE(s.frozen);
}

// Respeta el umbral de CUENTA: T ms cumplido pero menos de N lecturas NO dispara
// (pocas lecturas muy espaciadas). Defensa contra tasas de lectura raras.
void test_sample_threshold_respected(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // Pocas lecturas (N/2) pero muy espaciadas: T se supera de sobra, N no.
    int few = (int)cfg.min_samples / 2;
    if (few < 1) few = 1;
    bool frozen = feed_constant(s, 42, few, /*step*/ 500, now, cfg);
    TEST_ASSERT_FALSE(frozen);   // tiempo de sobra pero faltan lecturas
    TEST_ASSERT_FALSE(s.frozen);
}

// reset() limpia el estado y evita un descongelamiento espurio tras un re-cero.
void test_reset_clears_state(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    feed_constant(s, 300, 60, 50, now, cfg);
    TEST_ASSERT_TRUE(s.frozen);
    imu_freeze_reset(s);
    TEST_ASSERT_FALSE(s.seen);
    TEST_ASSERT_FALSE(s.frozen);
    TEST_ASSERT_EQUAL_UINT16(0, s.streak);
    // Tras reset, la primera lectura vuelve a sembrar sin congelar.
    TEST_ASSERT_FALSE(imu_freeze_update(s, 300, now, cfg));
}

// El contador de racha satura (no wrappea) en un quieto muy prolongado.
void test_streak_saturates_no_wrap(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // Muchísimas lecturas idénticas: streak debe saturar en 0xFFFF, no volver a 0.
    for (long i = 0; i < 80000; ++i) {
        imu_freeze_update(s, -1000, now, cfg);
        now += 1;
    }
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, s.streak);
    TEST_ASSERT_TRUE(s.frozen);
}

// Un solo cambio en medio de un largo congelamiento reinicia la cuenta de tiempo:
// el reloj de T arranca desde el cambio, no desde el origen.
void test_change_resets_time_window(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // Casi congela...
    feed_constant(s, 10, (int)cfg.min_samples - 1, 50, now, cfg);
    TEST_ASSERT_FALSE(s.frozen);
    // Cambia 1 LSB: reinicia. first_ms y streak vuelven a 1.
    imu_freeze_update(s, 11, now, cfg);
    TEST_ASSERT_EQUAL_UINT16(1, s.streak);
    TEST_ASSERT_EQUAL_UINT32(now, s.first_ms);
}

// imu_freeze_cfg_from_rate: deriva N desde la tasa pero NO se vuelve más sensible
// que el default (respeta el piso conservador) y nunca baja de 2 lecturas.
void test_cfg_from_rate_floor(void) {
    // Tasa rápida + T corto => ceil(T/dt) chico, pero NO debe bajar del piso.
    ImuFreezeCfg c = imu_freeze_cfg_from_rate(/*dt*/ 50, /*min_ms*/ 100);
    TEST_ASSERT_EQUAL_UINT32(100, c.min_ms);
    TEST_ASSERT_TRUE(c.min_samples >= 2);
    TEST_ASSERT_TRUE(c.min_samples >= IMU_FREEZE_MIN_SAMPLES);  // no más sensible que el default

    // dt=0 => usa defaults (no divide por cero).
    ImuFreezeCfg d = imu_freeze_cfg_from_rate(0, 9999);
    TEST_ASSERT_EQUAL_UINT16(IMU_FREEZE_MIN_SAMPLES, d.min_samples);

    // T grande @ 50 ms => N = ceil(T/50) por encima del piso.
    ImuFreezeCfg e = imu_freeze_cfg_from_rate(50, 5000);  // 100 lecturas
    TEST_ASSERT_EQUAL_UINT16(100, e.min_samples);
    TEST_ASSERT_EQUAL_UINT32(5000, e.min_ms);

    // BLINDAJE (M1): cfg_from_rate DEBE heredar gyro_motion_cdps del default. Si un
    // refactor lo dejara en 0, moving_now sería SIEMPRE true (|gyro|>=0) y se
    // reintroduciría el falso-DEAD del robot quieto (el bug del 2026-06-08). Este
    // assert lo atrapa en host antes de que llegue a la placa.
    TEST_ASSERT_EQUAL_UINT16(IMU_FREEZE_GYRO_MOTION_CDPS, c.gyro_motion_cdps);
    TEST_ASSERT_EQUAL_UINT16(IMU_FREEZE_GYRO_MOTION_CDPS, d.gyro_motion_cdps);
    TEST_ASSERT_EQUAL_UINT16(IMU_FREEZE_GYRO_MOTION_CDPS, e.gyro_motion_cdps);
}

// M1 (cont.): un robot QUIETO con la cfg DERIVADA por cfg_from_rate NO debe congelar
// (verifica end-to-end que la cfg derivada conserva la guarda de gyro, no solo el campo).
void test_g_cfg_from_rate_still_robot_not_frozen(void) {
    ImuFreezeCfg cfg = imu_freeze_cfg_from_rate(50, 1500);
    ImuFreezeState s{};
    uint32_t now = 0;
    bool frozen = false;
    for (int i = 0; i < (int)cfg.min_samples + 50; ++i) {
        float g = 0.3f * (float)((i % 3) - 1);   // ruido sub-umbral
        frozen = imu_freeze_update_g(s, 555, g, now, cfg);
        now += 50;
    }
    TEST_ASSERT_FALSE(frozen);
}

// Negativos de centideg (heading < 0) se comparan exacto igual que positivos.
void test_negative_centideg_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant(s, -17999, 60, 50, now, cfg));
}

// WRAP de millis(): el módulo afirma `held_ms = now_ms - first_ms` "wrap-safe"
// (resta unsigned). Lo probamos cruzando el reloj de cerca de 0xFFFFFFFF a 0:
//   • el valor se siembra cerca del overflow (first_ms ~ 0xFFFFFFF0),
//   • seguimos alimentando el MISMO heading mientras now_ms envuelve a 0 y sigue,
//   • held_ms NO debe "explotar" (~0xFFFFFFFF) por el cruce: la resta unsigned da
//     el delta REAL pequeño, así que el detector NO congela ANTES de tiempo,
//   • y una vez acumulado >= T ms reales (post-wrap) SÍ congela, demostrando que
//     el conteo de tiempo siguió siendo correcto a través del overflow.
void test_millis_wrap_held_ms_safe(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();   // N=40, T=1500 ms

    // Arrancamos 16 ms antes del overflow de uint32.
    uint32_t now = 0xFFFFFFFFu - 15u;   // 0xFFFFFFF0

    // Primera lectura: siembra first_ms = 0xFFFFFFF0.
    TEST_ASSERT_FALSE(imu_freeze_update(s, 100, now, cfg));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFF0u, s.first_ms);
    now += 4;   // 0xFFFFFFF4

    // Alimentamos N+ lecturas idénticas a 4 ms de paso, cruzando el wrap. En el
    // tick donde now envuelve a un valor PEQUEÑO (p.ej. 4), un cálculo no-wrap-safe
    // daría held_ms gigante (~4 mil millones) y dispararía el latch ANTES de los
    // 1500 ms reales. Verificamos que NO congela mientras held real < T.
    for (int i = 0; i < (int)cfg.min_samples + 5; ++i) {
        bool frozen = imu_freeze_update(s, 100, now, cfg);
        // held real = lecturas*4 ms; para las primeras decenas << 1500 ms.
        const uint32_t held = now - s.first_ms;   // misma cuenta unsigned del módulo
        TEST_ASSERT_TRUE(held < cfg.min_ms);      // delta real chico, NO explotó
        TEST_ASSERT_FALSE(frozen);                // todavía no debe congelar
        now += 4;
    }
    TEST_ASSERT_FALSE(s.frozen);

    // Ahora dejamos correr el tiempo REAL más allá de T (siempre mismo valor,
    // cruzando ya holgadamente el origen): debe terminar congelando. Esto prueba
    // que held_ms siguió siendo correcto y monótono a través del overflow.
    for (int i = 0; i < 2000 && !s.frozen; ++i) {
        imu_freeze_update(s, 100, now, cfg);
        now += 1;
    }
    TEST_ASSERT_TRUE(s.frozen);
    // held_ms efectivo en el disparo debe ser razonable (~T), no ~0xFFFFFFFF.
    const uint32_t held_at_freeze = now - s.first_ms;
    TEST_ASSERT_TRUE(held_at_freeze < 4000u);   // del orden de T, no un valor gigante
}

// ===========================================================================
// VARIANTE CON GUARDA DE GYRO (imu_freeze_update_g) — arregla el falso-DEAD
// del robot quieto (flag TOP_ENABLE_BNO_FREEZE_DETECT, banco 2026-06-08).
// ===========================================================================

// Quantización del gyro a centideg/s: redondeo y saturación.
void test_g_quantize_gyro(void) {
    TEST_ASSERT_EQUAL_INT16(0,    imu_freeze_quantize_gyro_cdps(0.0f));
    TEST_ASSERT_EQUAL_INT16(500,  imu_freeze_quantize_gyro_cdps(5.0f));
    TEST_ASSERT_EQUAL_INT16(-300, imu_freeze_quantize_gyro_cdps(-3.0f));
    TEST_ASSERT_EQUAL_INT16(32767,  imu_freeze_quantize_gyro_cdps(1e6f));   // satura +
    TEST_ASSERT_EQUAL_INT16(-32768, imu_freeze_quantize_gyro_cdps(-1e6f));  // satura -
}

// Caso PELIGROSO: el robot GIRA (gyro 10 dps >> umbral 5) pero el heading queda
// clavado al centidegrado => el heading está CONGELADO => CONGELA.
void test_g_motion_plus_stuck_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    bool frozen = feed_constant_g(s, 1234, 10.0f, (int)cfg.min_samples + 5, 50, now, cfg);
    TEST_ASSERT_TRUE(frozen);
    TEST_ASSERT_TRUE(s.frozen);
}

// EL ARREGLO DEL FALSO-DEAD (clave): robot QUIETO => heading clavado + gyro al PISO
// DE RUIDO (jitterea muy por debajo del umbral) => NO congela. El detector base
// mataba acá el único heading sano (banco 2026-06-08); la guarda lo evita.
void test_g_still_robot_not_frozen(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    bool frozen = false;
    for (int i = 0; i < (int)cfg.min_samples + 50; ++i) {
        float g = 0.3f * (float)((i % 3) - 1);   // -0.3, 0, +0.3 dps (ruido << 5 dps)
        frozen = imu_freeze_update_g(s, 777, g, now, cfg);
        now += 50;
    }
    TEST_ASSERT_FALSE(frozen);
    TEST_ASSERT_FALSE(s.frozen);
}

// gyro EXACTAMENTE 0 clavado + heading clavado => NO congela (no distinguible de un
// robot perfectamente quieto sin movimiento; elección conservadora documentada).
void test_g_zero_gyro_stuck_not_frozen(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    bool frozen = feed_constant_g(s, 0, 0.0f, (int)cfg.min_samples + 50, 50, now, cfg);
    TEST_ASSERT_FALSE(frozen);
}

// gyro girando pero heading CAMBIANDO normalmente => nunca congela (sensor vivo).
void test_g_moving_heading_never_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    for (int i = 0; i < 300; ++i) {
        int16_t v = (int16_t)(100 + (i % 5));  // heading cambia
        TEST_ASSERT_FALSE(imu_freeze_update_g(s, v, 10.0f, now, cfg));
        now += 50;
    }
    TEST_ASSERT_FALSE(s.frozen);
}

// _g es un refinamiento ESTRICTO del base: en el MISMO caso donde el base congela
// (heading clavado, sin movimiento), _g con gyro quieto NO congela.
void test_g_strict_refinement_vs_base(void) {
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    ImuFreezeState sb{};
    uint32_t nb = 0;
    TEST_ASSERT_TRUE(feed_constant(sb, 50, 60, 50, nb, cfg));      // BASE: congela
    ImuFreezeState sg{};
    uint32_t ng = 0;
    TEST_ASSERT_FALSE(feed_constant_g(sg, 50, 0.2f, 60, 50, ng, cfg));  // _g quieto: NO
}

// El movimiento del gyro DENTRO de la ventana (con el heading YA clavado) se RECUERDA:
// rotación mientras el rumbo no se mueve, luego el gyro se aquieta => CONGELA igual.
void test_g_motion_latched_in_window(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    imu_freeze_update_g(s, 900, 0.1f, now, cfg); now += 50;   // siembra heading (instante vivo)
    imu_freeze_update_g(s, 900, 12.0f, now, cfg); now += 50;  // heading YA clavado + rotación => motion
    bool frozen = false;
    for (int i = 0; i < (int)cfg.min_samples + 5; ++i) {
        frozen = imu_freeze_update_g(s, 900, 0.1f, now, cfg);  // se aquieta pero heading sigue clavado
        now += 50;
    }
    TEST_ASSERT_TRUE(frozen);
}

// Un cambio de heading LIMPIA la marca de movimiento: tras moverse y resetear, si el
// robot queda quieto (gyro al piso) con heading clavado, NO vuelve a congelar.
void test_g_heading_change_clears_motion(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    feed_constant_g(s, 300, 10.0f, 10, 50, now, cfg);   // girando, heading clavado (poco)
    imu_freeze_update_g(s, 301, 10.0f, now, cfg); now += 50;  // heading cambia => reset ventana
    // Ahora quieto (gyro al piso) con el nuevo heading clavado: NO debe congelar.
    bool frozen = false;
    for (int i = 0; i < (int)cfg.min_samples + 50; ++i) {
        frozen = imu_freeze_update_g(s, 301, 0.2f, now, cfg);
        now += 50;
    }
    TEST_ASSERT_FALSE(frozen);
}

// Respeta el umbral de TIEMPO también en _g: gyro girando + heading clavado N veces
// pero en < T ms NO congela todavía.
void test_g_time_threshold_respected(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    bool frozen = feed_constant_g(s, 7, 10.0f, (int)cfg.min_samples + 10, 1, now, cfg);
    TEST_ASSERT_FALSE(frozen);   // alcanzó N y hubo movimiento, pero NO T
}

// M2: borde EXACTO del umbral. 5.0 dps => 500 cdeg/s == umbral default; el corte es
// >= => CONGELA. 4.99 dps => 499 < 500 => NO congela. Fija el comparador y el redondeo.
void test_g_gyro_exactly_at_threshold_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();   // gyro_motion_cdps = 500
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant_g(s, 1234, 5.0f, (int)cfg.min_samples + 5, 50, now, cfg));
}
void test_g_gyro_just_below_threshold_not_frozen(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    // 4.99 dps => 499 cdeg/s < 500 => sub-umbral => NO congela aunque el heading esté clavado.
    TEST_ASSERT_FALSE(feed_constant_g(s, 1234, 4.99f, (int)cfg.min_samples + 50, 50, now, cfg));
}

// M3: rotación NEGATIVA (el robot gira al otro lado) + heading clavado => CONGELA.
// Ejercita la rama abs de negativos del cómputo de magnitud.
void test_g_negative_rotation_stuck_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant_g(s, -500, -10.0f, (int)cfg.min_samples + 5, 50, now, cfg));
}
// Límite patológico: -400 dps cuantiza a -40000 → satura a -32768; |·| no debe
// hacer overflow (por eso el cómputo usa int32/uint32). Sigue contando como movimiento.
void test_g_negative_saturation_no_overflow(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant_g(s, 7, -400.0f, (int)cfg.min_samples + 5, 50, now, cfg));
}

// MINOR: el latch PERSISTE tras congelar aunque el gyro se aquiete (mientras el
// heading siga clavado). No se "descongela" por falta de movimiento posterior.
void test_g_latch_persists_after_freeze(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant_g(s, 80, 10.0f, (int)cfg.min_samples + 5, 50, now, cfg));
    // Ahora gyro al piso, mismo heading clavado: sigue frozen.
    for (int i = 0; i < 20; ++i) { imu_freeze_update_g(s, 80, 0.1f, now, cfg); now += 50; }
    TEST_ASSERT_TRUE(s.frozen);
}

// MINOR: tras congelar, UN cambio de heading limpia el latch (equivalente _g del
// test_change_after_freeze_clears del detector base).
void test_g_change_after_freeze_clears(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed_constant_g(s, 80, 10.0f, (int)cfg.min_samples + 5, 50, now, cfg));
    TEST_ASSERT_FALSE(imu_freeze_update_g(s, 81, 10.0f, now, cfg));  // heading cambió
    TEST_ASSERT_FALSE(s.frozen);
    TEST_ASSERT_FALSE(s.gyro_motion_seen);   // ventana nueva: marca de movimiento reseteada
}

// MINOR: jitter de gyro alto pero SIEMPRE sub-umbral (4.0/4.5/4.9 dps) + heading
// clavado => NO congela (ruido grande no es rotación; defiende el umbral).
void test_g_subthreshold_jitter_never_freezes(void) {
    ImuFreezeState s{};
    ImuFreezeCfg cfg = imu_freeze_default_cfg();
    uint32_t now = 0;
    const float jit[3] = {4.0f, 4.5f, 4.9f};
    bool frozen = false;
    for (int i = 0; i < (int)cfg.min_samples + 50; ++i) {
        frozen = imu_freeze_update_g(s, 333, jit[i % 3], now, cfg);
        now += 50;
    }
    TEST_ASSERT_FALSE(frozen);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_not_frozen);
    RUN_TEST(test_changing_value_never_freezes);
    RUN_TEST(test_identical_value_for_n_freezes);
    RUN_TEST(test_zero_heading_stuck_is_detected);
    RUN_TEST(test_change_after_freeze_clears);
    RUN_TEST(test_time_threshold_respected);
    RUN_TEST(test_sample_threshold_respected);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_streak_saturates_no_wrap);
    RUN_TEST(test_change_resets_time_window);
    RUN_TEST(test_cfg_from_rate_floor);
    RUN_TEST(test_negative_centideg_freezes);
    RUN_TEST(test_millis_wrap_held_ms_safe);
    // Variante con guarda de gyro (imu_freeze_update_g)
    RUN_TEST(test_g_quantize_gyro);
    RUN_TEST(test_g_motion_plus_stuck_freezes);
    RUN_TEST(test_g_still_robot_not_frozen);
    RUN_TEST(test_g_zero_gyro_stuck_not_frozen);
    RUN_TEST(test_g_moving_heading_never_freezes);
    RUN_TEST(test_g_strict_refinement_vs_base);
    RUN_TEST(test_g_motion_latched_in_window);
    RUN_TEST(test_g_heading_change_clears_motion);
    RUN_TEST(test_g_time_threshold_respected);
    RUN_TEST(test_g_cfg_from_rate_still_robot_not_frozen);
    RUN_TEST(test_g_gyro_exactly_at_threshold_freezes);
    RUN_TEST(test_g_gyro_just_below_threshold_not_frozen);
    RUN_TEST(test_g_negative_rotation_stuck_freezes);
    RUN_TEST(test_g_negative_saturation_no_overflow);
    RUN_TEST(test_g_latch_persists_after_freeze);
    RUN_TEST(test_g_change_after_freeze_clears);
    RUN_TEST(test_g_subthreshold_jitter_never_freezes);
    return UNITY_END();
}
