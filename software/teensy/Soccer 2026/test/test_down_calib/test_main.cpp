// test_down_calib — pio test -e test_native -f test_down_calib
#include <unity.h>
#include <cstdint>
#include "line_calib.h"
#include "calib_storage.h"   // cs_serialize/cs_deserialize: la ruta EEPROM real
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_static_threshold_is_midpoint(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    TEST_ASSERT_EQUAL_UINT16(500, c.threshold);
}

void test_adapt_carpet_moves_toward_reading_when_off_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 260, /*on_line=*/false, 0.05f);
    TEST_ASSERT_UINT16_WITHIN(15, 260, c.carpet);
    TEST_ASSERT_UINT16_WITHIN(15, (260+800)/2, c.threshold);
}

void test_adapt_does_not_drift_when_on_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 790, /*on_line=*/true, 0.05f);
    TEST_ASSERT_EQUAL_UINT16(200, c.carpet);
}

void test_suspect_when_margin_too_small(void){
    SensorCalib cs[2]; lc_set_static(cs[0],400,900); lc_set_static(cs[1],500,560);
    TEST_ASSERT_TRUE(lc_is_suspect(cs,2,/*min_margin=*/100));
    SensorCalib ok[1]; lc_set_static(ok[0],200,800);
    TEST_ASSERT_FALSE(lc_is_suspect(ok,1,100));
}

void test_adapt_alpha_gt1_clamps_to_1(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    lc_adapt_carpet(c, 260, false, 2.0f);  // alpha clamped a 1 → un paso a 260
    TEST_ASSERT_EQUAL_UINT16(260, c.carpet);
}

// ============================================================================
// CARACTERIZACION del lazy-init de calib del DOWN (audit 2026-06-05
// 'down-calib-lazyinit-test-gap', LOW/test-gap).
//
// La MAQUINA DE ESTADOS de inicialización vive en glue Arduino no-testeable
// (src/down/comm_central.cpp): usa <Arduino.h>, line_ring, otos, down_tx. Acá
// la MIRROREAMOS en una mini-FSM pura DENTRO del test (caracterización: si el
// glue cambia, este mirror debe actualizarse a mano) y la testeamos con las
// MISMAS funciones puras que el glue usa para realizar cada transición
// (cs_serialize/cs_deserialize para la rama EEPROM, lc_set_static para la
// derivación). Así el test ata la SECUENCIA DE PRECEDENCIA — EEPROM cargado
// gana sobre derivación; sin EEPROM se deriva en el primer uso — sin tocar
// comm_central.cpp/otos.cpp ni ningún glue.
//
// MIRROR de comm_central.cpp (citas de línea, commit actual):
//   - g_dm_init = false al boot                                        (l.38)
//   - comm_central_load_persisted_calib(): ec_load_calibration OK
//        → g_dm_init = true  (EEPROM gana, bloquea re-derivación)      (l.184-194)
//   - comm_central_send_line_urgent(): if(!g_dm_init) derive_...()     (l.116-118)
//   - derive_calib_from_line_ring(): lc_set_static + g_dm_init = true  (l.43-50)
//   - handle_frame CENTRAL_CALIB_LINE paso 0 (carpet): g_dm_init=false (l.59-63)
//   - handle_frame CENTRAL_CALIB_LINE paso 1 (white): derive_...()     (l.64-84)
// ec_load_calibration == cs_deserialize(buffer-de-EEPROM)        (eeprom_calib.cpp l.34-41)
// ============================================================================

namespace {

// Estados del lazy-init, espejo de la FSM implícita del glue (g_dm_init + fuente).
enum class CalibState { PENDING, EEPROM_LOADED, DERIVED };

// Mini-FSM pura que MIRROREA la decisión de precedencia de comm_central.cpp.
// `g_dm_init` (true tras EEPROM_LOADED/DERIVED) es el gate que el glue usa para
// NO re-derivar. La FSM no toca SensorCalib; las funciones puras reales
// (cs_deserialize / lc_set_static) hacen el llenado en los tests de abajo.
struct CalibInitFsm {
    CalibState state = CalibState::PENDING;   // boot: g_dm_init = false

