// test_cameras_parser — tests host-native del parser de cámara (src/top/cameras.cpp).
//
// Corre con:  bash scripts/run-host-tests.sh test_cameras_parser
//             (o pio test -e test_native -f test_cameras_parser)
//
// Contrato v2 (contract-schema 2) — packet de 11 bytes:
//   [201, Xp_c, Yp_c, 202, Xam_c, Yam_c, 203, Xaz_c, Yaz_c, CRC8, 254]
//   - X e Y codifican SIMÉTRICO: coded = valor + 100 (X∈[-100,100] → 0..200).
//     ⇒ pelota a la IZQUIERDA (X<0) es representable (antes colapsaba a "al frente").
//   - SENTINEL "no detectado" = byte coded == 255 (inalcanzable desde datos [0,200]).
//   - CRC8 = XOR de los 9 bytes de datos; END = 254. Frame con CRC/END malo → descartar.
//
// Truco de compilación (unity-build): run-host-tests.sh solo linkea src/shared,
// pero cameras.cpp vive en src/top. Lo incluimos inline acá; g++ lo compila como
// parte de esta unidad de traducción.
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

// Constructor de frame v2 BIEN FORMADO: toma los 6 bytes coded y calcula CRC+END.
// out[] debe tener al menos 11 bytes. Devuelve la longitud (11).
static int make_frame(uint8_t out[11],
                      uint8_t xp, uint8_t yp,
                      uint8_t xam, uint8_t yam,
                      uint8_t xaz, uint8_t yaz) {
    out[0] = CAM_HEADER1; out[1] = xp;  out[2] = yp;
    out[3] = CAM_HEADER2; out[4] = xam; out[5] = yam;
    out[6] = CAM_HEADER3; out[7] = xaz; out[8] = yaz;
    out[9]  = cam_crc8(out);   // XOR de bytes 0..8
    out[10] = CAM_END_BYTE;
    return 11;
}

// Helper: coded de una coordenada con signo (igual que la cámara: valor + 100).
static uint8_t enc(int16_t v) { return static_cast<uint8_t>(v + 100); }

// --- 1) Pelota AL CENTRO: X=0 representable, ángulo ≈ 0 ----------------------
void test_ball_center_x_zero(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(0), enc(30), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(0, pk.ball_x);     // 100 - 100
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);    // 130 - 100
    TEST_ASSERT_TRUE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, p.crc_errors());
}

// --- 2) Pelota a la DERECHA: X positivo --------------------------------------
void test_ball_right_x_positive(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(80), enc(30), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(80, pk.ball_x);    // +80 → a la derecha
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);
    TEST_ASSERT_TRUE(pk.ball_visible);
}

// --- 3) Pelota a la IZQUIERDA: X NEGATIVO representable (el fix central) -----
// En v1 esto colapsaba a 0 ("al frente"). En v2, X_coded=20 → X=-80.
void test_ball_left_x_negative_representable(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(-80), enc(30), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(-80, pk.ball_x);   // 20 - 100 = -80 → a la IZQUIERDA
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);
    TEST_ASSERT_TRUE(pk.ball_visible);
}

// --- 4) Esquina extrema izquierda-atrás: X=-100, Y=-100 sigue visible -------
// En v1 (X=0,Y=-100) colisionaba con el sentinel. En v2 el sentinel es 255/255,
// así que la esquina física más extrema es una detección válida, NO un sentinel.
void test_extreme_corner_still_visible(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(-100), enc(-100), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(-100, pk.ball_x);  // 0 - 100
    TEST_ASSERT_EQUAL_INT16(-100, pk.ball_y);
    TEST_ASSERT_TRUE(pk.ball_visible);         // ya NO se confunde con "no detectado"
}

// --- 5) SENTINEL no-blob: todo 255/255 → nada visible -----------------------
void test_sentinel_all_not_visible(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
}

