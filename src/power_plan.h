// Pure, hardware-free deep-sleep policy for the compute-board Mini HAL.
//
// This translation unit contains all the *decision making* for low-power
// operation -- which pins go Hi-Z, in what order, how the config button is
// decoded, and how the RTC power domains are configured -- expressed as plain
// data. It includes no hardware headers so it can be exercised on a desktop
// host with real production code and no mocks. The thin ESP32 driver
// (`ComputeBoardHal.cpp`) replays the plan against the actual gpio_* / esp_sleep_*
// APIs.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "board_pins.h"

namespace cbhal {

// Decode a raw pin level (0 = LOW, non-zero = HIGH) into "is the config button
// asserted?". Active-low is the compute-board default (SW2 to GND, pull-up).
constexpr bool decodeConfigLevel(int level, bool activeLow) {
    return activeLow ? (level == 0) : (level != 0);
}

// --- RTC power-domain configuration ----------------------------------------
// Mirror of ESP-IDF's ESP_PD_OPTION_* kept in the pure layer so that the IDF
// enum (and esp_sleep.h) never leak into host-testable code.
enum class PowerDomainOption : std::uint8_t { Off,
                                              On,
                                              Auto };

// Config for the four domains the deep-sleep path touches. Every field defaults
// to Off -- byte-for-byte the maximum-savings behaviour. An explicit constexpr
// constructor (rather than default member initializers) keeps the type
// brace-initialisable under C++14 as well as C++17+.
struct RtcPowerConfig {
    PowerDomainOption rtc_periph;
    PowerDomainOption rtc_slow_mem;
    PowerDomainOption rtc_fast_mem;
    PowerDomainOption xtal;

    constexpr RtcPowerConfig(PowerDomainOption periph  = PowerDomainOption::Off,
                             PowerDomainOption slowMem = PowerDomainOption::Off,
                             PowerDomainOption fastMem = PowerDomainOption::Off,
                             PowerDomainOption crystal = PowerDomainOption::Off)
        : rtc_periph(periph),
          rtc_slow_mem(slowMem),
          rtc_fast_mem(fastMem),
          xtal(crystal) {}

    // All domains powered down (the default, lowest sleep current).
    static constexpr RtcPowerConfig maxSavings() { return RtcPowerConfig{}; }

    // Retain RTC slow/fast memory (e.g. for RTC-retained variables or a wake
    // stub) while still powering down peripherals and the crystal.
    static constexpr RtcPowerConfig keepRtcMemory() {
        return RtcPowerConfig{PowerDomainOption::Off, PowerDomainOption::On,
                              PowerDomainOption::On, PowerDomainOption::Off};
    }
};

// --- Deep-sleep pin plan ----------------------------------------------------
enum class PinAction : std::uint8_t {
    HiZHold,        // set input + float + disable pulls + latch through sleep
    Reset           // gpio_reset_pin (used for strapping / always-on pins)
};

struct PinOp {
    int gpio;
    PinAction action;
};

// Upper bound on plan size (ESP32 has 40 GPIO numbers). Keeps the plan on the
// stack -- no dynamic allocation.
constexpr std::size_t kMaxPlanPins = 40;

// An ordered, deduplicated set of pin operations to apply before deep sleep.
// The rail-enable, config and LED pins are never part of the ops: each is
// handled by the driver with a dedicated "drive LOW + hold" step, and the rail
// and LED pins are exposed here via railPin() / ledPin().
class SleepPlan {
  public:
    // Build from the board defaults plus caller-registered application pins.
    // `registered` may be null when `count` is 0. Invalid pins, the rail pin,
    // the config pin and the LED pin are silently skipped.
    static SleepPlan build(const int* registered, std::size_t count);

    const PinOp* ops() const { return ops_.data(); }
    std::size_t opCount() const { return opCount_; }

    // Rail-enable pin: always the final "drive LOW + hold" step at sleep.
    int railPin() const { return kPinVccAuxEna; }

    // LED1 drive pin (active-high): driven LOW + held at sleep so the internal
    // pull-up can't bleed current into the LED. Never appears in ops().
    int ledPin() const { return kPinLed; }

    bool contains(int gpio, PinAction action) const;
    bool hasHiZ(int gpio) const { return contains(gpio, PinAction::HiZHold); }
    bool hasReset(int gpio) const { return contains(gpio, PinAction::Reset); }

  private:
    void addUnique(int gpio, PinAction action);
    bool present(int gpio) const;

    std::array<PinOp, kMaxPlanPins> ops_{};
    std::size_t opCount_ = 0;
};

}        // namespace cbhal
