// LedBlink -- blink the on-board addressable (WS2812) LED red, once per second.
//
// begin() brings up VCC_AUX (the LED is on the secondary rail); we set the
// colour once, then toggle the LED on/off with a 500 ms half-period so it blinks
// once every second.

#include <ComputeBoardHal.h>

static cbhal::ComputeBoardHal board;

void setup() {
    board.begin();
    delay(100);
    board.setLedColor(cbhal::colors::Red);        // red, and turns the LED on
}

void loop() {
    board.ledOn();
    delay(500);
    board.ledOff();
    delay(500);
}
