// test_down_blackboard — TDD de la PIZARRA de la placa DOWN (wrapper sobre sensor_slot.h).
//
// Cubre src/down/down_blackboard.h: el struct DownBlackboard (3 SensorSlot<T> con los
// tipos REALES del DOWN — LineStatusV2 / Pose2D / Velocity2D de src/shared/types.h) y su
// instancia global g_down_bb, AMBOS gateados detrás de -DDOWN_BLACKBOARD.
//
// down_blackboard.h NO reimplementa el seqlock: REUSA los helpers de sensor_slot.h
// (slot_publish / slot_read_latest / slot_read_latest_capped / slot_is_fresh /
// slot_age_ms). Por eso acá NO re-testeamos la lógica interna del seqlock (eso lo hace
// test_sensor_slot, 30 tests): testeamos que el WRAPPER esté bien cableado — que cada
// slot guarde/devuelva su tipo real coherente, que publish→read dé el último valor, que
// la frescura funcione por slot, y que un read concurrente-simulado (seq impar a mano)
// nunca devuelva un torn-read.
//
// El gate: el runner (run-host-tests.sh) NO pasa -DDOWN_BLACKBOARD, así que lo
// DEFINIMOS acá ANTES del include para materializar el struct y el global. Eso también
// documenta el contrato: con el gate OFF, down_blackboard.h no define NADA (byte-idéntico
// a hoy); con el gate ON, define DownBlackboard + g_down_bb. (Un test extra verifica que
// la PIZARRA sólo existe bajo el gate.)
//
// Include por ruta relativa: el header vive en src/down/, y el runner sólo agrega
// -I src/shared (no -I src/down). La ruta relativa resuelve sin tocar los flags; los
// includes internos del header (sensor_slot.h, types.h) sí resuelven vía -I src/shared.
//
// PURO: compila host (sin Arduino). El tiempo es un uint32 que pasamos a mano.

#define DOWN_BLACKBOARD   // materializa DownBlackboard + g_down_bb (el runner no lo pasa)

#include <unity.h>
#include "../../src/down/down_blackboard.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// --- Helpers de igualdad por tipo (comparan los campos load-bearing de cada struct) ---

static bool line_eq(const LineStatusV2& a, const LineStatusV2& b) {
    return a.schema_version       == b.schema_version &&
           a.data_valid           == b.data_valid &&
           a.line_angle_centideg  == b.line_angle_centideg &&
           a.escape_angle_centideg== b.escape_angle_centideg &&
           a.penetration_mm       == b.penetration_mm &&
           a.cross_track_mm       == b.cross_track_mm &&
           a.line_present         == b.line_present &&
           a.sensors_on_line      == b.sensors_on_line &&
           a.event_flags          == b.event_flags &&
           a.quality              == b.quality &&
           a.sample_age_ms        == b.sample_age_ms &&
           a.reserved             == b.reserved;
}

static bool pose_eq(const Pose2D& a, const Pose2D& b) {
    return a.x_mm == b.x_mm && a.y_mm == b.y_mm &&
           a.heading_centideg == b.heading_centideg && a.confidence == b.confidence;
}

static bool vel_eq(const Velocity2D& a, const Velocity2D& b) {
    return a.vx_mm_s == b.vx_mm_s && a.vy_mm_s == b.vy_mm_s &&
           a.omega_centideg_s == b.omega_centideg_s && a.slip_estimate == b.slip_estimate;
}

// Una LineStatusV2 "real" plausible (línea presente al frente, válida).
static LineStatusV2 make_line(int16_t angle, uint8_t sensors, uint8_t age) {
    LineStatusV2 l{};
    l.schema_version        = LSV2_SCHEMA;
    l.data_valid            = 1;
    l.line_angle_centideg   = angle;
    l.escape_angle_centideg = (int16_t)(angle + 18000);  // opuesto (no canónico, sólo dato)
    l.penetration_mm        = 12;
    l.cross_track_mm        = 3;
    l.line_present          = 1;
    l.sensors_on_line       = sensors;
    l.event_flags           = EV_IMMINENT_EXIT;
    l.quality               = 90;
    l.sample_age_ms         = age;
    l.reserved              = 0;
    return l;
}

