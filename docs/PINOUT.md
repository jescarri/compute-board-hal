# Compute-Board Pinout

Authoritative ESP32-WROOM-32E pin assignments for the `compute-board` baseboard.
Every entry below was traced from the net-to-pin connectivity in
`compute-board.kicad_sch` (module reference `U1`), not inferred. These are the
values encoded in [`src/board_pins.h`](../src/board_pins.h).

## Control signals

| Signal | GPIO | Direction | Idle state | Notes |
|---|---|---|---|---|
| `VCC_AUX_ENA` | **13** | Output | HIGH (rail ON) | Drives the `XC6220` LDO (U2) enable. Active-high with a board pull-up, so the secondary rail powers up by default. Deep sleep drives it **LOW and holds it**. |
| `CONFIG_ENA` | **12** | Input | HIGH | Config button `SW2` to GND with pull-up (`R8`) + RC debounce (`C7`) → **active-low**. Also the MTDI strapping pin — see caveat below. |

## On-board peripherals

| Function | GPIO | Constant |
|---|---|---|
| LED1 (on/off LED) | 15 | `kPinLed` |
| I²C SDA | 21 | `kPinSda` |
| I²C SCL | 22 | `kPinScl` |
| SPI / SD CS | 5 | `kPinSdCs` |
| SPI / SD SCK | 18 | `kPinSdSck` |
| SPI / SD MISO | 19 | `kPinSdMiso` |
| SPI / SD MOSI | 23 | `kPinSdMosi` |
| UART0 RX | 3 | `kPinUart0Rx` |
| UART0 TX | 1 | `kPinUart0Tx` |

The SPI/SD and I²C pins live on the `VCC_AUX` rail and are placed in latched Hi-Z
automatically at deep sleep (`kDefaultAuxHiZPins`).

## Broken-out header (J1) GPIOs

Available on the add-on header for application peripherals. Register the ones you
drive with `board.registerPins({...})` so they are forced into Hi-Z at sleep.

| GPIO | Capability |
|---|---|
| 2, 4, 16, 17, 25, 26, 27, 32, 33 | full I/O |
| 34, 35, 36, 39 | **input-only** (no output drivers, no internal pulls) |

`GPIO37`/`GPIO38` are not exposed on the WROOM module but are treated as
input-only for safety in `isInputOnly()`.

## Deep-sleep pin roles

| Set | Constant | Action at sleep |
|---|---|---|
| VCC_AUX bus pins | `kDefaultAuxHiZPins` = {21, 22, 5, 18, 19, 23} | `INPUT` + float + disable pulls + `gpio_hold_en` |
| Application pins | (registered at runtime) | same Hi-Z + hold |
| RTC-capable Hi-Z pins | any Hi-Z pin where `isRtcGpio()` | `rtc_gpio_isolate()` (disconnect in/out/pulls + hold); released by `rtc_gpio_hold_dis()` at `begin()` — see [DEEP_SLEEP.md](DEEP_SLEEP.md) |
| Strapping / always-on | `kAlwaysOnResetPins` = {0, 2, 34, 35, 36, 37, 38, 39} | `gpio_reset_pin` |
| Boot strap | `kPinBoot` = 0 | `gpio_reset_pin` only — **never Hi-Z/isolated** (no board pull-up; needs internal pull-up to strap flash boot) |
| Config (strapping) | `kPinConfigEna` = 12 | driven **LOW** + `gpio_hold_en` (MTDI strap must be low at wake) |
| LED1 (strapping) | `kPinLed` = 15 | driven **LOW** + `gpio_hold_en` (active-high LED — see caveat) |
| Rail enable | `kPinVccAuxEna` = 13 | driven **LOW**, `gpio_hold_en`, `gpio_deep_sleep_hold_en` |

If a pin appears in both the registered/Hi-Z set and the reset set, **Hi-Z wins**
(the pin is latched, not reset).

## Caveats

- **GPIO12 is a strapping pin (MTDI).** It selects the flash voltage at boot and
  must read **LOW at reset** for a 3.3 V flash part. GPIO12 has an ESP32 **internal
  pull-down that is active at reset**, so the recommended board configuration is to
  **remove the external pull-up** on `CONFIG_ENA` and let the HAL provide the
  pull-up at runtime instead:
  - `begin()` enables the **internal pull-up** (`INPUT_PULLUP`) — no external
    resistor needed; the button still reads high idle / low pressed.
  - At reset (power-on and deep-sleep wake) the pin is low via the internal
    pull-down, so the flash strap is correct.
  - Deep sleep drives GPIO12 **LOW and latches it** (not Hi-Z): the wake reset
    re-samples the strap, and a floating pin with the debounce cap could otherwise
    hold a high charge into reset and mis-select 1.8 V flash.

  If you keep the external pull-up instead, it fights the strap: expect
  intermittent `invalid header: 0xffffffff` / `RTCWDT_RTC_RESET` boots, so don't
  hold the config button across reset. A small cap from GPIO12 to GND is fine.
- **GPIO0 (`ESP_PROG`)** is the boot/download strapping pin and is reset at sleep.
- Input-only pins (34–39) cannot be driven or held with output levels; they are
  reset rather than Hi-Z-held.
- **LED1 (GPIO15) is a plain on/off LED** (active-high). It is wired
  `GPIO15 → LED → 1 kΩ → GND`, i.e. the anode is driven **directly by the GPIO
  pad** (not from the `VCC_AUX` rail), so `ledOn()`/`ledOff()` work regardless of
  the rail state. The board originally carried an addressable WS2812-class part,
  but that chip's rated VDD is ≥ 3.5 V while this supply is 3.3 V, so it decoded
  data unreliably and was replaced with a basic LED.
- **GPIO15 is the MTDO strapping pin, and it must NOT be `gpio_reset_pin`'d at
  sleep.** `gpio_reset_pin()` **enables the internal pull-up** (~45 kΩ), which
  sources ~30 µA through the active-high LED and **faintly lights it during deep
  sleep**. The HAL therefore drives GPIO15 **LOW and latches it** (`gpio_hold_en`),
  exactly like the config pin, so the anode sits at 0 V and no current bleeds.
  Unlike GPIO12, GPIO15-low is a safe strap: it only **suppresses the ROM boot
  log** on U0TXD at the next wake (it does not affect flash voltage or boot mode),
  and the application's own `Serial` output after `Serial.begin()` is unaffected.