// --- 6) Sentinel mixto: pelota no, arco amarillo sí, azul no ----------------
void test_mixed_sentinel_yellow_visible(void) {
    CameraParser p;
    uint8_t f[11];
    // Pelota sentinel; arco amarillo X=20, Y=-20; arco azul sentinel.
    make_frame(f, CAM_SENTINEL, CAM_SENTINEL, enc(20), enc(-20), CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_TRUE(pk.goal_yellow_visible);
    TEST_ASSERT_EQUAL_INT16(20, pk.goal_yellow_x);
    TEST_ASSERT_EQUAL_INT16(-20, pk.goal_yellow_y);
    TEST_ASSERT_FALSE(pk.goal_blue_visible);
}

// --- 7) Sentinel parcial: un solo byte coded == 255 ⇒ objeto no visible -----
// Si X es real pero Y es sentinel (o viceversa), el objeto cuenta como NO visible.
void test_partial_sentinel_marks_invisible(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(50), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    TEST_ASSERT_FALSE(p.get_packet().ball_visible);   // Y=255 sentinel → no visible
}

// --- 8) CRC malo → frame DESCARTADO, no se publica, cuenta crc_errors -------
void test_bad_crc_discarded(void) {
    CameraParser p;
    // Primero un frame bueno conocido, para tener un estado publicado de referencia.
    uint8_t good[11];
    make_frame(good, enc(10), enc(10), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    feed_all(p, good, 11);
    const int16_t ref_x = p.get_packet().ball_x;   // 10

    // Ahora un frame con CRC corrupto (flip de 1 bit en el CRC).
    uint8_t bad[11];
    make_frame(bad, enc(77), enc(40), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    bad[9] ^= 0x01;   // corromper el CRC
    TEST_ASSERT_EQUAL_INT(0, feed_all(p, bad, 11));  // NO completa packet
    TEST_ASSERT_EQUAL_UINT32(1, p.crc_errors());
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded()); // sigue siendo 1 (solo el bueno)
    TEST_ASSERT_EQUAL_INT16(ref_x, p.get_packet().ball_x); // packet sin cambiar (no se pisó)
}

// --- 9) Bit-flip en un byte de datos lo atrapa el CRC -----------------------
void test_data_bitflip_caught_by_crc(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(50), enc(50), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    f[1] ^= 0x02;   // corromper Xp DESPUÉS de calcular el CRC → CRC ya no matchea
    TEST_ASSERT_EQUAL_INT(0, feed_all(p, f, 11));
    TEST_ASSERT_EQUAL_UINT32(1, p.crc_errors());
}

// --- 10) END byte ausente → frame descartado, resync --------------------------
void test_missing_end_byte_discarded(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(33), enc(33), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    f[10] = 0x00;   // END corrupto (no es 254)
    TEST_ASSERT_EQUAL_INT(0, feed_all(p, f, 11));
    TEST_ASSERT_EQUAL_UINT32(0, p.packets_decoded());
    TEST_ASSERT_TRUE(p.resync_events() >= 1);
}

// --- 11) Basura antes del HEADER1 se descarta y luego decodifica -------------
void test_garbage_before_header_then_decodes(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, enc(80), enc(30), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    uint8_t stream[15] = {5, 99, 200, 13};
    for (int i = 0; i < 11; ++i) stream[4 + i] = f[i];
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, stream, 15));
    TEST_ASSERT_EQUAL_UINT32(1, p.packets_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, p.resync_events());  // basura pre-header no es resync
    TEST_ASSERT_EQUAL_INT16(80, p.get_packet().ball_x);
}

