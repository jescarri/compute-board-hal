// Compute-Board Mini HAL -- public API.
//
// A small hardware abstraction layer for the ESP32-WROOM-32E compute-board:
// secondary-rail (VCC_AUX) control, config-button probe, and an ultra-low-power
// deep-sleep routine that latches all peripheral I/O into Hi-Z and collapses the
// rail for the whole sleep.
//
// This header is hardware-free (pulls in only the pure core), so it can be
// included anywhere. The implementation in ComputeBoardHal.cpp is compiled only
// for the ESP32 target and uses ESP-IDF driver APIs, so the library works under
// both the Arduino and ESP-IDF frameworks.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "board_pins.h"
#include "led_color.h"
#include "power_plan.h"

namespace cbhal {

class ComputeBoardHal {
  public:
    // Prepare the board: release any deep-sleep pin holds left from a previous
    // sleep, enable the VCC_AUX rail, and configure the config-button pin as an
    // input. Call once from setup().
    void begin();

    // --- Secondary rail (VCC_AUX) -----------------------------------------
    void enableAuxRail();
    void disableAuxRail();
    bool auxRailEnabled() const { return railEnabled_; }

    // --- Peripheral pin registration --------------------------------------
    // Register application pins (powered from VCC_AUX) to force into Hi-Z at
    // deep sleep, in addition to the HAL's known bus pins. Invalid pins and the
    // rail/config pins are ignored.
    void registerPin(int gpio);
    void registerPins(std::initializer_list<int> gpios);
    std::size_t registeredCount() const { return registeredCount_; }

    // --- Config button -----------------------------------------------------
    // Probe CONFIG_ENA (GPIO12). Returns true when the button is asserted
    // (active-low by default).
    bool isConfigAsserted() const;
    void setConfigActiveLow(bool activeLow) { configActiveLow_ = activeLow; }

    // --- On-board addressable LED (WS2812 on GPIO15) -----------------------
    // Set the LED colour and turn it on.
    void setLedColor(const LedColor& color);
    // Turn the LED on using the last colour set (defaults to white).
    void ledOn();
    // Turn the LED off (writes colour 0,0,0). The last colour is remembered so
    // a subsequent ledOn() restores it.
    void ledOff();
    bool ledIsOn() const { return ledOn_; }
    LedColor ledColor() const { return ledColor_; }
    // LED brightness (0-255). Defaults to a low value because the LED runs on the
    // 3.3 V VCC_AUX rail (below the WS2812's rated VDD); full brightness draws
    // enough current to droop that rail and corrupt the data decode. Raise it if
    // your supply can hold up.
    void setLedBrightness(std::uint8_t brightness);
    std::uint8_t ledBrightness() const { return ledBrightness_; }

    // --- RTC power domains -------------------------------------------------
    // Persistent override for the RTC power-domain configuration used by deep
    // sleep. Defaults to RtcPowerConfig::maxSavings() (all domains OFF).
    void setRtcPowerConfig(const RtcPowerConfig& cfg) { rtcCfg_ = cfg; }
    const RtcPowerConfig& rtcPowerConfig() const { return rtcCfg_; }

    // --- Deep sleep --------------------------------------------------------
    // Force every registered/bus pin into latched Hi-Z, reset the always-on
    // pins, apply the RTC power config, drive VCC_AUX LOW and hold it, then
    // enter timer-wake deep sleep. Does not return.
    [[noreturn]] void deepSleepMicros(std::uint64_t us, const RtcPowerConfig& cfg);
    [[noreturn]] void deepSleepMicros(std::uint64_t us) { deepSleepMicros(us, rtcCfg_); }
    [[noreturn]] void deepSleepSeconds(std::uint64_t s, const RtcPowerConfig& cfg) {
        deepSleepMicros(s * 1000000ULL, cfg);
    }
    [[noreturn]] void deepSleepSeconds(std::uint64_t s) { deepSleepMicros(s * 1000000ULL, rtcCfg_); }

    // Build the deep-sleep plan without sleeping. Exposed for inspection/testing.
    SleepPlan buildPlan() const { return SleepPlan::build(registered_.data(), registeredCount_); }

  private:
    void applyRtcPowerConfig(const RtcPowerConfig& cfg) const;
    void writeLed(const LedColor& color) const;

    std::array<int, kMaxPlanPins> registered_{};
    std::size_t registeredCount_ = 0;
    RtcPowerConfig rtcCfg_       = RtcPowerConfig::maxSavings();
    LedColor ledColor_           = colors::White;
    std::uint8_t ledBrightness_  = 50;        // ~20%; low for the 3.3 V rail
    bool railEnabled_            = false;
    bool ledOn_                  = false;
    bool configActiveLow_        = true;
};

}        // namespace cbhal
