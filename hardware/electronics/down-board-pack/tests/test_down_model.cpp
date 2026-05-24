// test_down_model — pio test -e test_native -f test_down_model
#include <unity.h>
#include "down_model.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static void mkcfg(DownModelCfg& c){
    c.imminent_depth=6; c.adapt_alpha=0.02f; c.calib_min_margin=120;
    c.lifted_debounce_ms=100; c.lifted_min_sensors=7; c.lifted_delta_below=80;
    c.line_end_min_track_ms=200;
}

void test_no_line_carpet_valid(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={205,198,202,201,199,203,200,204};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(2, s.schema_version);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
}

void test_line_front_sets_fields(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={850,830,210,200,205,202,198,206};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_TRUE(s.sensors_on_line >= 1);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.line_angle_centideg);
}

void test_lifted_sets_invalid_and_flag(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={5,5,5,5,5,5,5,5};
    dm_update(m,cfg,raw,8,0);
    LineStatusV2 s = dm_update(m,cfg,raw,8,200);
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);
    TEST_ASSERT_TRUE(s.event_flags & EV_LIFTED);
}

void test_calib_suspect_sets_invalid_and_flag(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    // margen blanco-carpet = 50 < calib_min_margin=120 => suspect
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],500,550);
    uint16_t raw[8]={505,508,502,501,499,503,506,504};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);
    TEST_ASSERT_TRUE(s.event_flags & EV_CALIB_SUSPECT);
}

void test_n_over_max_is_clamped(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<DM_MAX_SENSORS;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[DM_MAX_SENSORS]; for(int i=0;i<DM_MAX_SENSORS;++i) raw[i]=200;
    LineStatusV2 s = dm_update(m,cfg,raw,64,1000);   // n=64 > 32 => clamp
    TEST_ASSERT_TRUE(s.sensors_on_line <= DM_MAX_SENSORS);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_no_line_carpet_valid);
    RUN_TEST(test_line_front_sets_fields);
    RUN_TEST(test_lifted_sets_invalid_and_flag);
    RUN_TEST(test_calib_suspect_sets_invalid_and_flag);
    RUN_TEST(test_n_over_max_is_clamped);
    return UNITY_END();
}