// --- 12) HEADER2 ausente → resync y recuperación ----------------------------
void test_missing_header2_resyncs_and_recovers(void) {
    CameraParser p;
    // Frame corrupto: HEADER2 reemplazado por 0 a mitad de trama.
    uint8_t broken[4] = {CAM_HEADER1, 105, 150, 0};
    feed_all(p, broken, 4);
    TEST_ASSERT_TRUE(p.resync_events() >= 1);
    // Luego un frame válido completo se decodifica bien.
    uint8_t f[11];
    make_frame(f, enc(80), enc(30), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    TEST_ASSERT_EQUAL_INT16(80, p.get_packet().ball_x);
}

// --- 13) Byte de DATOS que coincide con un header en stream alineado --------
// Con la trama bien alineada (layout fijo de 11 bytes + CRC + END), un dato que
// coincide con un header NO puede pasar realmente (coords ∈ [0,200]); pero
// probamos que si el CRC matchea, el parser lo trata como dato y NO se desincroniza.
// Forzamos Xp = 201 (valor de HEADER1) y recomputamos el CRC para que sea válido.
void test_data_equal_header_value_aligned_ok(void) {
    CameraParser p;
    uint8_t f[11];
    // Construimos a mano con Xp = 201 (== HEADER1) y CRC consistente.
    f[0] = CAM_HEADER1; f[1] = 201; f[2] = enc(30);
    f[3] = CAM_HEADER2; f[4] = CAM_SENTINEL; f[5] = CAM_SENTINEL;
    f[6] = CAM_HEADER3; f[7] = CAM_SENTINEL; f[8] = CAM_SENTINEL;
    f[9] = cam_crc8(f); f[10] = CAM_END_BYTE;
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_EQUAL_INT16(101, pk.ball_x);   // 201 - 100 = 101 (aceptado como dato)
    TEST_ASSERT_EQUAL_INT16(30, pk.ball_y);
    TEST_ASSERT_TRUE(pk.ball_visible);
}

// --- 14) reset() descarta el packet parcial ---------------------------------
void test_reset_discards_partial(void) {
    CameraParser p;
    p.feed(CAM_HEADER1);
    p.feed(120);          // a mitad de packet
    p.reset();            // vuelve a WAIT_HEADER1
    uint8_t f[11];
    make_frame(f, enc(-58), enc(60), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    TEST_ASSERT_EQUAL_INT16(-58, p.get_packet().ball_x);
    TEST_ASSERT_EQUAL_INT16(60, p.get_packet().ball_y);
}

// --- 15) Dos packets consecutivos en un solo stream -------------------------
void test_two_consecutive_packets(void) {
    CameraParser p;
    uint8_t a[11], b[11];
    make_frame(a, enc(-90), enc(10), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    make_frame(b, enc(90),  enc(20), CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL);
    uint8_t stream[22];
    for (int i = 0; i < 11; ++i) { stream[i] = a[i]; stream[11 + i] = b[i]; }
    TEST_ASSERT_EQUAL_INT(2, feed_all(p, stream, 22));
    TEST_ASSERT_EQUAL_UINT32(2, p.packets_decoded());
    TEST_ASSERT_EQUAL_INT16(90, p.get_packet().ball_x);  // el último (a la derecha)
    TEST_ASSERT_EQUAL_INT16(20, p.get_packet().ball_y);
}

// --- 16) Solo arco azul visible (X izquierda negativo) ----------------------
void test_blue_only_visible_left(void) {
    CameraParser p;
    uint8_t f[11];
    make_frame(f, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, CAM_SENTINEL, enc(-40), enc(40));
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, f, 11));
    const CameraPacket& pk = p.get_packet();
    TEST_ASSERT_FALSE(pk.ball_visible);
    TEST_ASSERT_FALSE(pk.goal_yellow_visible);
    TEST_ASSERT_TRUE(pk.goal_blue_visible);
    TEST_ASSERT_EQUAL_INT16(-40, pk.goal_blue_x);
    TEST_ASSERT_EQUAL_INT16(40, pk.goal_blue_y);
}

// --- 17) crc8 del parser coincide con el del .py (mismo XOR) -----------------
// Vector conocido: data de un frame de pelota al centro, arcos sentinel.
void test_crc8_known_vector(void) {
    uint8_t data[9] = {201, 100, 130, 202, 255, 255, 203, 255, 255};
    // XOR a mano: 201^100^130^202^255^255^203^255^255
    uint8_t expected = 0;
    for (int i = 0; i < 9; ++i) expected ^= data[i];
    TEST_ASSERT_EQUAL_UINT8(expected, cam_crc8(data));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ball_center_x_zero);
    RUN_TEST(test_ball_right_x_positive);
    RUN_TEST(test_ball_left_x_negative_representable);
    RUN_TEST(test_extreme_corner_still_visible);
    RUN_TEST(test_sentinel_all_not_visible);
    RUN_TEST(test_mixed_sentinel_yellow_visible);
    RUN_TEST(test_partial_sentinel_marks_invisible);
    RUN_TEST(test_bad_crc_discarded);
    RUN_TEST(test_data_bitflip_caught_by_crc);
    RUN_TEST(test_missing_end_byte_discarded);
    RUN_TEST(test_garbage_before_header_then_decodes);
    RUN_TEST(test_missing_header2_resyncs_and_recovers);
    RUN_TEST(test_data_equal_header_value_aligned_ok);
    RUN_TEST(test_reset_discards_partial);
    RUN_TEST(test_two_consecutive_packets);
    RUN_TEST(test_blue_only_visible_left);
    RUN_TEST(test_crc8_known_vector);
    return UNITY_END();
}
