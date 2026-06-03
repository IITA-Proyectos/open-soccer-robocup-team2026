// test_cameras_parser — tests host-native del parser de cámara (src/top/cameras.cpp).
//
// Corre con:  bash scripts/run-host-tests.sh test_cameras_parser
//             (o pio test -e test_native -f test_cameras_parser)
//
// Por qué este archivo
// --------------------
// El parser CameraParser (máquina de estados de 9 bytes, headers 201/202/203) es
// el código MÁS crítico de la cadena de visión: ahí vive la heurística de
// "pelota fantasma". Tenía CERO cobertura host (cameras_fusion y ball_velocity sí
// la tienen). Estos tests son red de seguridad + caracterización del comportamiento
// ACTUAL (no cambian el parser; si algo sorprende, se documenta como hallazgo).
//
// Truco de compilación (unity-build)
// ----------------------------------
// run-host-tests.sh solo linkea src/shared, pero cameras.cpp vive en src/top.
// Lo incluimos inline acá: g++ lo compila como parte de esta unidad de traducción.
// El `#include "cameras.h"` interno de cameras.cpp resuelve por ruta relativa a
// src/top/cameras.h. No hay doble definición porque src/top NO se linkea aparte.
#include <unity.h>
#include "../../src/top/cameras.cpp"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Alimenta n bytes y devuelve cuántos packets completos (feed()==true) salieron.
static int feed_all(CameraParser& p, const uint8_t* b, int n) {
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (p.feed(b[i])) ++count;
    }
    return count;
}

// --- 1) Decode normal: pelota visible, arcos no -----------------------------
// [201,80,130, 202,0,0, 203,0,0]  → ball x=80 y=30 visible; arcos sentinel.
void test_normal_decode_ball_visible(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 80, 130, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(80, pk.ball_x);
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);          // 130 - 100
    TEST_ASSERT_TRUE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded());
}

// --- 2) Sentinel correcto: nada visible -------------------------------------
// [201,0,0, 202,0,0, 203,0,0]  → todo no visible (Y decodificado = -100).
void test_sentinel_all_not_visible(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 0, 0, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
}

// --- 3) CARACTERIZACIÓN del BUG FANTASMA (no es un fix) ----------------------
// Si el script viejo manda Y_coded=100 (en vez del sentinel 0), el parser
// decodifica Y = 100-100 = 0 e is_visible(0,0)=true → FANTASMA en (0,0).
// Esto DOCUMENTA que el parser depende de que el script mande el sentinel
// correcto (Y_coded=0). Los cam-*-n6.py ya mandan el sentinel correcto; este
// test deja constancia de por qué importa.
void test_ghost_bug_ycoded_100_marks_visible(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 0, 100, 202, 0, 0, 203, 0, 0};
    feed_all(p, bytes, sizeof(bytes));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(0, pk.ball_x);
    TEST_ASSERT_EQUAL_INT16(0, pk.ball_y);           // 100 - 100
    TEST_ASSERT_TRUE(pk.ball_visible);               // <-- fantasma documentado
}

// --- 4) Solo arco amarillo visible ------------------------------------------
// [201,0,0, 202,120,80, 203,0,0] → yellow x=120 y=-20 visible; ball/blue no.
void test_yellow_only_visible(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 0, 0, 202, 120, 80, 203, 0, 0};
    feed_all(p, bytes, sizeof(bytes));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_TRUE(pk.goal_yellow_visible);
    TEST_ASSERT_EQUAL_INT16(120, pk.goal_yellow_x);
    TEST_ASSERT_EQUAL_INT16(-20, pk.goal_yellow_y);  // 80 - 100
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
}

// --- 5) Basura antes del HEADER1 se descarta y luego decodifica --------------
// Bytes que no son 201 antes del packet se tiran en WAIT_HEADER1 (sin contar
// como resync). El packet válido posterior se decodifica bien.
void test_garbage_before_header_then_decodes(void) {
    CameraParser p;
    const uint8_t bytes[] = {5, 99, 200, 13,
                             201, 80, 130, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, p.resync_events());  // basura pre-header no es resync
    TEST_ASSERT_EQUAL_INT16(80, p.get_packet().ball_x);
}

// --- 6) HEADER2 ausente → resync y recuperación -----------------------------
// [201,5,150, 0 (≠202)] dispara resync; luego un packet completo se decodifica.
void test_missing_header2_resyncs_and_recovers(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 5, 150, 0,
                             201, 80, 130, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(p.resync_events() >= 1);
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded());
    TEST_ASSERT_EQUAL_INT16(80, p.get_packet().ball_x);
}

// --- 7) CARACTERIZACIÓN R6: header como dato en stream ALINEADO no rompe -----
// Con la trama bien alineada (layout fijo de 9 bytes), un valor de dato que
// coincide con un header (ej. ball_x=201) se acepta como dato y NO desincroniza.
// El riesgo R6 real (re-lock falso) solo aparece si ya se perdió la alineación.
void test_header_value_as_ball_x_in_aligned_stream_ok(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 201, 130, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(201, pk.ball_x);         // 201 aceptado como dato
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);
    TEST_ASSERT_TRUE(pk.ball_visible);
}

// --- 8) reset() descarta el packet parcial ----------------------------------
void test_reset_discards_partial(void) {
    CameraParser p;
    p.feed(201);
    p.feed(80);          // a mitad de packet
    p.reset();           // vuelve a WAIT_HEADER1
    const uint8_t bytes[] = {201, 42, 160, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_INT16(42, p.get_packet().ball_x);
    TEST_ASSERT_EQUAL_INT16(60, p.get_packet().ball_y);  // 160 - 100
}

// --- 9) Dos packets consecutivos en un solo stream --------------------------
void test_two_consecutive_packets(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 10, 110, 202, 0, 0, 203, 0, 0,
                             201, 20, 120, 202, 0, 0, 203, 0, 0};
    TEST_ASSERT_EQUAL_INT(2, feed_all(p, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT32(2, p.packets_decoded());
    TEST_ASSERT_EQUAL_INT16(20, p.get_packet().ball_x);  // el último
    TEST_ASSERT_EQUAL_INT16(20, p.get_packet().ball_y);  // 120 - 100
}

// --- 10) Solo arco azul visible ---------------------------------------------
void test_blue_only_visible(void) {
    CameraParser p;
    const uint8_t bytes[] = {201, 0, 0, 202, 0, 0, 203, 60, 140};
    feed_all(p, bytes, sizeof(bytes));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_TRUE(pk.goal_blue_visible);
    TEST_ASSERT_EQUAL_INT16(60, pk.goal_blue_x);
    TEST_ASSERT_EQUAL_INT16(40, pk.goal_blue_y);     // 140 - 100
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_normal_decode_ball_visible);
    RUN_TEST(test_sentinel_all_not_visible);
    RUN_TEST(test_ghost_bug_ycoded_100_marks_visible);
    RUN_TEST(test_yellow_only_visible);
    RUN_TEST(test_garbage_before_header_then_decodes);
    RUN_TEST(test_missing_header2_resyncs_and_recovers);
    RUN_TEST(test_header_value_as_ball_x_in_aligned_stream_ok);
    RUN_TEST(test_reset_discards_partial);
    RUN_TEST(test_two_consecutive_packets);
    RUN_TEST(test_blue_only_visible);
    return UNITY_END();
}
