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

// The LED helpers drive the on-board WS2812 with FastLED when building under
// Arduino -- the same, robust RMT path proven on the sibling lora-sensor board.
// (The Arduino core's neopixelWrite was tried first but its RMT path stalled
// after a few writes on this hardware.) Under a bare ESP-IDF build the LED helpers
// compile to no-ops so the rest of the HAL still builds for IDF.
#if defined(ARDUINO)
#include <Arduino.h>
#include <FastLED.h>
#endif

// Opt-in serial tracing. Build with -D CBHAL_DEBUG to have the driver print what
// it does over Serial (which the application must have begun). No-op otherwise.
#if defined(ARDUINO) && defined(CBHAL_DEBUG)
#define CBHAL_LOG(...) Serial.printf("[cbhal] " __VA_ARGS__)
#else
#define CBHAL_LOG(...) ((void)0)
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
//
// Under Arduino we use pinMode/digitalWrite. This is the path proven on this
// board (and its sibling lora-sensor) for GPIO13/VCC_AUX_ENA, which is an
// RTC-capable pad: bare gpio_set_direction/gpio_set_level does not reliably take
// such a pad high, whereas Arduino's pinMode does the extra pad-mux/reset work.
// Bare ESP-IDF builds fall back to the gpio driver (with gpio_reset_pin to
// detach any prior routing).
void driveOutput(int gpio, int level) {
    const gpio_num_t pin = asGpio(gpio);
    gpio_hold_dis(pin);
#if defined(ARDUINO)
    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, level ? HIGH : LOW);
#else
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
#endif
}

// Configure a pin as a plain input.
void configureInput(int gpio) {
#if defined(ARDUINO)
    pinMode(gpio, INPUT);
#else
    gpio_set_direction(asGpio(gpio), GPIO_MODE_INPUT);
#endif
}

// Read a pin's current logic level (0/1).
int readLevel(int gpio) {
#if defined(ARDUINO)
    return digitalRead(gpio);
#else
    return gpio_get_level(asGpio(gpio));
#endif
}

}        // namespace

void ComputeBoardHal::begin() {
    // Release holds latched by a previous deep sleep so the rail can move again.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(asGpio(kPinVccAuxEna));

    // Bring up the secondary rail (active-high; the board also pulls it up).
    driveOutput(kPinVccAuxEna, 1);
    gpio_hold_dis(asGpio(kPinVccAuxEna));        // ensure the pad is free to move
    railEnabled_ = true;

    // Config button is an input; the board provides the pull-up.
    configureInput(kPinConfigEna);
    CBHAL_LOG("begin: VCC_AUX_ENA(GPIO%d)=%d config(GPIO%d)=%d\n", kPinVccAuxEna,
              readLevel(kPinVccAuxEna), kPinConfigEna, readLevel(kPinConfigEna));
}

void ComputeBoardHal::enableAuxRail() {
    driveOutput(kPinVccAuxEna, 1);
    railEnabled_ = true;
    CBHAL_LOG("enableAuxRail: GPIO%d=%d\n", kPinVccAuxEna, readLevel(kPinVccAuxEna));
}

void ComputeBoardHal::disableAuxRail() {
    driveOutput(kPinVccAuxEna, 0);
    railEnabled_ = false;
    CBHAL_LOG("disableAuxRail: GPIO%d=%d\n", kPinVccAuxEna, readLevel(kPinVccAuxEna));
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
    return decodeConfigLevel(readLevel(kPinConfigEna), configActiveLow_);
}

void ComputeBoardHal::writeLed(const LedColor& color) const {
    CBHAL_LOG("led GPIO%d <- (%u,%u,%u) rail=%d\n", kPinLed, color.r, color.g, color.b,
              railEnabled_);
#if defined(ARDUINO)
    static CRGB leds[1];
    static bool ledsInit = false;
    if (!ledsInit) {
        FastLED.addLeds<WS2812B, kPinLed, GRB>(leds, 1);
        ledsInit = true;
    }
    leds[0] = CRGB(color.r, color.g, color.b);
    FastLED.show();
#else
    (void)color;        // WS2812 driving needs FastLED (Arduino); no-op under bare IDF
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

    CBHAL_LOG("deepSleep: %llu us, %u pin ops, rail->LOW+hold\n", us,
              static_cast<unsigned>(count));
#if defined(ARDUINO) && defined(CBHAL_DEBUG)
    Serial.flush();        // make sure the trace is out before the pads/UART die
#endif

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
