// test_line_early_escape — tests unitarios para src/shared/line_early_escape.h
// Corre en host con: pio test -e test_native -f test_line_early_escape
// (o: bash scripts/run-host-tests.sh test_line_early_escape)
//
// Cubre lo pedido (banco 2026-06-14):
//   - un blanco AISLADO sin vecino NO dispara;
//   - dos blancos VECINOS sí dan evidencia + vector;
//   - un sensor DEFECTUOSO se excluye del centroide;
//   - TODO-BLANCO = lifted invalida la lectura;
//   - el vector apunta OPUESTO a la línea.

#include <unity.h>
#include <cmath>
#include <cstring>
#include "line_early_escape.h"
#include "line_neighbors.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Helpers / geometría de prueba
// ============================================================================

// Anillo de 8 sensores en cruz+diagonales, R=80mm, convención del firmware
// (0°=frente=+Y, horario+, +90°=+X). Índices:
//   0 = frente (+Y),  2 = derecha (+X),  4 = atrás (-Y),  6 = izquierda (-X),
//   1/3/5/7 = diagonales. Posiciones a mano para que el centroide sea obvio.
static void fill_ring8(LeeSensorPos* pos) {
    const float R = 80.0f;
    const float D = R * 0.70710678f;  // R/√2 para las diagonales
    pos[0] = {  0.0f,  R   };  // frente
    pos[1] = {  D   ,  D   };  // frente-derecha
    pos[2] = {  R   ,  0.0f};  // derecha
    pos[3] = {  D   , -D   };  // atrás-derecha
    pos[4] = {  0.0f, -R   };  // atrás
    pos[5] = { -D   , -D   };  // atrás-izquierda
    pos[6] = { -R   ,  0.0f};  // izquierda
    pos[7] = { -D   ,  D   };  // frente-izquierda
}

// Normaliza un ángulo a (-180, +180] para comparar.
static float norm180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;
    return a;
}

// |diferencia angular| en grados, robusta al wrap (179 vs -179 = 2°, no 358°).
static float ang_abs_diff(float a, float b) {
    return std::fabs(norm180(a - b));
}

// ============================================================================
// 1) Un blanco AISLADO sin vecino NO dispara
// ============================================================================

void test_isolated_white_no_neighbor_does_not_fire(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // SOLO el sensor 0 cruza su umbral (raw muy por encima). Sus vecinos (7 y 1)
    // siguen en carpeta → el filtro espacial lo descarta.
    raw[0] = 900;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_FALSE(r.line_evidence);   // no hay confiable
    TEST_ASSERT_FALSE(r.imminent);
    TEST_ASSERT_FALSE(r.lifted);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
}

// ============================================================================
// 2) Dos blancos VECINOS sí dan evidencia + vector
// ============================================================================

void test_two_adjacent_whites_give_evidence_and_vector(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Sensores 0 (frente) y 1 (frente-derecha): vecinos → ambos confiables.
    raw[0] = 900;
    raw[1] = 900;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);
    TEST_ASSERT_FALSE(r.lifted);
    TEST_ASSERT_TRUE(r.confidence > 0);

    // La línea está al frente-derecha (centroide entre 0° y 45° → ~22.5°).
    // El escape debe apuntar OPUESTO: ~22.5 + 180 = ~202.5 → -157.5° (atrás-izq).
    float esc = norm180(r.escape_angle_deg);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -157.5f, esc);
}

// ============================================================================
// 3) El vector apunta OPUESTO a la línea (caso simple, frente puro)
// ============================================================================

void test_escape_points_opposite_line_front(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Línea al FRENTE: sensores 7, 0, 1 (centro en 0° = +Y). Centroide simétrico → 0°.
    raw[7] = raw[0] = raw[1] = 900;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);
    // Escape debe ser hacia ATRÁS: 180° (o -180°, mismo punto).
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 180.0f, ang_abs_diff(r.escape_angle_deg, 0.0f));
}

void test_escape_points_opposite_line_right(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Línea a la DERECHA: sensores 1, 2, 3 (centro en 2 = +90° = +X). Centroide → +90°.
    raw[1] = raw[2] = raw[3] = 900;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);
    // Escape hacia la IZQUIERDA: -90°.
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -90.0f, norm180(r.escape_angle_deg));
}

