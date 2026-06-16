// test_hcsr04_async — TDD de la FSM ASÍNCRONA del HC-SR04 (reemplazo del pulseIn
// bloqueante). Cubre src/shared/hcsr04_async.h: ciclo nominal de medición, el
// FAIL-SAFE (timeouts sin flanco que devuelven SENTINELA y NUNCA el valor viejo),
// rango físico, wrap de micros() y descarte de ecos espurios.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_hcsr04_async
//
// El módulo es PURO (solo <stdint.h>) → no linkea Arduino. El "tiempo" entra como
// uint32 de micros() que acá lo controla el test (no hay reloj real).

#include <unity.h>
#include "hcsr04_async.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: un Async recién inicializado (equivale al zero-init de .bss en el Teensy).
static Hcsr04Async fresh_fsm(void) {
    Hcsr04Async s{};   // value-init → phase=IDLE, sin medida previa, valid=false
    return s;
}

// ----------------------------------------------------------------------------
// Sanity: zero-init == IDLE, sin medida, poll da NO_READING.
// ----------------------------------------------------------------------------
void test_zero_init_is_idle_no_reading(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);
    TEST_ASSERT_FALSE(s.valid);
    // sin ninguna medición → sentinela, no un cero engañoso.
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, hcsr04_poll(s, 1000u, cfg));
}

// ----------------------------------------------------------------------------
// Defaults del cfg (contrato: 60000 / 12000 / 20 / 2000).
// ----------------------------------------------------------------------------
void test_default_cfg_values(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    TEST_ASSERT_EQUAL_UINT32(60000u, cfg.trig_period_us);
    TEST_ASSERT_EQUAL_UINT32(12000u, cfg.echo_timeout_us);
    TEST_ASSERT_EQUAL_UINT16(20u,    cfg.min_mm);
    TEST_ASSERT_EQUAL_UINT16(2000u,  cfg.max_mm);
}

// ----------------------------------------------------------------------------
// CICLO NOMINAL: due → trig → rise@t0 → fall@t0+1750us → poll ~300 mm.
//   width = 1750 us → mm = 1750*343/2000 = 600250/2000 = 300 mm exacto.
// ----------------------------------------------------------------------------
void test_nominal_cycle_300mm(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    // En IDLE con now grande → toca disparar.
    const uint32_t t0 = 1000000u;   // 1 s en micros
    TEST_ASSERT_TRUE(hcsr04_due(s, t0, cfg));

    // El glue dispara el trigger y avisa.
    hcsr04_on_trig_sent(s, t0);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);

    // Llega la subida del eco en t0 (eco empieza ~enseguida).
    hcsr04_on_edge(s, /*rising=*/true, t0);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_FALL, s.phase);

    // Llega la bajada 1750 us después → medición completa, vuelve a IDLE.
    hcsr04_on_edge(s, /*rising=*/false, t0 + 1750u);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);
    TEST_ASSERT_TRUE(s.valid);

    // poll devuelve 300 mm exacto.
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 2000u, cfg));
}

// ----------------------------------------------------------------------------
// FAIL-SAFE: timeout SIN subida (WAIT_RISE) → NO_READING, phase IDLE, y NO la
// medida anterior. Primero dejamos una medida válida vieja para probar que el
// timeout NO la devuelve.
// ----------------------------------------------------------------------------
void test_timeout_no_rise_returns_no_reading_not_stale(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    // --- Ciclo 1: una medida buena de 300 mm (queda en last_dist_mm) ---
    const uint32_t t0 = 1000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true,  t0);
    hcsr04_on_edge(s, false, t0 + 1750u);
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 2000u, cfg));
    TEST_ASSERT_EQUAL_UINT16(300u, s.last_dist_mm);   // la vieja está guardada

    // --- Ciclo 2: disparamos pero NUNCA llega la subida ---
    const uint32_t t1 = t0 + 100000u;   // próximo disparo
    hcsr04_on_trig_sent(s, t1);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);

    // Justo en el límite (waited == timeout) → todavía NO vence: devuelve la última
    // buena (300), no NO_READING. Esto confirma que el corte es ESTRICTO (> timeout).
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t1 + cfg.echo_timeout_us, cfg));
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);   // sigue esperando

    // Un us más allá del timeout → SE RINDE: NO_READING + IDLE, NUNCA los 300 viejos.
    const uint16_t r = hcsr04_poll(s, t1 + cfg.echo_timeout_us + 1u, cfg);
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, r);
    TEST_ASSERT_NOT_EQUAL(300u, r);                           // jamás el dato viejo
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);        // FSM limpia
}