// ============================================================================
// EL GATE: con -DDOWN_BLACKBOARD el struct y el global EXISTEN y son zero-init.
// ============================================================================

void test_gate_on_blackboard_exists_and_zero_init(void) {
    // Con el gate ON, DownBlackboard y g_down_bb están definidos. Recién construido
    // (zero-init), los 3 slots están vacíos: nunca publicados, seq par, no frescos.
    DownBlackboard bb{};
    TEST_ASSERT_FALSE(bb.line.published);
    TEST_ASSERT_FALSE(bb.pose.published);
    TEST_ASSERT_FALSE(bb.vel.published);
    TEST_ASSERT_EQUAL_UINT32(0u, bb.line.seq & 1u);   // par (sin escritura a medio hacer)
    TEST_ASSERT_EQUAL_UINT32(0u, bb.pose.seq & 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, bb.vel.seq & 1u);
    // Antes del primer publish: read da el valor inicial (ceros) y no es fresco → el
    // difusor trata cada slot como no-fresco y manda sentinela (default-to-safe).
    TEST_ASSERT_FALSE(slot_is_fresh(bb.line, 1000, 100));
    TEST_ASSERT_FALSE(slot_is_fresh(bb.pose, 1000, 100));
    TEST_ASSERT_FALSE(slot_is_fresh(bb.vel,  1000, 100));
}

void test_global_instance_is_usable(void) {
    // El global g_down_bb existe y es publicable/legible (lo que el loop vivo hará).
    // Lo dejamos limpio al final para no contaminar otros tests (orden de RUN_TEST).
    slot_publish(g_down_bb.line, make_line(500, 4, 2), /*now*/ 7000);
    LineStatusV2 r = slot_read_latest(g_down_bb.line);
    TEST_ASSERT_TRUE(line_eq(make_line(500, 4, 2), r));
    g_down_bb = DownBlackboard{};   // reset (zero-init) — higiene entre tests
}

// ============================================================================
// LINE slot: publish → read-latest devuelve el último valor coherente.
// ============================================================================

void test_line_publish_then_read_latest(void) {
    DownBlackboard bb{};
    LineStatusV2 v = make_line(-1234, 6, 5);
    slot_publish(bb.line, v, 1000);
    LineStatusV2 r = slot_read_latest(bb.line);
    TEST_ASSERT_TRUE(line_eq(v, r));
    TEST_ASSERT_TRUE(bb.line.published);
}

void test_line_read_gives_latest_after_multiple_publishes(void) {
    DownBlackboard bb{};
    slot_publish(bb.line, make_line(100, 1, 1), 1000);
    slot_publish(bb.line, make_line(200, 2, 2), 1005);
    slot_publish(bb.line, make_line(300, 3, 3), 1010);
    LineStatusV2 r = slot_read_latest(bb.line);
    TEST_ASSERT_TRUE(line_eq(make_line(300, 3, 3), r));   // el ÚLTIMO, entero
}

void test_line_sentinel_na_roundtrips(void) {
    // Una LineStatusV2 con los campos en sentinela N/A (sin línea) sobrevive intacta:
    // los sentinelas LSV2_NA_* son datos como cualquier otro para el slot.
    DownBlackboard bb{};
    LineStatusV2 na{};
    na.schema_version        = LSV2_SCHEMA;
    na.data_valid            = 0;            // compuerta maestra cerrada
    na.line_angle_centideg   = LSV2_NA_I16;
    na.escape_angle_centideg = LSV2_NA_I16;
    na.penetration_mm        = LSV2_NA_U16;
    na.cross_track_mm        = LSV2_NA_I16;
    slot_publish(bb.line, na, 2000);
    LineStatusV2 r = slot_read_latest(bb.line);
    TEST_ASSERT_TRUE(line_eq(na, r));
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, r.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT16(LSV2_NA_U16, r.penetration_mm);
}