    bool dm_init() const { return state != CalibState::PENDING; }

    // comm_central_load_persisted_calib(): solo gana si el EEPROM valida.
    void on_load_attempt(bool ec_load_ok) {
        if (ec_load_ok) state = CalibState::EEPROM_LOADED;  // bloquea derivación
        // load falló → queda como estaba (PENDING al boot) → fallback lazy.
    }

    // comm_central_send_line_urgent(): deriva SOLO si aún no hay calib (gate).
    // Devuelve true si EN ESTE send se ejecutó la derivación (lc_set_static).
    bool on_send() {
        if (!dm_init()) { state = CalibState::DERIVED; return true; }
        return false;   // EEPROM_LOADED o ya DERIVED → no re-deriva.
    }

    // CENTRAL_CALIB_LINE paso 0 (carpet): re-armar para re-derivar al próximo send.
    void on_calib_carpet() { state = CalibState::PENDING; }

    // CENTRAL_CALIB_LINE paso 1 (white): deriva YA (y el glue persiste a EEPROM).
    void on_calib_white() { state = CalibState::DERIVED; }
};

// Helper: arma un buffer de EEPROM válido (lo que ec_save_calibration habría
// escrito) para los `n` sensores dados. Usa la ruta de serialización REAL.
void make_eeprom_buf(uint8_t* buf, const SensorCalib* calib, int n) {
    const int written = cs_serialize(buf, CS_PAYLOAD_SIZE, calib, n);
    TEST_ASSERT_EQUAL_INT(CS_PAYLOAD_SIZE, written);
}

}  // namespace

// --- FSM mirror: secuencia de precedencia ----------------------------------

void test_fsm_boot_state_is_pending(void){
    CalibInitFsm fsm;
    TEST_ASSERT_FALSE(fsm.dm_init());            // g_dm_init = false al boot
    TEST_ASSERT_TRUE(fsm.state == CalibState::PENDING);
}

void test_fsm_eeprom_load_wins_and_blocks_derivation(void){
    // load OK → EEPROM_LOADED; el primer send NO re-deriva (EEPROM gana).
    CalibInitFsm fsm;
    fsm.on_load_attempt(/*ec_load_ok=*/true);
    TEST_ASSERT_TRUE(fsm.state == CalibState::EEPROM_LOADED);
    TEST_ASSERT_TRUE(fsm.dm_init());
    const bool derived = fsm.on_send();
    TEST_ASSERT_FALSE(derived);                  // bloqueada la re-derivación
    TEST_ASSERT_TRUE(fsm.state == CalibState::EEPROM_LOADED);
}

void test_fsm_no_eeprom_derives_on_first_send_once(void){
    // load FALLA → sigue PENDING; el PRIMER send deriva; el segundo ya no.
    CalibInitFsm fsm;
    fsm.on_load_attempt(/*ec_load_ok=*/false);
    TEST_ASSERT_TRUE(fsm.state == CalibState::PENDING);
    TEST_ASSERT_TRUE(fsm.on_send());             // 1er send: deriva
    TEST_ASSERT_TRUE(fsm.state == CalibState::DERIVED);
    TEST_ASSERT_FALSE(fsm.on_send());            // 2do send: NO re-deriva
}

void test_fsm_carpet_step_rearms_then_next_send_rederives(void){
    // CALIB_LINE paso 0 vuelve a PENDING (calib incompleta) → re-derivar luego.
    CalibInitFsm fsm;
    fsm.on_send();                               // estado DERIVED
    TEST_ASSERT_TRUE(fsm.dm_init());
    fsm.on_calib_carpet();                       // paso 0 → PENDING
    TEST_ASSERT_FALSE(fsm.dm_init());
    TEST_ASSERT_TRUE(fsm.on_send());             // re-deriva en el próximo send
}

