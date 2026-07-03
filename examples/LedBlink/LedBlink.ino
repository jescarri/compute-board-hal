// LedBlink -- blink the on-board WS2812 red once per second, with serial debug.
//
// begin() brings up VCC_AUX (the LED is on the secondary rail); we set the colour
// once, then toggle the LED on/off with a 500 ms half-period so it blinks once a
// second. Serial (115200) reports the boot/reset reason and rail state.
//
// Serial discipline: the UART is flushed *before* every LED write, and no serial
// is printed while the WS2812 RMT transfer is in flight, so the LED driver and
// the console never corrupt each other. Lines end in CRLF for dumb terminals.

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
    Serial.printf("\r\n[LedBlink] boot -- reset reason: %s\r\n", resetReasonStr(esp_reset_reason()));

    board.begin();
    delay(100);        // let VCC_AUX settle before the first LED write

    Serial.printf("[LedBlink] auxRailEnabled=%d  GPIO%d(level)=%d  configAsserted=%d\r\n",
                  board.auxRailEnabled(), cbhal::kPinVccAuxEna,
                  digitalRead(cbhal::kPinVccAuxEna), board.isConfigAsserted());
    Serial.flush();        // drain the UART before the first LED write

    board.setLedColor(cbhal::colors::Red);
    Serial.println("[LedBlink] LED set to red\r");
}

void loop() {
    static uint32_t n = 0;

    Serial.printf("[LedBlink] #%lu ON\r\n", static_cast<unsigned long>(n));
    Serial.flush();        // UART fully out before FastLED.show() runs
    board.ledOn();
    delay(500);        // WS2812 transfer completes here; no serial in flight

    Serial.printf("[LedBlink] #%lu OFF\r\n", static_cast<unsigned long>(n));
    Serial.flush();
    board.ledOff();
    delay(500);
    ++n;
}
