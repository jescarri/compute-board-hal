// DeepSleepCycle -- ultra-low-power wake/sleep loop.
//
// On each boot the HAL brings up VCC_AUX, registers the peripheral pins this
// app uses, and checks the config button. If the button is held we stay awake
// (e.g. to run a config portal); otherwise we do our work and drop into deep
// sleep, which latches every peripheral pin into Hi-Z and holds VCC_AUX_ENA LOW
// so the secondary rail collapses.

#include <ComputeBoardHal.h>

static cbhal::ComputeBoardHal board;

static constexpr uint64_t kSleepSeconds = 60;

void setup() {
    board.begin();

    // Peripheral pins this application drives on the VCC_AUX rail.
    board.registerPins({25, 26, 27});

    if (board.isConfigAsserted()) {
        // Config button held at boot: stay awake and hand off to your portal.
        // (Left to the application; we simply return here.)
        return;
    }

    // ... do sensor reads / transmit here while VCC_AUX is powered ...

    // Sleep with the default all-domains-off RTC config. To keep RTC memory
    // instead, pass RtcPowerConfig::keepRtcMemory().
    board.deepSleepSeconds(kSleepSeconds);
}

void loop() {
    // Not reached in the sleep path; used only when the config button is held.
}
