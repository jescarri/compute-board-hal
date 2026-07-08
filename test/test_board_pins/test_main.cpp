// Native unit tests for the pin map. No hardware, no mocks -- these assert on
// the pure constexpr tables in board_pins.h.

#include <unity.h>

#include "board_pins.h"

using namespace cbhal;

namespace {

template <std::size_t N>
bool inSet(const std::array<int, N>& set, int gpio) {
    for (int p : set) {
        if (p == gpio) return true;
    }
    return false;
}

}        // namespace

void setUp(void) {}
void tearDown(void) {}

void test_control_pins_match_schematic(void) {
    TEST_ASSERT_EQUAL_INT(13, kPinVccAuxEna);
    TEST_ASSERT_EQUAL_INT(12, kPinConfigEna);
}

void test_rail_and_config_never_in_pin_sets(void) {
    TEST_ASSERT_FALSE(inSet(kDefaultAuxHiZPins, kPinVccAuxEna));
    TEST_ASSERT_FALSE(inSet(kDefaultAuxHiZPins, kPinConfigEna));
    TEST_ASSERT_FALSE(inSet(kAlwaysOnResetPins, kPinVccAuxEna));
    TEST_ASSERT_FALSE(inSet(kAlwaysOnResetPins, kPinConfigEna));
}

void test_no_pin_in_both_sets(void) {
    for (int p : kDefaultAuxHiZPins) {
        TEST_ASSERT_FALSE(inSet(kAlwaysOnResetPins, p));
    }
}

void test_all_pins_are_valid_gpios(void) {
    for (int p : kDefaultAuxHiZPins) TEST_ASSERT_TRUE(isValidGpio(p));
    for (int p : kAlwaysOnResetPins) TEST_ASSERT_TRUE(isValidGpio(p));
    TEST_ASSERT_TRUE(isValidGpio(kPinVccAuxEna));
    TEST_ASSERT_TRUE(isValidGpio(kPinConfigEna));
}

void test_input_only_pins_are_not_driven(void) {
    // Input-only pins (34..39) have no output drivers; they must never appear in
    // the default Hi-Z (drive-to-input + hold) set.
    for (int p : kDefaultAuxHiZPins) {
        TEST_ASSERT_FALSE(isInputOnly(p));
    }
    TEST_ASSERT_TRUE(isInputOnly(34));
    TEST_ASSERT_TRUE(isInputOnly(39));
    TEST_ASSERT_FALSE(isInputOnly(33));
    TEST_ASSERT_FALSE(isInputOnly(kPinVccAuxEna));
}

void test_rtc_gpio_membership(void) {
    // A representative slice of the ESP32 RTC-capable set.
    for (int p : {0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 39}) {
        TEST_ASSERT_TRUE(isRtcGpio(p));
    }
    // Non-RTC digital pins (incl. the I2C/SPI bus pins) must be excluded so the
    // driver keeps using the digital Hi-Z + hold path for them.
    for (int p : {1, 3, 5, 16, 17, 18, 19, 21, 22, 23}) {
        TEST_ASSERT_FALSE(isRtcGpio(p));
    }
    // The boot pin is RTC-capable but is handled via reset, never isolated.
    TEST_ASSERT_TRUE(isRtcGpio(kPinBoot));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_control_pins_match_schematic);
    RUN_TEST(test_rail_and_config_never_in_pin_sets);
    RUN_TEST(test_no_pin_in_both_sets);
    RUN_TEST(test_all_pins_are_valid_gpios);
    RUN_TEST(test_input_only_pins_are_not_driven);
    RUN_TEST(test_rtc_gpio_membership);
    return UNITY_END();
}