// ----------------------------------------------------------------------------
// FAIL-SAFE: timeout a MEDIA medición (rise llegó, fall NUNCA) → en WAIT_FALL,
// pasado el timeout desde rise_us → NO_READING + IDLE.
// ----------------------------------------------------------------------------
void test_timeout_rise_without_fall(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 2000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true, t0);            // subida OK
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_FALL, s.phase);

    // En el límite exacto desde la subida → todavía espera la bajada.
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING,
                             hcsr04_poll(s, t0 + cfg.echo_timeout_us, cfg));
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_FALL, s.phase);   // sin vencer aún

    // Pasado el timeout DESDE rise_us (=t0) → se rinde.
    const uint16_t r = hcsr04_poll(s, t0 + cfg.echo_timeout_us + 1u, cfg);
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, r);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);
}

// ----------------------------------------------------------------------------
// FUERA DE RANGO (> max): un eco larguísimo da una distancia > max_mm → NO_READING.
//   Para superar 2000 mm: width > 2000*2000/343 ≈ 11662 us. Usamos 12000 us, que
//   además es el techo del eco; mm = 12000*343/2000 = 2058 mm > 2000 → fuera.
// ----------------------------------------------------------------------------
void test_out_of_range_above_max(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 3000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true,  t0);
    hcsr04_on_edge(s, false, t0 + 12000u);   // ancho 12000 us → 2058 mm
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);

    // La medición se completó pero cae fuera de [20,2000] → sentinela.
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, hcsr04_poll(s, t0 + 13000u, cfg));
}

// ----------------------------------------------------------------------------
// FUERA DE RANGO (< min): un eco brevísimo da una distancia < min_mm → NO_READING.
//   width 100 us → mm = 100*343/2000 = 17 mm < 20 → fuera.
// ----------------------------------------------------------------------------
void test_out_of_range_below_min(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 4000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true,  t0);
    hcsr04_on_edge(s, false, t0 + 100u);     // 17 mm < 20
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, hcsr04_poll(s, t0 + 200u, cfg));
}

// ----------------------------------------------------------------------------
// WRAP de micros() cerca de 2^32: el disparo arranca antes del wrap y la bajada
// llega después de dar la vuelta. La resta unsigned debe dar el ancho real.
//   t0 = 2^32 - 500;  rise = t0;  fall = rise + 1750 (envuelve a 1250).
//   width = (uint32)(1250 - (2^32-500)) = 1750 → 300 mm.
// ----------------------------------------------------------------------------
void test_wrap_around_micros(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 0xFFFFFFFFu - 500u;     // 500 us antes del wrap
    // due con un now apenas mayor (mismo lado del wrap) → dispara.
    TEST_ASSERT_TRUE(hcsr04_due(s, t0, cfg));    // trig_start_us=0 inicial, resta enorme

    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true,  t0);                // rise en t0
    const uint32_t fall = t0 + 1750u;            // envuelve más allá de 2^32
    hcsr04_on_edge(s, false, fall);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);

    // El ancho cruzando el wrap se mide bien → 300 mm.
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, fall + 100u, cfg));
}

// ----------------------------------------------------------------------------
// WRAP en el TIMEOUT: WAIT_RISE arrancó justo antes del wrap; el poll llega después
// del wrap. La resta unsigned debe medir el intervalo real (no un timeout espurio
// ni perderlo). Disparo en 2^32-100; poll en 50 (envuelto): waited=150 < timeout →
// sigue esperando. poll en 2^32-100+timeout+1 (envuelto) → vence.
// ----------------------------------------------------------------------------
void test_wrap_around_timeout(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 0xFFFFFFFFu - 100u;      // 100 us antes del wrap
    hcsr04_on_trig_sent(s, t0);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);

    // poll en 50 us (ya envuelto): waited = 50 - (2^32-100) = 150 us < 12 ms → espera.
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, hcsr04_poll(s, 50u, cfg));
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);

    // poll bien pasado el timeout (envuelto) → vence aunque cruzó el wrap.
    const uint32_t after = t0 + cfg.echo_timeout_us + 1u;   // envuelve
    TEST_ASSERT_EQUAL_UINT16(HCSR04_NO_READING, hcsr04_poll(s, after, cfg));
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);      // se rindió por timeout real
}

// ----------------------------------------------------------------------------
// due() respeta trig_period_us: tras un disparo+ciclo completo, no vuelve a "tocar"
// hasta que pasó el período desde el último trig_start_us.
// ----------------------------------------------------------------------------
void test_due_respects_trig_period(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 5000000u;
    // Mientras MIDE (no IDLE), due() es false sí o sí.
    hcsr04_on_trig_sent(s, t0);
    TEST_ASSERT_FALSE(hcsr04_due(s, t0 + 1000000u, cfg));   // en WAIT_RISE → no dispara

    // Completar la medición → vuelve a IDLE con trig_start_us=t0.
    hcsr04_on_edge(s, true,  t0);
    hcsr04_on_edge(s, false, t0 + 1750u);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);

    // Antes del período (justo en el límite: corte es >=) → due en el borde es true,
    // un us antes es false.
    TEST_ASSERT_FALSE(hcsr04_due(s, t0 + cfg.trig_period_us - 1u, cfg));
    TEST_ASSERT_TRUE (hcsr04_due(s, t0 + cfg.trig_period_us,       cfg));
    TEST_ASSERT_TRUE (hcsr04_due(s, t0 + cfg.trig_period_us + 100u, cfg));
}

