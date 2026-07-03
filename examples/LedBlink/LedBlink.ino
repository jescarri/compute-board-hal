// LedBlink -- blink the on-board WS2812 red once per second, with serial debug.
//
// begin() brings up VCC_AUX (the LED is on the secondary rail); we set the
// colour once, then toggle the LED on/off with a 500 ms half-period so it blinks
// once every second. Serial output at 115200 reports the boot/reset reason, the
// rail-enable pin state, and each LED toggle so you can see what the board does.
//
// If the reset reason shows RTCWDT_RTC_RESET / BROWNOUT, or the console restarts
// mid-run, the board is rebooting -- most likely the GPIO12 (CONFIG_ENA / MTDI)
// flash-voltage strapping issue, which also makes the LED blink only "sometimes".

#include <ComputeBoardHal.h>

#include "esp_system.h"

static cbhal::ComputeBoardHal board;

static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:
            return "POWERON";
        case ESP_RST_EXT:
            return "EXT";
        case ESP_RST_SW:
            return "SW";
        case ESP_RST_PANIC:
            return "PANIC";
        case ESP_RST_INT_WDT:
            return "INT_WDT";
        case ESP_RST_TASK_WDT:
            return "TASK_WDT";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_DEEPSLEEP:
            return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT";
        default:
            return "UNKNOWN/OTHER";
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.printf("[LedBlink] boot -- reset reason: %s\n", resetReasonStr(esp_reset_reason()));

    board.begin();
    delay(100);        // let VCC_AUX settle before the first LED write

    // GPIO13 is an output now; digitalRead reports the driven level. It should
    // read 1 (rail enabled). If it reads 0, the rail is not powered.
    Serial.printf("[LedBlink] auxRailEnabled=%d  GPIO%d(level)=%d  configAsserted=%d\n",
                  board.auxRailEnabled(), cbhal::kPinVccAuxEna,
                  digitalRead(cbhal::kPinVccAuxEna), board.isConfigAsserted());

    board.setLedColor(cbhal::colors::Red);
    Serial.println("[LedBlink] LED set to red");
}

void loop() {
    static uint32_t n = 0;
    board.ledOn();
    Serial.printf("[LedBlink] #%lu ON   rail=%d GPIO%d=%d\n", static_cast<unsigned long>(n),
                  board.auxRailEnabled(), cbhal::kPinVccAuxEna,
                  digitalRead(cbhal::kPinVccAuxEna));
    delay(500);
    board.ledOff();
    Serial.printf("[LedBlink] #%lu OFF\n", static_cast<unsigned long>(n));
    delay(500);
    ++n;
}