// ============================================================================
// 4) Sensor DEFECTUOSO excluido del centroide
// ============================================================================

void test_unhealthy_sensor_excluded_from_centroid(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Línea real a la DERECHA (1,2,3). Además el sensor 6 (izquierda) está
    // CLAVADO en saturación pero MARCADO defectuoso → debe ignorarse aunque
    // tenga un vecino "blanco" que también clavamos para tentar al filtro.
    raw[1] = raw[2] = raw[3] = 900;
    raw[6] = 1023; healthy[6] = false;   // defectuoso
    raw[5] = 900;  // vecino de 6, sano: si 6 contara, sesgaría el escape

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);
    // Si el sensor 6 (izquierda, -X) NO se excluyera, el centroide se correría
    // hacia atrás-izquierda y el escape dejaría de apuntar a la izquierda pura.
    // Con 6 excluido, la línea sigue dominada por la derecha → escape ≈ -90°
    // (hacia la izquierda), tolerancia amplia por el aporte de 5.
    float esc = norm180(r.escape_angle_deg);
    // El escape debe seguir en el semiplano izquierdo (X negativo): cos(esc)<0
    // en convención atan2(x,y) ⇒ |esc| > 90°. Chequeo robusto e inequívoco.
    float esc_x = std::sin(esc * 3.14159265f / 180.0f);  // componente X del escape
    TEST_ASSERT_TRUE(esc_x < 0.0f);  // escape apunta a la izquierda
}

void test_unhealthy_isolated_white_does_not_fire(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Dos sensores vecinos cruzan el umbral, PERO ambos están defectuosos →
    // ninguno cuenta → sin evidencia (la salud manda sobre la geometría).
    raw[0] = raw[1] = 1023;
    healthy[0] = healthy[1] = false;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_FALSE(r.line_evidence);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
}

// ============================================================================
// 5) TODO-BLANCO = lifted invalida
// ============================================================================

void test_all_white_is_lifted_and_invalidates(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 900; thr[i] = 500; healthy[i] = true; }

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.lifted);
    // Lectura inválida: NO se reporta evidencia ni escape.
    TEST_ASSERT_FALSE(r.line_evidence);
    TEST_ASSERT_FALSE(r.imminent);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.escape_angle_deg);
}

void test_all_white_among_healthy_only(void) {
    // lifted se evalúa sobre los sensores SANOS: si 6 de 8 están defectuosos y
    // los 2 sanos leen blanco, eso es 2/2 = 100% de los sanos → lifted.
    // (Es el comportamiento conservador correcto: con casi todo el anillo
    //  inutilizable y lo poco sano saturado, no confiamos en la lectura.)
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = false; }

    healthy[0] = healthy[1] = true;   // solo 2 sanos
    raw[0] = raw[1] = 900;            // y ambos blancos

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.lifted);
    TEST_ASSERT_FALSE(r.line_evidence);
}

void test_strong_line_not_lifted_32_ring(void) {
    // Una línea FUERTE pero plausible (≈1/3 del anillo) NO debe leerse como lifted.
    constexpr int N = 32;
    LeeSensorPos pos[N];
    for (int i = 0; i < N; ++i) {
        const float a = (2.0f * 3.14159265f * i) / N;  // ángulo polar uniforme
        pos[i] = { 80.0f * std::sin(a), 80.0f * std::cos(a) };
    }
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // 10 de 32 blancos consecutivos (≈31% < 7/8): línea grande de esquina, válida.
    for (int i = 0; i < 10; ++i) raw[i] = 900;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_FALSE(r.lifted);
    TEST_ASSERT_TRUE(r.line_evidence);
    TEST_ASSERT_TRUE(r.imminent);   // 10 >> imminent_count(3)
}

// ============================================================================
// 6) Inminencia, histéresis y guardas
// ============================================================================

