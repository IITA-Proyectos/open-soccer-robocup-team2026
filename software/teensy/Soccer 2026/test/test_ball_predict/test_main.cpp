// test_ball_predict — módulo PURO ball_predict (anticipación del arquero).
//
// Casos: pelota quieta (v=0 → px=x, py=y = fallback/conducta actual),
// moviéndose +x (px>x), -x (px<x), clamp a max_lead, escala con lookahead.

#include "unity.h"
#include "ball_predict.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// v=0 (quieta o N/A) → la predicción es la posición actual (fallback automático).
static void test_stationary_no_lead(void) {
    BallPredictParams p = ball_predict_default_params();
    BallPredictOut out = ball_predict(120, -80, 0, 0, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, out.px_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -80.0f, out.py_mm);
}

// Moviéndose en +x → la X predicha adelanta (px > x).
static void test_moving_positive_x_leads_forward(void) {
    BallPredictParams p = ball_predict_default_params();  // lookahead=0.2, max=400
    // lead_x = 500 * 0.2 = 100 mm
    BallPredictOut out = ball_predict(0, 0, 500, 0, p);
    TEST_ASSERT_TRUE(out.px_mm > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, out.px_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out.py_mm);
}

// Moviéndose en -x → la X predicha retrocede (px < x).
static void test_moving_negative_x_leads_back(void) {
    BallPredictParams p = ball_predict_default_params();
    // lead_x = -500 * 0.2 = -100 mm
    BallPredictOut out = ball_predict(0, 0, -500, 0, p);
    TEST_ASSERT_TRUE(out.px_mm < 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, out.px_mm);
}

// El lead en Y también responde (eje independiente).
static void test_moving_positive_y_leads_forward(void) {
    BallPredictParams p = ball_predict_default_params();
    // lead_y = 1000 * 0.2 = 200 mm
    BallPredictOut out = ball_predict(50, 50, 0, 1000, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, out.px_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 250.0f, out.py_mm);
}

// Clamp a +max_lead: velocidad muy alta no proyecta más allá del tope.
static void test_clamps_to_max_lead_positive(void) {
    BallPredictParams p = ball_predict_default_params();  // max=400
    // lead crudo = 5000 * 0.2 = 1000 mm → clamp a 400 mm
    BallPredictOut out = ball_predict(0, 0, 5000, 0, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 400.0f, out.px_mm);
}

// Clamp a -max_lead (lado negativo).
static void test_clamps_to_max_lead_negative(void) {
    BallPredictParams p = ball_predict_default_params();
    // lead crudo = -5000 * 0.2 = -1000 mm → clamp a -400 mm
    BallPredictOut out = ball_predict(0, 0, -5000, 0, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -400.0f, out.px_mm);
}

// El lead escala linealmente con lookahead_s.
static void test_lookahead_scales_lead(void) {
    BallPredictParams p = ball_predict_default_params();
    p.lookahead_s = 0.4f;  // doble del default
    p.max_lead_mm = 10000.0f;  // sin tope para ver la escala pura
    // lead_x = 500 * 0.4 = 200 mm
    BallPredictOut out = ball_predict(0, 0, 500, 0, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, out.px_mm);
}

// El lead se suma al offset de la posición actual (no la pisa).
static void test_lead_adds_to_position(void) {
    BallPredictParams p = ball_predict_default_params();
    // pos=(300,-200), lead_x = 500*0.2 = 100 → px=400; lead_y = -250*0.2 = -50 → py=-250
    BallPredictOut out = ball_predict(300, -200, 500, -250, p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 400.0f, out.px_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -250.0f, out.py_mm);
}

// Parámetros por defecto: lookahead_s=0.2, max_lead_mm=400.
static void test_default_params(void) {
    BallPredictParams p = ball_predict_default_params();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, p.lookahead_s);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 400.0f, p.max_lead_mm);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stationary_no_lead);
    RUN_TEST(test_moving_positive_x_leads_forward);
    RUN_TEST(test_moving_negative_x_leads_back);
    RUN_TEST(test_moving_positive_y_leads_forward);
    RUN_TEST(test_clamps_to_max_lead_positive);
    RUN_TEST(test_clamps_to_max_lead_negative);
    RUN_TEST(test_lookahead_scales_lead);
    RUN_TEST(test_lead_adds_to_position);
    RUN_TEST(test_default_params);
    return UNITY_END();
}