// ============================================================================
// POSE slot: publish → read-latest devuelve la última pose coherente.
// ============================================================================

void test_pose_publish_then_read_latest(void) {
    DownBlackboard bb{};
    Pose2D p{ -500, 1200, 9000, 80 };
    slot_publish(bb.pose, p, 3000);
    Pose2D r = slot_read_latest(bb.pose);
    TEST_ASSERT_TRUE(pose_eq(p, r));
}

void test_pose_read_gives_latest_after_multiple_publishes(void) {
    DownBlackboard bb{};
    slot_publish(bb.pose, Pose2D{ 10, 10, 100, 50 }, 1000);
    slot_publish(bb.pose, Pose2D{ 20, 20, 200, 60 }, 1010);
    slot_publish(bb.pose, Pose2D{ 30, 30, 300, 70 }, 1020);
    Pose2D r = slot_read_latest(bb.pose);
    TEST_ASSERT_TRUE(pose_eq(Pose2D{ 30, 30, 300, 70 }, r));
}

// ============================================================================
// VEL slot: publish → read-latest devuelve la última velocidad coherente.
// ============================================================================

void test_vel_publish_then_read_latest(void) {
    DownBlackboard bb{};
    Velocity2D v{ 350, -120, 4500, 7 };
    slot_publish(bb.vel, v, 4000);
    Velocity2D r = slot_read_latest(bb.vel);
    TEST_ASSERT_TRUE(vel_eq(v, r));
}

// ============================================================================
// SLOTS INDEPENDIENTES: publicar en uno NO toca a los otros.
// ============================================================================

void test_slots_are_independent(void) {
    DownBlackboard bb{};
    slot_publish(bb.line, make_line(500, 5, 5), 1000);
    // pose y vel siguen vírgenes (no publicados, no frescos).
    TEST_ASSERT_TRUE(bb.line.published);
    TEST_ASSERT_FALSE(bb.pose.published);
    TEST_ASSERT_FALSE(bb.vel.published);
    // Publicar pose no afecta lo leído de line.
    slot_publish(bb.pose, Pose2D{ 1, 2, 3, 4 }, 1010);
    TEST_ASSERT_TRUE(line_eq(make_line(500, 5, 5), slot_read_latest(bb.line)));
    TEST_ASSERT_TRUE(pose_eq(Pose2D{ 1, 2, 3, 4 }, slot_read_latest(bb.pose)));
    TEST_ASSERT_FALSE(bb.vel.published);   // vel intacto
}

// ============================================================================
// FRESCURA POR SLOT: cada slot envejece según SU última publicación (wrap-safe).
// ============================================================================

void test_per_slot_freshness(void) {
    DownBlackboard bb{};
    slot_publish(bb.line, make_line(0, 1, 0), /*now*/ 5000);
    slot_publish(bb.pose, Pose2D{ 0, 0, 0, 0 }, /*now*/ 5100);   // 100 ms más tarde
    // En now=5150 (timeout 100): line ya venció (elapsed 150 > 100), pose sigue fresca
    // (elapsed 50 ≤ 100). Demuestra que la frescura es POR slot, no global.
    TEST_ASSERT_FALSE(slot_is_fresh(bb.line, 5150, 100));
    TEST_ASSERT_TRUE (slot_is_fresh(bb.pose, 5150, 100));
    // vel nunca se publicó → no fresco siempre.
    TEST_ASSERT_FALSE(slot_is_fresh(bb.vel, 5150, 100));
    // Edad (telemetría) coherente con lo anterior.
    TEST_ASSERT_EQUAL_UINT32(150u, slot_age_ms(bb.line, 5150));
    TEST_ASSERT_EQUAL_UINT32(50u,  slot_age_ms(bb.pose, 5150));
}

// ============================================================================
// READ ACOTADO (fail-safe del difusor): capped da true+valor en slot coherente.
// ============================================================================