void test_imminent_triggers_at_threshold_count(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // 2 blancos vecinos confiables: hay evidencia pero NO inminente (default=3).
    raw[0] = raw[1] = 900;
    LeeResult r2 = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r2.line_evidence);
    TEST_ASSERT_FALSE(r2.imminent);

    // 3 blancos vecinos confiables: ahora SÍ inminente.
    bool prev2[N] = {};
    raw[2] = 900;
    LeeResult r3 = lee_compute(raw, thr, healthy, pos, prev2, N);
    TEST_ASSERT_TRUE(r3.line_evidence);
    TEST_ASSERT_TRUE(r3.imminent);
}

void test_hysteresis_band_blocks_borderline_entry(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Dos vecinos JUSTO en el umbral (500) — sin superar la banda (umbral+20=520).
    // No deben entrar a blanco desde carpeta → sin evidencia.
    raw[0] = raw[1] = 510;   // > umbral pero < umbral+banda
    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_FALSE(r.line_evidence);

    // Subiendo a 520 (umbral+banda) sí entran.
    bool prev2[N] = {};
    raw[0] = raw[1] = 520;
    LeeResult r2 = lee_compute(raw, thr, healthy, pos, prev2, N);
    TEST_ASSERT_TRUE(r2.line_evidence);
}

void test_hysteresis_keeps_white_until_low_band(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // Tick 1: entran a blanco (520).
    raw[0] = raw[1] = 520;
    lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(prev[0]);
    TEST_ASSERT_TRUE(prev[1]);

    // Tick 2: bajan a 490 (entre umbral-banda=480 y umbral=500). Con histéresis
    // SIGUEN blancos (no caen hasta < 480).
    raw[0] = raw[1] = 490;
    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);

    // Tick 3: caen a 470 (< umbral-banda) → salen de blanco.
    raw[0] = raw[1] = 470;
    LeeResult r2 = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_FALSE(r2.line_evidence);
}

void test_individual_thresholds_per_sensor(void) {
    // Cada sensor compara contra SU propio umbral. Un mismo raw puede ser blanco
    // en un sensor (umbral bajo) y carpeta en otro (umbral alto).
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 300; healthy[i] = true; }

    // Umbrales individuales: sensores 0 y 1 con umbral bajo (200) → 300 es blanco.
    // El resto con umbral alto (900) → 300 es carpeta.
    for (int i = 0; i < N; ++i) thr[i] = 900;
    thr[0] = thr[1] = 200;

    LeeResult r = lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(r.line_evidence);   // 0 y 1 vecinos confiables
    TEST_ASSERT_FALSE(r.imminent);       // solo 2 < 3
}

void test_null_and_zero_guards(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N];
    for (int i = 0; i < N; ++i) { raw[i] = 900; thr[i] = 500; healthy[i] = true; }

    LeeResult r0 = lee_compute(nullptr, thr, healthy, pos, nullptr, N);
    TEST_ASSERT_FALSE(r0.line_evidence);
    TEST_ASSERT_FALSE(r0.lifted);

    LeeResult rn = lee_compute(raw, thr, healthy, pos, nullptr, 0);
    TEST_ASSERT_FALSE(rn.line_evidence);

    // prev_white nullptr es válido (sin memoria temporal): igual detecta.
    LeeResult rp = lee_compute(raw, thr, healthy, pos, nullptr, N);
    TEST_ASSERT_TRUE(rp.lifted);  // todos blancos
}

void test_prev_white_updates_in_place(void) {
    constexpr int N = 8;
    LeeSensorPos pos[N]; fill_ring8(pos);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    raw[2] = raw[3] = 900;  // derecha
    lee_compute(raw, thr, healthy, pos, prev, N);
    TEST_ASSERT_TRUE(prev[2]);
    TEST_ASSERT_TRUE(prev[3]);
    TEST_ASSERT_FALSE(prev[0]);
    TEST_ASSERT_FALSE(prev[5]);
}

// ============================================================================
// 7) MODO LUT DE VECINO FÍSICO (cfg.neighbors) — NUEVO
//    El default (sin LUT) ya está cubierto por los 18 tests de arriba: este
//    bloque prueba SOLO el camino nuevo, con la geometría REAL del PCB DOWN
//    donde el vecino de ÍNDICE puede estar a 14 cm del vecino FÍSICO.
// ============================================================================

