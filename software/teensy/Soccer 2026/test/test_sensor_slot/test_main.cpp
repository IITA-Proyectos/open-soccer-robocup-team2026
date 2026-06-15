// test_sensor_slot — TDD de la PIZARRA productor/consumidor (rediseño sensorial TOP).
//
// Cubre el módulo PURO src/shared/sensor_slot.h: un SensorSlot<T> con doble buffer +
// seqlock (slot_publish / slot_read_latest lock-free) + frescura wrap-safe
// (slot_is_fresh / slot_age_ms). El glue Arduino (volatile, barreras de memoria,
// correr el publish dentro de la ISR / desde DMA) NO se testea acá — vive en la
// integración de banco. Acá se verifica la LÓGICA: alternancia de buffers, no-torn-read
// simulando el estado intermedio del productor a mano, y expiración de frescura.
//
// Compila host (sin Arduino). El runner agrega -I src/shared, así que el include es
// directo "sensor_slot.h". Binario único por test (lo arma run-host-tests.sh):
//
//   g++ -std=gnu++17 -I "src/shared" -I lib/Unity/src \
//       lib/Unity/src/unity.c test/test_sensor_slot/test_main.cpp \
//       -o /tmp/t_sensor_slot && /tmp/t_sensor_slot

#include <unity.h>
#include "sensor_slot.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Dato de prueba multi-campo: un avistaje de pelota. La gracia de tener VARIOS campos
// es que un torn read (mitad viejo / mitad nuevo) se detecta como una struct
// inconsistente (x e y de publicaciones distintas), no como un solo número roto.
struct Ball {
    int16_t x_mm;
    int16_t y_mm;
    uint8_t confidence;
};

static bool ball_eq(const Ball& a, const Ball& b) {
    return a.x_mm == b.x_mm && a.y_mm == b.y_mm && a.confidence == b.confidence;
}

// Un dato que es coherente solo si y == x*2 + conf. Así cualquier mezcla de campos de
// dos publicaciones distintas rompe la invariante y el test lo cacha.
static bool ball_coherent(const Ball& b) {
    return b.y_mm == (int16_t)(b.x_mm * 2 + b.confidence);
}
static Ball make_coherent(int16_t x, uint8_t conf) {
    return Ball{ x, (int16_t)(x * 2 + conf), conf };
}

// ============================================================================
// READ SIN ESCRITOR → último valor (o el inicial si nunca se publicó)
// ============================================================================

void test_read_without_writer_gives_initial_zero(void) {
    // Slot recién construido (zero-init): nunca se publicó → buffer[0] = T{} = ceros.
    SensorSlot<Ball> slot{};
    Ball r = slot_read_latest(slot);
    TEST_ASSERT_EQUAL_INT16(0, r.x_mm);
    TEST_ASSERT_EQUAL_INT16(0, r.y_mm);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
}

void test_read_after_one_publish_gives_that_value(void) {
    SensorSlot<Ball> slot{};
    Ball v = make_coherent(100, 5);   // {100, 205, 5}
    slot_publish(slot, v, /*now*/ 1000);
    Ball r = slot_read_latest(slot);
    TEST_ASSERT_TRUE(ball_eq(v, r));
}

void test_read_is_idempotent_without_new_publish(void) {
    // Leer muchas veces sin publicar nada nuevo devuelve siempre el mismo último valor.
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(42, 7), 1000);
    Ball r1 = slot_read_latest(slot);
    Ball r2 = slot_read_latest(slot);
    Ball r3 = slot_read_latest(slot);
    TEST_ASSERT_TRUE(ball_eq(r1, r2));
    TEST_ASSERT_TRUE(ball_eq(r2, r3));
}

void test_read_gives_latest_after_multiple_publishes(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 1000);
    slot_publish(slot, make_coherent(2, 2), 1010);
    slot_publish(slot, make_coherent(3, 3), 1020);
    Ball r = slot_read_latest(slot);
    TEST_ASSERT_TRUE(ball_eq(make_coherent(3, 3), r));
}

// ============================================================================
// NO-TORN-READ: las publicaciones ALTERNAN de buffer; el lector siempre saca un
// dato COHERENTE (de una sola publicación), nunca mitad-viejo-mitad-nuevo.
// ============================================================================

void test_buffers_alternate_between_publishes(void) {
    // Tras publicaciones sucesivas, el índice de buffer seleccionado por seq alterna
    // 1,0,1,0... → el productor nunca pisa el buffer que el lector mira en ESE momento.
    SensorSlot<Ball> slot{};
    uint32_t prev_idx = (slot.seq >> 1) & 1u;
    for (int i = 1; i <= 6; ++i) {
        slot_publish(slot, make_coherent((int16_t)i, (uint8_t)i), 1000u + i);
        uint32_t idx = (slot.seq >> 1) & 1u;
        TEST_ASSERT_NOT_EQUAL(prev_idx, idx);   // cambió de casillero
        prev_idx = idx;
        // Y el dato leído es siempre el recién publicado, entero.
        TEST_ASSERT_TRUE(ball_eq(make_coherent((int16_t)i, (uint8_t)i),
                                 slot_read_latest(slot)));
    }
}