void test_fsm_white_step_derives_immediately(void){
    // CALIB_LINE paso 1 deriva YA (sin esperar al send) → el send no re-deriva.
    CalibInitFsm fsm;
    fsm.on_calib_white();                        // paso 2: deriva ya
    TEST_ASSERT_TRUE(fsm.state == CalibState::DERIVED);
    TEST_ASSERT_FALSE(fsm.on_send());
}

void test_fsm_eeprom_then_recalib_carpet_white_full_sequence(void){
    // Secuencia completa: arranca de EEPROM, luego recalibración manual en banco.
    CalibInitFsm fsm;
    fsm.on_load_attempt(true);                   // EEPROM_LOADED
    TEST_ASSERT_FALSE(fsm.on_send());            // no re-deriva
    fsm.on_calib_carpet();                       // paso 0: re-arma
    TEST_ASSERT_FALSE(fsm.dm_init());            // calib incompleta, no persiste
    fsm.on_calib_white();                        // paso 1: deriva ya
    TEST_ASSERT_TRUE(fsm.state == CalibState::DERIVED);
    TEST_ASSERT_FALSE(fsm.on_send());            // ya derivada, no re-deriva
}

// --- Precedencia anclada a las funciones puras REALES ----------------------
// Estos tests usan cs_deserialize (rama EEPROM) y lc_set_static (rama
// derivación) para mostrar que, cuando ambas fuentes existen, el VALOR que
// sobrevive es el del EEPROM — porque la FSM bloquea la derivación.

void test_eeprom_value_survives_over_line_ring_derivation(void){
    constexpr int N = 4;
    // (1) Fuente EEPROM: una referencia de BLANCO real medida en banco.
    SensorCalib eeprom_src[N];
    for (int i = 0; i < N; ++i) lc_set_static(eeprom_src[i], 180, 870);
    uint8_t buf[CS_PAYLOAD_SIZE];
    make_eeprom_buf(buf, eeprom_src, N);

    // Estado inicial del modelo (lo que tendría g_dm.calib pre-init).
    SensorCalib calib[N] = {};

    // (2) FSM: intentar cargar EEPROM (cs_deserialize == ec_load_calibration).
    CalibInitFsm fsm;
    const bool ok = cs_deserialize(buf, CS_PAYLOAD_SIZE, calib, N);
    TEST_ASSERT_TRUE(ok);
    fsm.on_load_attempt(ok);                     // → EEPROM_LOADED
    TEST_ASSERT_TRUE(fsm.state == CalibState::EEPROM_LOADED);

    // (3) Primer send: la FSM bloquea la derivación → NO pisamos con line_ring.
    if (fsm.on_send()) {
        // (no debería entrar) derivación desde valores boot del anillo
        for (int i = 0; i < N; ++i) lc_set_static(calib[i], 300, 600);
    }
    // El valor del EEPROM sobrevive intacto.
    for (int i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_UINT16(180, calib[i].carpet);
        TEST_ASSERT_EQUAL_UINT16(870, calib[i].white);
        TEST_ASSERT_EQUAL_UINT16(525, calib[i].threshold);  // punto medio
    }
}

void test_no_eeprom_derives_line_ring_values_on_first_use(void){
    constexpr int N = 4;
    // (1) EEPROM virgen (todo cero) → cs_deserialize falla.
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    SensorCalib calib[N] = {};
    const bool ok = cs_deserialize(buf, CS_PAYLOAD_SIZE, calib, N);
    TEST_ASSERT_FALSE(ok);

    CalibInitFsm fsm;
    fsm.on_load_attempt(ok);                     // load falló → sigue PENDING
    TEST_ASSERT_TRUE(fsm.state == CalibState::PENDING);

    // (2) Primer send: la FSM ordena derivar; emulamos derive_calib_from_line_ring
    //     con lc_set_static sobre los promedios del anillo (carpet=300, white=600).
    TEST_ASSERT_TRUE(fsm.on_send());
    for (int i = 0; i < N; ++i) lc_set_static(calib[i], 300, 600);

    // Valores derivados del anillo, no del EEPROM (que estaba vacío).
    for (int i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_UINT16(300, calib[i].carpet);
        TEST_ASSERT_EQUAL_UINT16(600, calib[i].white);
        TEST_ASSERT_EQUAL_UINT16(450, calib[i].threshold);  // punto medio
    }
}

