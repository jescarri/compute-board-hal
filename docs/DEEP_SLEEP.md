# Deep-sleep GPIO handling & `rtc_gpio_isolate()`

This note explains how the HAL drives GPIOs to their lowest-power state at deep
sleep, and specifically **when to use `rtc_gpio_isolate()`, how it works, and when
*not* to use it**. It exists because getting this wrong is the difference between
tens of µA of avoidable leakage and a board that won't wake correctly.

On this board, isolating the RTC pads took measured deep-sleep current from
**31.08 µA → 20.74 µA** — a ~10 µA win from internal pulls that were otherwise
held live for the entire sleep.

## Background: why RTC pads are special

Every ESP32 GPIO pad is driven by the **digital IO-MUX** in active mode. A subset
of pads — the **RTC-capable GPIOs** (`RTC_GPIO`) — are *also* wired to the **RTC
IO mux** and can be controlled by the always-on RTC domain:

```
{0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39}   ← isRtcGpio()
```

In deep sleep the **digital domain is powered down**, so a *digital-only* pad
(e.g. the I²C/SPI pins 21/22/5/18/19/23) simply goes quiet — its internal
pull-up/down is de-energised with the domain. But an **RTC pad retains whatever
the RTC mux is holding** — direction, and crucially its **internal pull-up/down**.
So if an RTC pad enters sleep with an internal pull enabled and there is *any* DC
path on the net (an external pull to the opposite rail, an LED, a divider, a
sensor input), that pull sources/sinks current for the **entire** sleep.

That is exactly how leakage sneaks in:

- `gpio_reset_pin()` **enables the internal pull-up** — so "resetting" an RTC
  strapping pin actually arms a pull-up that can leak (this is what bled ~30 µA
  through the GPIO15 LED before it was special-cased).
- A peripheral (I²C, one-wire) may have left a pull-up enabled on an RTC pad.
- A floating RTC input with a marginal external bias can idle in the pad's
  linear region.

## What `rtc_gpio_isolate()` does

```c
esp_err_t rtc_gpio_isolate(gpio_num_t gpio);   // driver/rtc_io.h
```

For one RTC pad it, in a single call:

1. selects the **RTC** function for the pad,
2. **disables input**, **disables output**,
3. **disables pull-up and pull-down**, and
4. **enables the RTC pad hold**, latching that isolated state.

The result is a genuine high-impedance island: no internal driver, no internal
pull, held that way through the whole sleep. There is nothing left inside the
chip to conduct — the only current that can flow on that net is through external
components, which is out of firmware's hands.

### The mandatory wake-up step

The RTC hold **survives the deep-sleep wake reset** (that persistence is the whole
point during sleep). So on the next boot you **must release it** before the pad
can be used again:

```c
rtc_gpio_hold_dis(gpio);          // per pin, or
rtc_gpio_force_hold_dis_all();    // all RTC pads at once
```

The HAL does this for you in `begin()` — it walks the RTC set and calls
`rtc_gpio_hold_dis()` on each, so isolated SD/I²C/sensor pads are free to be
re-initialised. **If you isolate a pin yourself and forget this, the pad stays
latched and every later `pinMode()` / `Wire.begin()` / `SPI.begin()` silently
does nothing** — the classic "peripheral is dead after the first deep-sleep
wake" bug.

## When to use it

Use `rtc_gpio_isolate()` on an **RTC-capable** pad that you are parking as a
floating input for sleep, especially when:

- the pad might have an **internal pull** enabled (from `gpio_reset_pin()`, a
  peripheral driver, or the ROM default), **and**
- the net has any **external DC path** (pull resistor, LED, divider, another
  device) — or you simply want to *guarantee* no internal contribution.

Rule of thumb in this HAL: **every Hi-Z pin where `isRtcGpio()` is true gets
isolated.** Espressif makes the same recommendation for `GPIO_NUM_12` on WROVER
modules. It is the right default for unused/peripheral RTC pins.

## When *not* to use it

- **Pins you must hold at a defined level through sleep.** The rail-enable
  (GPIO13, held LOW to keep the LDO off), the config/MTDI pin (GPIO12, held LOW
  for the 3.3 V flash strap) and the LED (GPIO15, held LOW so it can't bleed)
  are **driven + `gpio_hold_en`**, *not* isolated. Isolating them removes the
  driver and lets the pin float, losing the level you need.
- **Strapping pins that need an internal pull at the wake reset.** On this board
  **GPIO0 has no external pull-up** and relies on the internal pull-up to strap
  boot-from-flash. Isolating it would float it → risk of dropping into download
  mode on wake. The HAL keeps GPIO0 on `gpio_reset_pin()` (pull-up retained) and
  `SleepPlan::build()` refuses to promote it to Hi-Z even if registered.
- **Non-RTC (digital-only) pins.** `rtc_gpio_isolate()` returns
  `ESP_ERR_INVALID_ARG` on them. Use the digital float + `gpio_hold_en` path
  (the HAL does this automatically for `!isRtcGpio()` pins).
- **Pins that must stay connected during sleep** — an `EXT0`/`EXT1` GPIO wake
  source, or a ULP / touch input. Isolating removes them from the RTC mux and
  they can't do their job.
- **Input-only pins (34–39).** Harmless to isolate but pointless — they have no
  internal pull hardware to disconnect; the HAL leaves them on `gpio_reset_pin()`.

## Downsides & gotchas

- **Forgotten hold release** is the #1 footgun — see the wake-up step above.
  Symptom: a peripheral works on the very first boot but is dead after any
  deep-sleep wake.
- **The pad floats.** Isolation is ideal for *leakage*, but a floating net with
  no external bias is undefined if something downstream tries to read it. Add an
  external pull if the *board* needs a defined level while the MCU sleeps.
- **Strap coupling.** Isolating a strapping pin hands the strap decision entirely
  to external circuitry at the wake reset. Only safe when the board defines that
  strap without the internal pull.
- **Not portable.** It only applies to RTC pads and is ESP32-family specific.
  Non-RTC pins need the digital hold path instead.

## How the HAL wires this together

| Step | Code | Effect |
|---|---|---|
| Decide | `isRtcGpio()` (pure, unit-tested) | classifies each Hi-Z pin |
| Sleep — RTC Hi-Z pins | `rtc_gpio_isolate(pin)` | disconnect in/out/pulls + hold |
| Sleep — non-RTC Hi-Z pins | `gpio_set_direction(INPUT)` + float + pull-dis + `gpio_hold_en` | digital latched Hi-Z |
| Sleep — driven pins (12/13/15) | `driveOutput(LOW)` + `gpio_hold_en` | held LOW, **not** isolated |
| Sleep — GPIO0 & input-only | `gpio_reset_pin()` | keep pull-up (strap) / no-op pulls |
| Wake | `begin()` → `rtc_gpio_hold_dis()` over the RTC set | release isolation so pads are usable |

See also [PINOUT.md](PINOUT.md) for the per-pin roles and the strapping-pin
caveats, and the README section "Choosing which pins go Hi-Z at deep sleep" for
the application-facing `registerPins()` API.
