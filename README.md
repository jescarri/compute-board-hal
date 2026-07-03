# ComputeBoardHal

A mini hardware abstraction layer for the custom **ESP32-WROOM-32E `compute-board`**.
It wraps the three things every add-on for this board needs to get right:

1. **Pin definitions** — the schematic-traced pin map in one place.
2. **Secondary rail control** — bring up / tear down the `VCC_AUX` rail that powers
   all peripherals.
3. **Ultra-low-power deep sleep** — latch every configured GPIO into Hi-Z and hold
   `VCC_AUX_ENA` LOW for the whole sleep so the rail collapses to near-zero current.

It is a PlatformIO library for the **Arduino** framework and also builds under
**ESP-IDF** (the driver uses IDF `gpio_*` / `esp_sleep_*` APIs, which exist under
both).

## Why this exists

On the compute-board every peripheral is powered from a secondary rail (`VCC_AUX`)
enabled by an active-high LDO on **GPIO13**, which has a board pull-up so the rail
is ON by default. To reach microamp-level deep-sleep current you must (a) put all
peripheral I/O into Hi-Z and *latch* it through sleep, and (b) drive the rail-enable
LOW and *hold* it LOW for the entire sleep. This library packages that exact
sequence — proven on the sibling `lora-sensor` board — behind a small API.

See [`docs/PINOUT.md`](docs/PINOUT.md) for the full pin map and
[`docs/adr/ADR001.md`](docs/adr/ADR001.md) for the architecture rationale.

## Install

`platformio.ini`:

```ini
lib_deps = https://github.com/jescarri/compute-board-hal.git
```

## Quick start

```cpp
#include <ComputeBoardHal.h>

static cbhal::ComputeBoardHal board;

void setup() {
    board.begin();                       // release sleep holds, enable VCC_AUX, config pin = input
    board.registerPins({25, 26, 27});    // peripheral pins this app drives on VCC_AUX

    if (board.isConfigAsserted()) {      // config button (GPIO12) held?
        // enter your config portal ...
        return;
    }

    // ... read sensors / transmit while VCC_AUX is powered ...

    board.deepSleepSeconds(60);          // Hi-Z all pins, collapse VCC_AUX, sleep 60 s
}

void loop() {}
```

## API

| Method | Purpose |
|---|---|
| `begin()` | Release prior deep-sleep holds, enable `VCC_AUX`, set config pin as input. |
| `enableAuxRail()` / `disableAuxRail()` / `auxRailEnabled()` | Manual secondary-rail control. |
| `registerPin(gpio)` / `registerPins({...})` | Add application pins to force Hi-Z at sleep. |
| `isConfigAsserted()` | Probe `CONFIG_ENA` (GPIO12). Returns `true`/`false` (active-low by default). |
| `setConfigActiveLow(bool)` | Override config-button polarity. |
| `ledOn()` / `ledOff()` / `setLed(bool)` / `toggleLed()` | Drive the on-board LED (LED1 / GPIO15), a plain on/off LED. |
| `setRtcPowerConfig(cfg)` / `rtcPowerConfig()` | Persistent RTC power-domain config (default: all OFF). |
| `deepSleepSeconds(s[, cfg])` / `deepSleepMicros(us[, cfg])` | Enter deep sleep; does not return. |
| `buildPlan()` | Inspect the deep-sleep pin plan without sleeping. |

### RTC power domains

Deep sleep powers down all RTC domains by default (lowest current). To keep RTC
memory — e.g. for RTC-retained variables or a wake stub — pass a config:

```cpp
board.deepSleepSeconds(60, cbhal::RtcPowerConfig::keepRtcMemory());
// or persistently:
board.setRtcPowerConfig(cbhal::RtcPowerConfig::keepRtcMemory());
```

`RtcPowerConfig` is a plain aggregate; you can also set each domain
(`rtc_periph`, `rtc_slow_mem`, `rtc_fast_mem`, `xtal`) to `Off` / `On` / `Auto`.

### On-board LED

`LED1` (GPIO15) is a plain on/off LED on the `VCC_AUX` rail (active-high). No
external library is required.

```cpp
board.ledOn();
board.ledOff();
board.setLed(true);      // == ledOn()
board.toggleLed();
```

The `LedBlink` example blinks it and also demonstrates the config-button probe
(prints `CONF_PRESSED`, waits 10 s, and reboots when the button is pressed).

## Building the examples

A `Makefile` wraps the common tasks:

```bash
make test                                   # host unit tests
make build  EXAMPLE=LedBlink                # compile an example for the board
make upload EXAMPLE=LedBlink PORT=/dev/ttyUSB0   # compile + flash to a board
make build-all                              # compile every example
```

## Debugging

Build with `-D CBHAL_DEBUG` to have the HAL trace what it does over `Serial`
(the sketch must call `Serial.begin()`):

```ini
build_flags = -D CBHAL_DEBUG
```

It prints the `VCC_AUX_ENA`/`CONFIG_ENA` pin levels in `begin()`, every rail and
LED change, and the deep-sleep entry. The `LedBlink` example also prints the
boot/reset reason and the rail pin level each cycle without the flag.

> **Boot caveat — GPIO12 (`CONFIG_ENA` / MTDI).** GPIO12 is the ESP32 flash-voltage
> strapping pin and must be **low at reset**. An external pull-up on the config
> button forces it high → selects 1.8 V flash → intermittent boot failures
> (`invalid header: 0xffffffff`, `RTCWDT_RTC_RESET`) and a "sometimes" LED.
> **Recommended: remove the external pull-up.** GPIO12 has an internal pull-down at
> reset (correct 3.3 V strap), and the HAL supplies the pull-up at runtime
> (`INPUT_PULLUP`) for the button; deep sleep drives GPIO12 low + holds it so the
> wake reset also straps correctly. A cap from GPIO12 to GND is fine.

## Design & testing

The library is split into a **pure core** (`board_pins.h`, `power_plan.*`) that
contains all the decision logic as plain data, and a **thin ESP32 driver**
(`ComputeBoardHal.cpp`) that replays that data against the hardware APIs. Because
the core is hardware-free, it is unit-tested on the host with Unity — **no
hardware is mocked**. The hardware path is verified by compiling for the target.

```bash
# Host unit tests (no board required, nothing mocked)
pio test -e native

# Compile-check an example for the ESP32 target
pio ci --lib="." --board=esp32dev --project-option="framework=arduino" \
  examples/DeepSleepCycle/DeepSleepCycle.ino
```

## License

MIT — see [LICENSE](LICENSE).