// ── lc_count_weak (banco 2026-06-12): cuenta + máscara de sensores débiles ──

void test_count_weak_none(void){
    SensorCalib cs[4];
    for(int i=0;i<4;++i) lc_set_static(cs[i],200,800);
    uint32_t mask = 0xDEAD;
    TEST_ASSERT_EQUAL_INT(0, lc_count_weak(cs,4,100,&mask));
    TEST_ASSERT_EQUAL_UINT32(0u, mask);
}

void test_count_weak_mask_marks_right_sensors(void){
    SensorCalib cs[4];
    lc_set_static(cs[0],200,800);   // sano
    lc_set_static(cs[1],500,560);   // débil (60 < 100)
    lc_set_static(cs[2],200,800);   // sano
    lc_set_static(cs[3],500,520);   // débil (20 < 100)
    uint32_t mask = 0;
    TEST_ASSERT_EQUAL_INT(2, lc_count_weak(cs,4,100,&mask));
    TEST_ASSERT_EQUAL_UINT32((1u<<1)|(1u<<3), mask);
}

void test_count_weak_all_weak_and_null_mask(void){
    SensorCalib cs[3];
    for(int i=0;i<3;++i) lc_set_static(cs[i],500,520);
    // mask_out=null no debe crashear (caller que solo quiere el conteo).
    TEST_ASSERT_EQUAL_INT(3, lc_count_weak(cs,3,100,nullptr));
}

void test_count_weak_consistent_with_is_suspect(void){
    // lc_is_suspect == (lc_count_weak > 0): misma definición de "débil".
    SensorCalib cs[2];
    lc_set_static(cs[0],400,900);
    lc_set_static(cs[1],500,560);
    TEST_ASSERT_TRUE(lc_is_suspect(cs,2,100));
    TEST_ASSERT_EQUAL_INT(1, lc_count_weak(cs,2,100,nullptr));
    lc_set_static(cs[1],200,900);
    TEST_ASSERT_FALSE(lc_is_suspect(cs,2,100));
    TEST_ASSERT_EQUAL_INT(0, lc_count_weak(cs,2,100,nullptr));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_static_threshold_is_midpoint);
    RUN_TEST(test_adapt_carpet_moves_toward_reading_when_off_line);
    RUN_TEST(test_adapt_does_not_drift_when_on_line);
    RUN_TEST(test_suspect_when_margin_too_small);
    RUN_TEST(test_adapt_alpha_gt1_clamps_to_1);
    // lc_count_weak (exclusión de débiles, banco 2026-06-12)
    RUN_TEST(test_count_weak_none);
    RUN_TEST(test_count_weak_mask_marks_right_sensors);
    RUN_TEST(test_count_weak_all_weak_and_null_mask);
    RUN_TEST(test_count_weak_consistent_with_is_suspect);
    // Caracterización lazy-init / precedencia EEPROM (audit down-calib-lazyinit).
    RUN_TEST(test_fsm_boot_state_is_pending);
    RUN_TEST(test_fsm_eeprom_load_wins_and_blocks_derivation);
    RUN_TEST(test_fsm_no_eeprom_derives_on_first_send_once);
    RUN_TEST(test_fsm_carpet_step_rearms_then_next_send_rederives);
    RUN_TEST(test_fsm_white_step_derives_immediately);
    RUN_TEST(test_fsm_eeprom_then_recalib_carpet_white_full_sequence);
    RUN_TEST(test_eeprom_value_survives_over_line_ring_derivation);
    RUN_TEST(test_no_eeprom_derives_line_ring_values_on_first_use);
    return UNITY_END();
}
