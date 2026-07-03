// Native unit tests for the deep-sleep policy. No hardware, no mocks -- these
// exercise the real SleepPlan / decode / RtcPowerConfig production code.

#include <unity.h>

#include "board_pins.h"
#include "power_plan.h"

using namespace cbhal;

void setUp(void) {}
void tearDown(void) {}

// --- Config-button decode ---------------------------------------------------

void test_decode_active_low(void) {
    TEST_ASSERT_TRUE(decodeConfigLevel(0, true));         // LOW -> pressed
    TEST_ASSERT_FALSE(decodeConfigLevel(1, true));        // HIGH -> released
}

void test_decode_active_high(void) {
    TEST_ASSERT_FALSE(decodeConfigLevel(0, false));
    TEST_ASSERT_TRUE(decodeConfigLevel(1, false));
}

// --- SleepPlan --------------------------------------------------------------

void test_plan_includes_default_bus_pins_as_hiz(void) {
    SleepPlan plan = SleepPlan::build(nullptr, 0);
    for (int p : kDefaultAuxHiZPins) {
        TEST_ASSERT_TRUE(plan.hasHiZ(p));
    }
}

void test_plan_includes_always_on_pins_as_reset(void) {
    SleepPlan plan = SleepPlan::build(nullptr, 0);
    for (int p : kAlwaysOnResetPins) {
        TEST_ASSERT_TRUE(plan.hasReset(p));
    }
}

void test_plan_excludes_rail_and_config(void) {
    const int regs[] = {kPinVccAuxEna, kPinConfigEna};
    SleepPlan plan   = SleepPlan::build(regs, 2);
    TEST_ASSERT_FALSE(plan.hasHiZ(kPinVccAuxEna));
    TEST_ASSERT_FALSE(plan.hasReset(kPinVccAuxEna));
    TEST_ASSERT_FALSE(plan.hasHiZ(kPinConfigEna));
    TEST_ASSERT_FALSE(plan.hasReset(kPinConfigEna));
    // Rail pin is still reported for the final drive-LOW step.
    TEST_ASSERT_EQUAL_INT(kPinVccAuxEna, plan.railPin());
}

void test_plan_registers_user_pin_once(void) {
    const int regs[] = {25, 25, 26};        // duplicate 25
    SleepPlan plan   = SleepPlan::build(regs, 3);
    TEST_ASSERT_TRUE(plan.hasHiZ(25));
    TEST_ASSERT_TRUE(plan.hasHiZ(26));

    std::size_t count25 = 0;
    for (std::size_t i = 0; i < plan.opCount(); ++i) {
        if (plan.ops()[i].gpio == 25) ++count25;
    }
    TEST_ASSERT_EQUAL_UINT(1, count25);
}

void test_plan_skips_invalid_pins(void) {
    const int regs[] = {99, -1, 40};
    SleepPlan plan   = SleepPlan::build(regs, 3);
    TEST_ASSERT_FALSE(plan.hasHiZ(99));
    TEST_ASSERT_FALSE(plan.hasHiZ(40));
}

void test_plan_hiz_wins_over_reset_on_overlap(void) {
    // GPIO2 is in the always-on reset set. Registering it should promote it to
    // Hi-Z and it must not also appear as a Reset op.
    const int regs[] = {2};
    SleepPlan plan   = SleepPlan::build(regs, 1);
    TEST_ASSERT_TRUE(plan.hasHiZ(2));
    TEST_ASSERT_FALSE(plan.hasReset(2));
}

void test_plan_never_exceeds_capacity(void) {
    SleepPlan plan = SleepPlan::build(nullptr, 0);
    TEST_ASSERT_TRUE(plan.opCount() <= kMaxPlanPins);
}

// --- RtcPowerConfig ---------------------------------------------------------

void test_rtc_default_is_all_off(void) {
    RtcPowerConfig cfg;        // aggregate default
    TEST_ASSERT_TRUE(cfg.rtc_periph == PowerDomainOption::Off);
    TEST_ASSERT_TRUE(cfg.rtc_slow_mem == PowerDomainOption::Off);
    TEST_ASSERT_TRUE(cfg.rtc_fast_mem == PowerDomainOption::Off);
    TEST_ASSERT_TRUE(cfg.xtal == PowerDomainOption::Off);
}

void test_rtc_max_savings_matches_default(void) {
    RtcPowerConfig cfg = RtcPowerConfig::maxSavings();
    TEST_ASSERT_TRUE(cfg.rtc_periph == PowerDomainOption::Off);
    TEST_ASSERT_TRUE(cfg.xtal == PowerDomainOption::Off);
}

void test_rtc_keep_memory_retains_slow_fast(void) {
    RtcPowerConfig cfg = RtcPowerConfig::keepRtcMemory();
    TEST_ASSERT_TRUE(cfg.rtc_slow_mem == PowerDomainOption::On);
    TEST_ASSERT_TRUE(cfg.rtc_fast_mem == PowerDomainOption::On);
    TEST_ASSERT_TRUE(cfg.rtc_periph == PowerDomainOption::Off);
    TEST_ASSERT_TRUE(cfg.xtal == PowerDomainOption::Off);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_active_low);
    RUN_TEST(test_decode_active_high);
    RUN_TEST(test_plan_includes_default_bus_pins_as_hiz);
    RUN_TEST(test_plan_includes_always_on_pins_as_reset);
    RUN_TEST(test_plan_excludes_rail_and_config);
    RUN_TEST(test_plan_registers_user_pin_once);
    RUN_TEST(test_plan_skips_invalid_pins);
    RUN_TEST(test_plan_hiz_wins_over_reset_on_overlap);
    RUN_TEST(test_plan_never_exceeds_capacity);
    RUN_TEST(test_rtc_default_is_all_off);
    RUN_TEST(test_rtc_max_savings_matches_default);
    RUN_TEST(test_rtc_keep_memory_retains_slow_fast);
    return UNITY_END();
}