// Coords reales del PCB DOWN (= SENSOR_POS de sensor_geometry.cpp). Copiadas
// acá para que el módulo PURO no dependa de ese .cpp.
static const LeeSensorPos DOWN_POS[32] = {
    {-36.28f, +82.04f}, {-30.57f, +75.06f}, {-20.28f, +74.17f}, {-10.12f, +74.17f},
    {+10.20f, +74.17f}, {+20.36f, +74.17f}, {+30.52f, +75.18f}, {+36.49f, +82.04f},
    {-86.32f, +12.19f}, {-86.32f,  -0.51f}, {-84.92f, -13.21f}, {-81.24f, -26.04f},
    {-67.65f, -50.04f}, {-60.03f, -60.20f}, {-48.98f, -69.09f}, {-36.28f, -76.71f},
    {+36.36f, -76.71f}, {+49.31f, -69.09f}, {+60.87f, -60.20f}, {+69.63f, -50.04f},
    {+82.21f, -25.91f}, {+85.64f, -13.21f}, {+87.29f,  -0.51f}, {+86.40f, +12.06f},
    {-22.82f, +50.93f}, {-12.66f, +50.93f}, {+12.74f, +51.05f}, {+22.90f, +50.93f},
    {+34.55f,  -9.51f}, {+34.55f, -19.67f}, {-33.25f,  -9.51f}, {-33.25f, -19.80f},
};

// line_neighbors usa su PROPIO struct de posición; copiamos bit a bit (mismo
// layout x_mm,y_mm). Construye la LUT física desde DOWN_POS.
static void build_down_lut(LineNeighbors* lut) {
    LnSensorPos p[32];
    for (int i = 0; i < 32; ++i) { p[i].x_mm = DOWN_POS[i].x_mm; p[i].y_mm = DOWN_POS[i].y_mm; }
    ln_build(p, 32, lut);
}

// EL test clave: idx 7 y 8 son contiguos de ÍNDICE pero están a 141mm. Con el
// filtro de índice (sin LUT) contarían como vecinos y darían evidencia. Con la
// LUT física NO son vecinos → cada uno queda aislado → SIN evidencia.
void test_lut_index_far_pair_does_not_fire(void) {
    constexpr int N = 32;
    LineNeighbors lut; build_down_lut(&lut);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    // SOLO idx 7 y 8 blancos (contiguos de índice, lejanos físicos).
    raw[7] = raw[8] = 900;

    // (a) Sin LUT (default): el filtro de índice los toma como vecinos → evidencia.
    LeeConfig cfg_idx;  // neighbors == nullptr
    bool prevA[N] = {};
    LeeResult r_idx = lee_compute(raw, thr, healthy, DOWN_POS, prevA, N, cfg_idx);
    TEST_ASSERT_TRUE(r_idx.line_evidence);   // comportamiento histórico (vecino de índice)

    // (b) Con LUT física: 7 y 8 NO son vecinos → ambos aislados → SIN evidencia.
    LeeConfig cfg_phys; cfg_phys.neighbors = &lut;
    bool prevB[N] = {};
    LeeResult r_phys = lee_compute(raw, thr, healthy, DOWN_POS, prevB, N, cfg_phys);
    TEST_ASSERT_FALSE(r_phys.line_evidence);  // corregido: no son adyacentes físicos
}

// Dos sensores FÍSICAMENTE adyacentes (idx 6 y 7, a 9mm) SÍ disparan en modo LUT.
void test_lut_physical_pair_fires(void) {
    constexpr int N = 32;
    LineNeighbors lut; build_down_lut(&lut);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    raw[6] = raw[7] = 900;   // vecinos físicos reales (9.1mm)

    LeeConfig cfg; cfg.neighbors = &lut;
    LeeResult r = lee_compute(raw, thr, healthy, DOWN_POS, prev, N, cfg);
    TEST_ASSERT_TRUE(r.line_evidence);
    TEST_ASSERT_FALSE(r.lifted);
    TEST_ASSERT_TRUE(r.confidence > 0);
}

