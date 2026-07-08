// Pure implementation of the deep-sleep plan builder. No hardware here.

#include "power_plan.h"

namespace cbhal {

bool SleepPlan::present(int gpio) const {
    for (std::size_t i = 0; i < opCount_; ++i) {
        if (ops_[i].gpio == gpio) return true;
    }
    return false;
}

void SleepPlan::addUnique(int gpio, PinAction action) {
    if (!isValidGpio(gpio)) return;
    // The rail, config and LED pins have dedicated drive-LOW + hold handling in
    // the driver and never appear in ops (Hi-Z on the LED would let the internal
    // pull-up bleed current into the active-high LED during sleep).
    if (gpio == kPinVccAuxEna || gpio == kPinConfigEna || gpio == kPinLed) return;
    if (present(gpio)) return;        // first action registered for a pin wins
    if (opCount_ >= ops_.size()) return;
    ops_[opCount_++] = PinOp{gpio, action};
}

SleepPlan SleepPlan::build(const int* registered, std::size_t count) {
    SleepPlan plan;

    // 1. On-board bus pins on the VCC_AUX rail -> Hi-Z + hold.
    for (int gpio : kDefaultAuxHiZPins) {
        plan.addUnique(gpio, PinAction::HiZHold);
    }

    // 2. Application-registered peripheral pins -> Hi-Z + hold. GPIO0 is never
    //    promoted to Hi-Z even if registered: it must keep its pull-up for the
    //    boot strap (see kPinBoot), so it stays in the reset set below.
    if (registered != nullptr) {
        for (std::size_t i = 0; i < count; ++i) {
            if (registered[i] == kPinBoot) continue;
            plan.addUnique(registered[i], PinAction::HiZHold);
        }
    }

    // 3. Strapping / always-on pins -> reset. Skipped automatically if a pin was
    //    already claimed as Hi-Z above (Hi-Z wins).
    for (int gpio : kAlwaysOnResetPins) {
        plan.addUnique(gpio, PinAction::Reset);
    }

    return plan;
}

bool SleepPlan::contains(int gpio, PinAction action) const {
    for (std::size_t i = 0; i < opCount_; ++i) {
        if (ops_[i].gpio == gpio && ops_[i].action == action) return true;
    }
    return false;
}

}        // namespace cbhal
