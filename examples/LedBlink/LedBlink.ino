// LedBlink -- blink the on-board LED (LED1 / GPIO15) and test the config button.
//
// The LED is a plain on/off LED on the VCC_AUX rail; begin() brings the rail up.
// The loop blinks it once per second. Each iteration also checks CONFIG_ENA
// (GPIO12): when the button is pressed (pulled to GND) it prints "CONF_PRESSED",
// waits 10 s, then reboots.

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

// Drive the LED for `ms`, polling the config button throughout. Returns true if
// the button was seen pressed during the interval.
static bool blinkAndPoll(bool on, uint32_t ms) {
    board.setLed(on);
    const uint32_t start = millis();
    while (millis() - start < ms) {
        if (board.isConfigAsserted()) return true;
        delay(10);
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\r\n[LedBlink] boot -- reset reason: %s\r\n", resetReasonStr(esp_reset_reason()));

    board.begin();
    Serial.printf("[LedBlink] auxRailEnabled=%d  configAsserted=%d\r\n", board.auxRailEnabled(),
                  board.isConfigAsserted());
}

void loop() {
    // One blink cycle (on 500 ms, off 500 ms), polling the config button throughout.
    if (blinkAndPoll(true, 500) || blinkAndPoll(false, 500)) {
        Serial.println("CONF_PRESSED\r");
        Serial.flush();
        delay(10000);         // wait 10 s
        ESP.restart();        // reboot
    }
}