// Un blanco aislado (sin vecino físico blanco) NO dispara en modo LUT.
void test_lut_isolated_white_does_not_fire(void) {
    constexpr int N = 32;
    LineNeighbors lut; build_down_lut(&lut);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }

    raw[5] = 900;   // solo uno

    LeeConfig cfg; cfg.neighbors = &lut;
    LeeResult r = lee_compute(raw, thr, healthy, DOWN_POS, prev, N, cfg);
    TEST_ASSERT_FALSE(r.line_evidence);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
}

// LUT vacía (nullptr dentro de cfg apuntando a LUT sin construir) = fail-safe:
// ln_has_white_neighbor da false para todo → sin evidencia (lado conservador),
// nunca un falso positivo. Aquí usamos una LUT construida con pos=nullptr.
void test_lut_empty_is_conservative(void) {
    constexpr int N = 32;
    LineNeighbors empty_lut;
    ln_build(nullptr, 32, &empty_lut);   // queda count=0 en todos
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 100; thr[i] = 500; healthy[i] = true; }
    raw[6] = raw[7] = 900;   // vecinos físicos reales, pero la LUT está vacía

    LeeConfig cfg; cfg.neighbors = &empty_lut;
    LeeResult r = lee_compute(raw, thr, healthy, DOWN_POS, prev, N, cfg);
    // LUT vacía => ningún confiable => sin evidencia. Degrada conservador.
    TEST_ASSERT_FALSE(r.line_evidence);
}

// El modo LUT NO altera la detección de lifted (todo-blanco sigue invalidando):
// lifted se evalúa sobre white_raw, ANTES del filtro espacial, así que la LUT no
// lo toca. Una franja imposible (casi todo el anillo) sigue marcando lifted.
void test_lut_does_not_break_lifted(void) {
    constexpr int N = 32;
    LineNeighbors lut; build_down_lut(&lut);
    uint16_t raw[N], thr[N];
    bool healthy[N], prev[N] = {};
    for (int i = 0; i < N; ++i) { raw[i] = 900; thr[i] = 500; healthy[i] = true; }  // todo blanco

    LeeConfig cfg; cfg.neighbors = &lut;
    LeeResult r = lee_compute(raw, thr, healthy, DOWN_POS, prev, N, cfg);
    TEST_ASSERT_TRUE(r.lifted);
    TEST_ASSERT_FALSE(r.line_evidence);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // 1) aislado no dispara
    RUN_TEST(test_isolated_white_no_neighbor_does_not_fire);

    // 2) vecinos dan evidencia + vector
    RUN_TEST(test_two_adjacent_whites_give_evidence_and_vector);

    // 3) escape opuesto a la línea
    RUN_TEST(test_escape_points_opposite_line_front);
    RUN_TEST(test_escape_points_opposite_line_right);

    // 4) sensor defectuoso excluido
    RUN_TEST(test_unhealthy_sensor_excluded_from_centroid);
    RUN_TEST(test_unhealthy_isolated_white_does_not_fire);

    // 5) todo-blanco = lifted invalida
    RUN_TEST(test_all_white_is_lifted_and_invalidates);
    RUN_TEST(test_all_white_among_healthy_only);
    RUN_TEST(test_strong_line_not_lifted_32_ring);

    // 6) inminencia, histéresis, umbrales individuales, guardas
    RUN_TEST(test_imminent_triggers_at_threshold_count);
    RUN_TEST(test_hysteresis_band_blocks_borderline_entry);
    RUN_TEST(test_hysteresis_keeps_white_until_low_band);
    RUN_TEST(test_individual_thresholds_per_sensor);
    RUN_TEST(test_null_and_zero_guards);
    RUN_TEST(test_prev_white_updates_in_place);

    // 7) modo LUT de vecino físico (camino nuevo, backward-compatible)
    RUN_TEST(test_lut_index_far_pair_does_not_fire);
    RUN_TEST(test_lut_physical_pair_fires);
    RUN_TEST(test_lut_isolated_white_does_not_fire);
    RUN_TEST(test_lut_empty_is_conservative);
    RUN_TEST(test_lut_does_not_break_lifted);

    return UNITY_END();
}
