// Authoritative pin map for the compute-board (ESP32-WROOM-32E baseboard).
//
// Every value here was traced from the net-to-pin connectivity in
// `compute-board.kicad_sch`, not guessed. This header is intentionally
// hardware-free: it includes no <Arduino.h> and no driver headers, so it
// compiles and runs on a desktop host for unit testing.

#pragma once

#include <array>
#include <cstddef>

namespace cbhal {

// --- Control pins -----------------------------------------------------------
// Secondary rail (VCC_AUX) LDO enable. Active-high, board pull-up -> rail is ON
// by default at boot. Deep sleep must drive this LOW and hold it.
constexpr int kPinVccAuxEna = 13;
// Config button (SW2). Active-low (button to GND, board pull-up + RC debounce).
// Note: also the MTDI flash-voltage strapping pin.
constexpr int kPinConfigEna = 12;

// --- On-board peripherals ---------------------------------------------------
constexpr int kPinLed = 15;        // LED1: on-board on/off LED (active-high).

// I2C
constexpr int kPinSda = 21;
constexpr int kPinScl = 22;

// SPI / microSD
constexpr int kPinSdCs   = 5;
constexpr int kPinSdSck  = 18;
constexpr int kPinSdMiso = 19;
constexpr int kPinSdMosi = 23;

// UART0
constexpr int kPinUart0Rx = 3;
constexpr int kPinUart0Tx = 1;

// --- GPIO classification ----------------------------------------------------
constexpr int kGpioMin = 0;
constexpr int kGpioMax = 39;

// True for a GPIO number the ESP32 actually has.
constexpr bool isValidGpio(int gpio) {
    return gpio >= kGpioMin && gpio <= kGpioMax;
}

// GPIO34..39 are input-only on the ESP32 (no output drivers, no pulls).
constexpr bool isInputOnly(int gpio) {
    return gpio >= 34 && gpio <= 39;
}

// Bus pins that live on the VCC_AUX rail and should be forced into Hi-Z at
// deep sleep by default, on top of any pins the application registers.
constexpr std::array<int, 6> kDefaultAuxHiZPins = {
    kPinSda,
    kPinScl,
    kPinSdCs,
    kPinSdSck,
    kPinSdMiso,
    kPinSdMosi,
};

// Strapping / always-on pins that get gpio_reset_pin() at sleep rather than a
// latched Hi-Z. Deliberately excludes the rail (13), config (12) and LED (15)
// pins, which all have dedicated drive-LOW + hold handling. In particular the
// LED pin must NOT be reset here: gpio_reset_pin() enables the internal pull-up,
// which would source ~30 uA into the active-high LED and faintly light it during
// deep sleep.
constexpr std::array<int, 8> kAlwaysOnResetPins = {
    0,
    2,
    34,
    35,
    36,
    37,
    38,
    39,
};

}        // namespace cbhal
