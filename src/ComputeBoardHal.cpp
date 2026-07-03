// Compute-Board Mini HAL -- thin ESP32 driver.
//
// This is the only translation unit that touches hardware. It asks the pure
// core (SleepPlan / RtcPowerConfig) *what* to do and then performs the matching
// ESP-IDF calls. It deliberately uses the IDF gpio/sleep driver APIs rather than
// the Arduino digital API, so the same source compiles under both the Arduino
// and ESP-IDF frameworks. ESP_PLATFORM is defined by both.

#include "ComputeBoardHal.h"

#if defined(ESP_PLATFORM)

#include "driver/gpio.h"
#include "esp_sleep.h"

// neopixelWrite() (RMT-backed single-WS2812 driver) ships with the Arduino-ESP32
// core. The LED helpers use it when building under Arduino; under a bare ESP-IDF
// build they compile to no-ops (documented limitation) so the rest of the HAL
// still builds for IDF.
#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace cbhal {

namespace {

constexpr gpio_num_t asGpio(int gpio) { return static_cast<gpio_num_t>(gpio); }

esp_sleep_pd_option_t toIdfOption(PowerDomainOption option) {
    switch (option) {
        case PowerDomainOption::On:
            return ESP_PD_OPTION_ON;
        case PowerDomainOption::Auto:
            return ESP_PD_OPTION_AUTO;
        case PowerDomainOption::Off:
        default:
            return ESP_PD_OPTION_OFF;
    }
}

// Drive an output pin to a level after clearing any latched hold on it.
void driveOutput(int gpio, int level) {
    const gpio_num_t pin = asGpio(gpio);
    gpio_hold_dis(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
}

}        // namespace

void ComputeBoardHal::begin() {
    // Release holds latched by a previous deep sleep so the rail can move again.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(asGpio(kPinVccAuxEna));

    // Bring up the secondary rail (active-high; the board also pulls it up).
    driveOutput(kPinVccAuxEna, 1);
    railEnabled_ = true;

    // Config button is an input; the board provides the pull-up.
    gpio_set_direction(asGpio(kPinConfigEna), GPIO_MODE_INPUT);
}

void ComputeBoardHal::enableAuxRail() {
    driveOutput(kPinVccAuxEna, 1);
    railEnabled_ = true;
}

void ComputeBoardHal::disableAuxRail() {
    driveOutput(kPinVccAuxEna, 0);
    railEnabled_ = false;
}

void ComputeBoardHal::registerPin(int gpio) {
    if (!isValidGpio(gpio)) return;
    if (gpio == kPinVccAuxEna || gpio == kPinConfigEna) return;
    for (std::size_t i = 0; i < registeredCount_; ++i) {
        if (registered_[i] == gpio) return;
    }
    if (registeredCount_ < registered_.size()) {
        registered_[registeredCount_++] = gpio;
    }
}

void ComputeBoardHal::registerPins(std::initializer_list<int> gpios) {
    for (int gpio : gpios) registerPin(gpio);
}

bool ComputeBoardHal::isConfigAsserted() const {
    return decodeConfigLevel(gpio_get_level(asGpio(kPinConfigEna)), configActiveLow_);
}

void ComputeBoardHal::writeLed(const LedColor& color) const {
#if defined(ARDUINO)
    neopixelWrite(kPinLed, color.r, color.g, color.b);
#else
    (void)color;        // requires the Arduino core's neopixelWrite; no-op under bare IDF
#endif
}

void ComputeBoardHal::setLedColor(const LedColor& color) {
    ledColor_ = color;
    ledOn_    = true;
    writeLed(color);
}

void ComputeBoardHal::ledOn() {
    ledOn_ = true;
    writeLed(ledColor_);
}

void ComputeBoardHal::ledOff() {
    ledOn_ = false;
    writeLed(colors::Off);
}

void ComputeBoardHal::applyRtcPowerConfig(const RtcPowerConfig& cfg) const {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, toIdfOption(cfg.rtc_periph));
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, toIdfOption(cfg.rtc_slow_mem));
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, toIdfOption(cfg.rtc_fast_mem));
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, toIdfOption(cfg.xtal));
}

void ComputeBoardHal::deepSleepMicros(std::uint64_t us, const RtcPowerConfig& cfg) {
    const SleepPlan plan    = buildPlan();
    const PinOp* ops        = plan.ops();
    const std::size_t count = plan.opCount();

    // 1. Put peripheral I/O into Hi-Z (latched) and reset the always-on pins.
    for (std::size_t i = 0; i < count; ++i) {
        const gpio_num_t pin = asGpio(ops[i].gpio);
        if (ops[i].action == PinAction::HiZHold) {
            gpio_set_direction(pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode(pin, GPIO_FLOATING);
            gpio_pullup_dis(pin);
            gpio_pulldown_dis(pin);
            gpio_hold_en(pin);        // latch high-Z through sleep
        } else {
            gpio_reset_pin(pin);
        }
    }

    // 2. Apply the RTC power-domain configuration (default: all domains OFF).
    applyRtcPowerConfig(cfg);

    // 3. Collapse the secondary rail and latch it LOW for the whole sleep.
    driveOutput(kPinVccAuxEna, 0);
    railEnabled_ = false;
    gpio_hold_en(asGpio(kPinVccAuxEna));
    gpio_deep_sleep_hold_en();

    // 4. Arm the timer wake-up and sleep. Does not return.
    esp_sleep_enable_timer_wakeup(us);
    esp_deep_sleep_start();
}

}        // namespace cbhal

#endif        // ESP_PLATFORM