void test_capped_read_coherent_returns_value(void) {
    DownBlackboard bb{};
    LineStatusV2 v = make_line(700, 8, 9);
    slot_publish(bb.line, v, 1000);
    LineStatusV2 out{};
    bool ok = slot_read_latest_capped(bb.line, out, /*max_retries*/ 4);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(line_eq(v, out));
}

// ============================================================================
// NO-TORN-READ (concurrencia simulada): el productor deja seq IMPAR a mano (como si
// la ISR se hubiera colgado a medio publicar). El read ACOTADO se rinde (false) y NO
// toca `out` → el difusor conserva su sentinela, nunca un Frankenstein. El read PAR
// previo (la última publicación completa) sigue intacto y coherente.
// ============================================================================

void test_no_torn_read_when_writer_mid_publish(void) {
    DownBlackboard bb{};
    LineStatusV2 good = make_line(1500, 3, 1);
    slot_publish(bb.line, good, 1000);          // publicación completa → seq par

    // El productor (ISR) ARRANCA otra publicación y se cuelga: deja seq IMPAR y empieza
    // a escribir el OTRO buffer con basura parcial — un frame a medio armar.
    const uint32_t start = bb.line.seq;
    bb.line.seq = start + 1;                    // → IMPAR: "escritura en curso"
    const uint32_t write_idx = ((start + 2u) >> 1) & 1u;
    LineStatusV2 garbage{};
    garbage.schema_version = 0xEE;              // basura: ni siquiera el schema correcto
    garbage.data_valid     = 1;
    bb.line.buffer[write_idx] = garbage;

    // El buffer COHERENTE (última publicación completa) NO se tocó y es el que el seq
    // PAR previo selecciona.
    const uint32_t committed_idx = (start >> 1) & 1u;
    TEST_ASSERT_NOT_EQUAL(committed_idx, write_idx);
    TEST_ASSERT_TRUE(line_eq(good, bb.line.buffer[committed_idx]));

    // El difusor lee ACOTADO con seq impar: se rinde (false) y conserva su sentinela en
    // `out`. JAMÁS propaga el frame a medio armar.
    LineStatusV2 sentinel{};
    sentinel.schema_version = LSV2_SCHEMA;
    sentinel.data_valid     = 0;               // sentinela "sin dato confiable"
    sentinel.line_angle_centideg = LSV2_NA_I16;
    LineStatusV2 out = sentinel;
    bool ok = slot_read_latest_capped(bb.line, out, /*max_retries*/ 4);
    TEST_ASSERT_FALSE(ok);                      // se rindió, NO colgó
    TEST_ASSERT_TRUE(line_eq(sentinel, out));   // out INTACTO (default-to-safe)

    // El productor termina la publicación → seq par de nuevo → read da el dato nuevo.
    LineStatusV2 fresh = make_line(2000, 6, 2);
    bb.line.buffer[write_idx] = fresh;
    bb.line.seq = start + 2;                    // → PAR: publicado
    bb.line.last_publish_ms = 1010;
    LineStatusV2 r = slot_read_latest(bb.line);
    TEST_ASSERT_TRUE(line_eq(fresh, r));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    // El gate / la instancia global.
    RUN_TEST(test_gate_on_blackboard_exists_and_zero_init);
    RUN_TEST(test_global_instance_is_usable);
    // Slot line.
    RUN_TEST(test_line_publish_then_read_latest);
    RUN_TEST(test_line_read_gives_latest_after_multiple_publishes);
    RUN_TEST(test_line_sentinel_na_roundtrips);
    // Slot pose.
    RUN_TEST(test_pose_publish_then_read_latest);
    RUN_TEST(test_pose_read_gives_latest_after_multiple_publishes);
    // Slot vel.
    RUN_TEST(test_vel_publish_then_read_latest);
    // Independencia + frescura por slot.
    RUN_TEST(test_slots_are_independent);
    RUN_TEST(test_per_slot_freshness);
    // Read acotado + no-torn-read (fail-safe del difusor).
    RUN_TEST(test_capped_read_coherent_returns_value);
    RUN_TEST(test_no_torn_read_when_writer_mid_publish);
    return UNITY_END();
}
