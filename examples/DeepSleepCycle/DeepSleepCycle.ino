// DeepSleepCycle -- Wi-Fi wake / HTTP fetch / deep-sleep power-measurement loop.
//
// Built to measure the board's deep-sleep current draw around a realistic duty
// cycle: on each boot the HAL brings up VCC_AUX and registers the peripheral
// pins, we join Wi-Fi, do one HTTPS GET, then drop into deep sleep for 10
// minutes -- which latches every peripheral pin into Hi-Z and holds VCC_AUX_ENA
// LOW so the secondary rail collapses for the whole sleep. Holding the config
// button at boot keeps the board awake instead (e.g. to run a config portal).
//
// The Wi-Fi credentials are injected at COMPILE time (never checked into
// source) via -DWIFI_SSID / -DWIFI_PASS. Build/flash through the Makefile, which
// supplies them and aborts if they are unset:
//
//   make deep-sleep-example        WIFI_SSID=MyNet WIFI_PASS=secret
//   make upload-deep-sleep-example WIFI_SSID=MyNet WIFI_PASS=secret PORT=/dev/ttyUSB0

#include <ComputeBoardHal.h>

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Credentials arrive from the compiler command line. Fail the build loudly if
// the Makefile (or your build flags) didn't set them.
#if !defined(WIFI_SSID) || !defined(WIFI_PASS)
#error "WIFI_SSID and WIFI_PASS must be set at compile time -- build with `make deep-sleep-example WIFI_SSID=<ssid> WIFI_PASS=<password>`"
#endif

static cbhal::ComputeBoardHal board;

// Sleep 10 minutes between wakes.
static constexpr uint64_t kSleepSeconds = 10 * 60;

// Endpoint hit once per wake.
static constexpr char kUrl[] = "https://dummy-json.mock.beeceptor.com/posts";

// Bound the association attempt so a bad/absent AP can't keep us awake (and
// burning current) forever.
static constexpr uint32_t kWifiTimeoutMs = 20000;

// Join Wi-Fi, returning true once we have an IP. Bounded by kWifiTimeoutMs.
static bool connectWifi() {
    Serial.printf("[DeepSleep] connecting to SSID \"%s\"\r\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > kWifiTimeoutMs) {
            Serial.println("[DeepSleep] Wi-Fi connect timed out\r");
            return false;
        }
        delay(200);
    }
    Serial.printf("[DeepSleep] connected, IP %s\r\n", WiFi.localIP().toString().c_str());
    return true;
}

// One HTTPS GET against kUrl. Prints the status code and body length.
static void fetchOnce() {
    WiFiClientSecure client;
    client.setInsecure();        // skip cert validation -- fine for this power test

    HTTPClient http;
    if (!http.begin(client, kUrl)) {
        Serial.println("[DeepSleep] http.begin() failed\r");
        return;
    }

    const int code = http.GET();
    if (code > 0) {
        const String body = http.getString();
        Serial.printf("[DeepSleep] GET %s -> %d (%u bytes)\r\n", kUrl, code,
                      static_cast<unsigned>(body.length()));
    } else {
        Serial.printf("[DeepSleep] GET failed: %s\r\n",
                      HTTPClient::errorToString(code).c_str());
    }
    http.end();
}

// Cleanly power the radio down before we drop into (and measure) deep sleep.
static void wifiOff() {
    WiFi.disconnect(true);        // disconnect + turn the radio off
    WiFi.mode(WIFI_OFF);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\r\n[DeepSleep] boot\r");

    board.begin();

    // Sleep-current measurement: force every otherwise-unused GPIO into latched
    // Hi-Z so nothing is left floating during deep sleep. The HAL already handles
    // the on-board bus pins (I2C 21/22 + SD 5/18/19/23) and drives the rail (13),
    // config (12) and LED (15) pins LOW+held, so here we add all the remaining
    // header-breakout GPIOs. Deliberate exclusions:
    //   - GPIO0        : no external pull-up on this board -> must keep its
    //                    internal pull-up to strap boot-from-flash on wake.
    //   - GPIO1/3      : UART0 console; non-RTC pads power down on their own and
    //                    leaving them keeps the serial log alive.
    //   - GPIO34/35/36/39 : input-only (no drivers, no pulls) -> can't leak; the
    //                    HAL resets them.
    board.registerPins({2, 4, 14, 16, 17, 25, 26, 27, 32, 33});

    if (board.isConfigAsserted()) {
        // Config button held at boot: stay awake and hand off to your portal.
        // (Left to the application; we simply return here.)
        Serial.println("[DeepSleep] config asserted -- staying awake\r");
        return;
    }

    if (connectWifi()) {
        fetchOnce();
    }
    wifiOff();

    Serial.printf("[DeepSleep] sleeping for %llu s\r\n", kSleepSeconds);
    Serial.flush();

    // Sleep with the default all-domains-off RTC config. To keep RTC memory
    // instead, pass RtcPowerConfig::keepRtcMemory().
    board.deepSleepSeconds(kSleepSeconds);
}

void loop() {
    // Not reached in the sleep path; used only when the config button is held.
}