void test_every_read_is_coherent_across_many_publishes(void) {
    // Estrés de la invariante y==2x+conf: tras CADA publicación, lo leído cumple la
    // invariante (jamás una mezcla de dos publicaciones distintas).
    SensorSlot<Ball> slot{};
    for (int i = 0; i < 50; ++i) {
        slot_publish(slot, make_coherent((int16_t)(i * 3), (uint8_t)(i % 200)),
                     1000u + i);
        TEST_ASSERT_TRUE(ball_coherent(slot_read_latest(slot)));
    }
}

void test_no_torn_read_when_writer_mid_publish(void) {
    // Simula la concurrencia SIN hilos: dejamos el slot en el estado INTERMEDIO de una
    // publicación (seq impar = "escribiendo") y verificamos que lo último COMPLETO que
    // se ve sigue siendo coherente — nunca el buffer a medio escribir.
    SensorSlot<Ball> slot{};
    Ball good = make_coherent(10, 4);       // {10, 24, 4}
    slot_publish(slot, good, 1000);         // publicación completa → seq par

    // El productor ARRANCA una nueva publicación pero NO la termina: marca seq impar y
    // empieza a escribir el OTRO buffer con basura parcial (mitad de un dato nuevo).
    const uint32_t start = slot.seq;
    slot.seq = start + 1;                   // → IMPAR: "escritura en curso"
    const uint32_t write_idx = ((start + 2u) >> 1) & 1u;
    slot.buffer[write_idx] = Ball{999, 0, 0};   // basura: NO cumple la invariante

    // El buffer COHERENTE (el de la última publicación completa) sigue intacto y es el
    // que `seq` (en su valor PAR previo) selecciona. El lector real reintentaría hasta
    // que seq vuelva a ser par; acá comprobamos que el buffer publicado no se tocó.
    const uint32_t committed_idx = (start >> 1) & 1u;
    TEST_ASSERT_TRUE(ball_eq(good, slot.buffer[committed_idx]));
    TEST_ASSERT_TRUE(ball_coherent(slot.buffer[committed_idx]));

    // Y el buffer que se está escribiendo (incoherente) es OTRO, no el publicado.
    TEST_ASSERT_NOT_EQUAL(committed_idx, write_idx);

    // El productor termina la publicación → seq par de nuevo → ahora read da el nuevo.
    slot.buffer[write_idx] = make_coherent(20, 6);
    slot.seq = start + 2;                   // → PAR: publicado
    slot.last_publish_ms = 1010;
    Ball r = slot_read_latest(slot);
    TEST_ASSERT_TRUE(ball_coherent(r));
    TEST_ASSERT_TRUE(ball_eq(make_coherent(20, 6), r));
}

void test_seq_is_even_after_each_publish(void) {
    // Invariante del seqlock: fuera de una publicación, seq SIEMPRE queda par
    // (ninguna escritura a medio hacer visible al lector).
    SensorSlot<Ball> slot{};
    TEST_ASSERT_EQUAL_UINT32(0u, slot.seq & 1u);     // inicial par
    for (int i = 0; i < 10; ++i) {
        slot_publish(slot, make_coherent((int16_t)i, 1), 1000u + i);
        TEST_ASSERT_EQUAL_UINT32(0u, slot.seq & 1u); // par tras publicar
    }
}

// ============================================================================
// FRESCURA: expira por timeout (wrap-safe)
// ============================================================================

void test_fresh_right_after_publish(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), /*now*/ 5000);
    // elapsed 0 < timeout → fresco.
    TEST_ASSERT_TRUE(slot_is_fresh(slot, /*now*/ 5000, /*timeout*/ 100));
}

void test_fresh_within_timeout(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    // elapsed 80 < 100 → fresco.
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 5080, 100));
}

void test_fresh_exactly_at_timeout_boundary(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    // elapsed == timeout (100) todavía cuenta como FRESCO (corte es > timeout).
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 5100, 100));
}

void test_stale_one_ms_past_timeout(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    // elapsed 101 > 100 → vencido.
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 5101, 100));
}

void test_stale_far_past_timeout(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    // El sensor se calló hace 5 s → vencido (CENTRAL debe ignorar el dato).
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 10000, 100));
}

void test_never_published_is_not_fresh(void) {
    // Slot recién construido (zero-init): nunca se publicó → NO fresco (no hay base).
    SensorSlot<Ball> slot{};
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 1000, 100));
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 0, 0));
}

void test_zero_timeout_expires_after_one_ms(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    // timeout 0: elapsed 0 sigue fresco (corte > timeout), elapsed 1 ya venció.
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 5000, 0));
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 5001, 0));
}