// ----------------------------------------------------------------------------
// ECO ESPURIO: una BAJADA sin SUBIDA previa (estamos en WAIT_RISE, llega un fall)
// → se IGNORA. La FSM no cambia de fase ni inventa distancia. Luego una medición
// normal sigue funcionando.
// ----------------------------------------------------------------------------
void test_spurious_fall_without_rise_ignored(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 6000000u;
    hcsr04_on_trig_sent(s, t0);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);

    // Bajada espuria mientras esperábamos una SUBIDA → ignorada, seguimos en WAIT_RISE.
    hcsr04_on_edge(s, /*rising=*/false, t0 + 500u);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_RISE, s.phase);
    TEST_ASSERT_FALSE(s.valid);                      // no inventó una medición

    // Ahora una medición real funciona normal.
    hcsr04_on_edge(s, true,  t0 + 1000u);
    hcsr04_on_edge(s, false, t0 + 1000u + 1750u);    // width 1750 → 300 mm
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 3000u, cfg));
}

// ----------------------------------------------------------------------------
// ECO ESPURIO 2: una SUBIDA cuando NO estamos en WAIT_RISE (estamos en WAIT_FALL,
// ya recibimos una subida) → la subida extra se ignora; gana la PRIMERA subida.
// ----------------------------------------------------------------------------
void test_spurious_extra_rise_ignored(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 7000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true, t0);            // primera subida → WAIT_FALL, rise_us=t0
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_FALL, s.phase);

    // Subida espuria en t0+200: NO debe reabrir el cronómetro (rise_us sigue en t0).
    hcsr04_on_edge(s, true, t0 + 200u);
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::WAIT_FALL, s.phase);

    // Bajada en t0+1750: el ancho se mide desde la PRIMERA subida (t0) → 300 mm, no
    // desde la espuria (que habría dado 1550 us → 265 mm).
    hcsr04_on_edge(s, false, t0 + 1750u);
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 2000u, cfg));
}

// ----------------------------------------------------------------------------
// due() NO dispara mientras se está MIDIENDO (anti-pisar medición en curso),
// confirmado de forma aislada (complementa test_due_respects_trig_period).
// ----------------------------------------------------------------------------
void test_due_false_while_measuring(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 8000000u;
    hcsr04_on_trig_sent(s, t0);
    // En WAIT_RISE, aunque pasó muchísimo (más que el período), due es false.
    TEST_ASSERT_FALSE(hcsr04_due(s, t0 + 10u * cfg.trig_period_us, cfg));

    hcsr04_on_edge(s, true, t0);
    // En WAIT_FALL, idem.
    TEST_ASSERT_FALSE(hcsr04_due(s, t0 + 10u * cfg.trig_period_us, cfg));
}

// ----------------------------------------------------------------------------
// CONTINUIDAD: entre disparos (en IDLE, sin medición nueva en curso) poll sigue
// reportando la última distancia válida (no NO_READING) hasta el próximo ciclo o
// timeout. Esto evita un "hueco" de un frame en el min_obstacle.
// ----------------------------------------------------------------------------
void test_holds_last_valid_between_cycles(void) {
    const Hcsr04AsyncCfg cfg = hcsr04_async_default_cfg();
    Hcsr04Async s = fresh_fsm();

    const uint32_t t0 = 9000000u;
    hcsr04_on_trig_sent(s, t0);
    hcsr04_on_edge(s, true,  t0);
    hcsr04_on_edge(s, false, t0 + 1750u);            // 300 mm, vuelve a IDLE

    // En IDLE, repetidos poll devuelven 300 (continuidad), sin volverse stale solos.
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 2000u, cfg));
    TEST_ASSERT_EQUAL_UINT16(300u, hcsr04_poll(s, t0 + 50000u, cfg));
    TEST_ASSERT_EQUAL_INT(Hcsr04Async::IDLE, s.phase);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_is_idle_no_reading);
    RUN_TEST(test_default_cfg_values);
    RUN_TEST(test_nominal_cycle_300mm);
    RUN_TEST(test_timeout_no_rise_returns_no_reading_not_stale);
    RUN_TEST(test_timeout_rise_without_fall);
    RUN_TEST(test_out_of_range_above_max);
    RUN_TEST(test_out_of_range_below_min);
    RUN_TEST(test_wrap_around_micros);
    RUN_TEST(test_wrap_around_timeout);
    RUN_TEST(test_due_respects_trig_period);
    RUN_TEST(test_spurious_fall_without_rise_ignored);
    RUN_TEST(test_spurious_extra_rise_ignored);
    RUN_TEST(test_due_false_while_measuring);
    RUN_TEST(test_holds_last_valid_between_cycles);
    return UNITY_END();
}
