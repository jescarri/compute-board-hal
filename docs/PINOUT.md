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
| WS2812 LED | 15 | `kPinLed` |
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
| Strapping / always-on | `kAlwaysOnResetPins` = {0, 2, 15, 34, 35, 36, 37, 38, 39} | `gpio_reset_pin` |
| Config (strapping) | `kPinConfigEna` = 12 | driven **LOW** + `gpio_hold_en` (MTDI strap must be low at wake) |
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
- **The on-board LED (`XL-0807RGBC-WS2812B`) needs VDD ≥ 3.5 V.** Per its datasheet
  the supply range is **3.5–5.5 V** ("100% functional above 4.5 V"), but it is wired
  to the **3.3 V `VCC_AUX`** rail — **below its minimum**. At 3.3 V it lights but
  decodes data unreliably (wrong/shifting colours); this is a supply-voltage issue,
  not a firmware one. The data order is **GRB** (matches the HAL). Because this part's
  data threshold is **`VIH = 0.55 × VDD`**, powering VDD from **5 V** still accepts the
  ESP32's 3.3 V data with **no level shifter** (2.75 V threshold < 3.3 V). Feeding the
  LED from 5 V (ideally a rail still gated for deep-sleep) is the reliable fix.
