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

// Boot / download strapping pin (GPIO0, "ESP_PROG"). On this board it has NO
// external pull-up (only the boot button to GND), so it relies on the ESP32's
// internal pull-up to strap boot-from-flash on every reset -- including the
// deep-sleep wake reset. It must therefore never be forced Hi-Z or isolated at
// sleep (that would let it float low -> download mode); it is gpio_reset_pin'd
// (keeps the pull-up) and left un-held.
constexpr int kPinBoot = 0;

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

// True for the RTC-capable (RTC_GPIO) pads on the ESP32. These retain their pull
// / hold configuration into deep sleep, so at sleep they are fully disconnected
// with rtc_gpio_isolate() (belt-and-suspenders against internal-pull leakage)
// and released with rtc_gpio_hold_dis() on wake. The full set is
// {0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39}.
constexpr bool isRtcGpio(int gpio) {
    switch (gpio) {
        case 0:
        case 2:
        case 4:
        case 12:
        case 13:
        case 14:
        case 15:
        case 25:
        case 26:
        case 27:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
            return true;
        default:
            return false;
    }
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