void test_freshness_renews_on_new_publish(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 5000);
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 5200, 100));   // venció (elapsed 200)
    // Una publicación nueva renueva el reloj de frescura.
    slot_publish(slot, make_coherent(2, 2), 5200);
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 5250, 100));    // elapsed 50 desde la nueva
}

void test_freshness_wrap_around_does_not_falsely_stale(void) {
    // millis() wrapeó (~49,7 días): last_publish cerca del tope, now wrapeó chico.
    // elapsed real = 100 ms; la resta unsigned lo da bien → sigue fresco.
    SensorSlot<Ball> slot{};
    const uint32_t last = 0xFFFFFFFFu - 49;   // 50 ms antes del wrap
    const uint32_t now  = 50;                  // 50 ms después del wrap
    slot_publish(slot, make_coherent(1, 1), last);
    TEST_ASSERT_EQUAL_UINT32(100u, now - last);
    TEST_ASSERT_TRUE(slot_is_fresh(slot, now, /*timeout*/ 250));
}

void test_freshness_wrap_around_still_detects_real_stale(void) {
    // Mismo wrap pero el elapsed real (300 ms) SÍ supera el timeout (250) → vencido.
    SensorSlot<Ball> slot{};
    const uint32_t last = 0xFFFFFFFFu - 49;
    const uint32_t now  = 250;
    slot_publish(slot, make_coherent(1, 1), last);
    TEST_ASSERT_EQUAL_UINT32(300u, now - last);
    TEST_ASSERT_FALSE(slot_is_fresh(slot, now, 250));
}

// ============================================================================
// EDAD (telemetría)
// ============================================================================

void test_age_zero_before_any_publish(void) {
    SensorSlot<Ball> slot{};
    TEST_ASSERT_EQUAL_UINT32(0u, slot_age_ms(slot, 1234));
}

void test_age_after_publish(void) {
    SensorSlot<Ball> slot{};
    slot_publish(slot, make_coherent(1, 1), 1000);
    TEST_ASSERT_EQUAL_UINT32(250u, slot_age_ms(slot, 1250));
}

void test_age_wrap_safe(void) {
    SensorSlot<Ball> slot{};
    const uint32_t last = 0xFFFFFFFFu - 49;
    slot_publish(slot, make_coherent(1, 1), last);
    TEST_ASSERT_EQUAL_UINT32(100u, slot_age_ms(slot, 50));
}

// ============================================================================
// SLOT GENÉRICO: funciona con un escalar simple, no solo con structs.
// ============================================================================

void test_works_with_scalar_type(void) {
    SensorSlot<uint16_t> dist{};
    TEST_ASSERT_EQUAL_UINT16(0, slot_read_latest(dist));   // inicial
    slot_publish(dist, (uint16_t)1234, 1000);
    TEST_ASSERT_EQUAL_UINT16(1234, slot_read_latest(dist));
    slot_publish(dist, (uint16_t)5678, 1010);
    TEST_ASSERT_EQUAL_UINT16(5678, slot_read_latest(dist));
    TEST_ASSERT_TRUE(slot_is_fresh(dist, 1010, 50));
    TEST_ASSERT_FALSE(slot_is_fresh(dist, 1200, 50));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    // Read sin escritor → último valor.
    RUN_TEST(test_read_without_writer_gives_initial_zero);
    RUN_TEST(test_read_after_one_publish_gives_that_value);
    RUN_TEST(test_read_is_idempotent_without_new_publish);
    RUN_TEST(test_read_gives_latest_after_multiple_publishes);
    // No-torn-read / alternancia de buffers.
    RUN_TEST(test_buffers_alternate_between_publishes);
    RUN_TEST(test_every_read_is_coherent_across_many_publishes);
    RUN_TEST(test_no_torn_read_when_writer_mid_publish);
    RUN_TEST(test_seq_is_even_after_each_publish);
    // Frescura por timeout (wrap-safe).
    RUN_TEST(test_fresh_right_after_publish);
    RUN_TEST(test_fresh_within_timeout);
    RUN_TEST(test_fresh_exactly_at_timeout_boundary);
    RUN_TEST(test_stale_one_ms_past_timeout);
    RUN_TEST(test_stale_far_past_timeout);
    RUN_TEST(test_never_published_is_not_fresh);
    RUN_TEST(test_zero_timeout_expires_after_one_ms);
    RUN_TEST(test_freshness_renews_on_new_publish);
    RUN_TEST(test_freshness_wrap_around_does_not_falsely_stale);
    RUN_TEST(test_freshness_wrap_around_still_detects_real_stale);
    // Edad (telemetría).
    RUN_TEST(test_age_zero_before_any_publish);
    RUN_TEST(test_age_after_publish);
    RUN_TEST(test_age_wrap_safe);
    // Genérico con escalar.
    RUN_TEST(test_works_with_scalar_type);
    return UNITY_END();
}
