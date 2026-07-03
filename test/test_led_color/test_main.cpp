// Native unit tests for the LedColor value type. Pure, no hardware.

#include <unity.h>

#include "led_color.h"

using namespace cbhal;

void setUp(void) {}
void tearDown(void) {}

void test_named_colors_have_expected_channels(void) {
    TEST_ASSERT_EQUAL_UINT8(255, colors::Red.r);
    TEST_ASSERT_EQUAL_UINT8(0, colors::Red.g);
    TEST_ASSERT_EQUAL_UINT8(0, colors::Red.b);

    TEST_ASSERT_EQUAL_UINT8(0, colors::Off.r);
    TEST_ASSERT_EQUAL_UINT8(0, colors::Off.g);
    TEST_ASSERT_EQUAL_UINT8(0, colors::Off.b);

    TEST_ASSERT_EQUAL_UINT8(255, colors::White.r);
    TEST_ASSERT_EQUAL_UINT8(255, colors::White.g);
    TEST_ASSERT_EQUAL_UINT8(255, colors::White.b);
}

void test_equality_operators(void) {
    TEST_ASSERT_TRUE((colors::Red == LedColor{255, 0, 0}));
    TEST_ASSERT_TRUE(colors::Red != colors::Green);
    TEST_ASSERT_FALSE((colors::Off != LedColor{0, 0, 0}));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_named_colors_have_expected_channels);
    RUN_TEST(test_equality_operators);
    return UNITY_END();
}
