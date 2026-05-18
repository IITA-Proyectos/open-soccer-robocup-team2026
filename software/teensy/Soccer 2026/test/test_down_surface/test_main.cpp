// test_down_surface — pio test -e test_native -f test_down_surface
#include <unity.h>
#include "surface_monitor.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_lifted_requires_debounce(void){
    SurfaceMonitor s{};
    uint16_t filt[8]={10,10,10,10,10,10,10,10};
    uint16_t carp[8]={200,200,200,200,200,200,200,200};
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,0,  100,7,50));
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,50, 100,7,50));
    TEST_ASSERT_TRUE (sm_update(s,filt,carp,8,120,100,7,50));
}

void test_not_lifted_when_on_floor(void){
    SurfaceMonitor s{};
    uint16_t filt[8]={210,205,200,198,202,207,201,203};
    uint16_t carp[8]={200,200,200,200,200,200,200,200};
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,0,100,7,50));
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,500,100,7,50));
}

void test_data_valid_gate(void){
    TEST_ASSERT_EQUAL_UINT8(0, sm_data_valid(true,false));
    TEST_ASSERT_EQUAL_UINT8(0, sm_data_valid(false,true));
    TEST_ASSERT_EQUAL_UINT8(1, sm_data_valid(false,false));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_lifted_requires_debounce);
    RUN_TEST(test_not_lifted_when_on_floor);
    RUN_TEST(test_data_valid_gate);
    return UNITY_END();
}
