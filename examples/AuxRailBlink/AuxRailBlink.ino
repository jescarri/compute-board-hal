// AuxRailBlink -- toggle the secondary VCC_AUX rail on and off.
//
// Demonstrates the minimal HAL lifecycle: begin() brings the rail up; then we
// pulse it so you can measure VCC_AUX switching on a multimeter/scope.

#include <ComputeBoardHal.h>

static cbhal::ComputeBoardHal board;

void setup() {
    board.begin();        // releases sleep holds, enables VCC_AUX, config pin = input
}

void loop() {
    board.enableAuxRail();
    delay(1000);
    board.disableAuxRail();
    delay(1000);
}
