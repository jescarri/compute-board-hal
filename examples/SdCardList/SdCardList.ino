// SdCardList -- mount the on-board microSD card and list its files over Serial.
//
// The microSD slot hangs off the SPI bus on the VCC_AUX rail (CS=5, SCK=18,
// MISO=19, MOSI=23 -- see docs/PINOUT.md). begin() brings VCC_AUX up so the
// card is powered; we then start SPI on those exact pins, mount the card with
// the Arduino SD library, and walk the directory tree once, printing every
// entry with its size. The on-board LED (LED1 / GPIO15) turns ON when the card
// reads successfully and stays OFF otherwise. The loop just idles afterwards.

#include <ComputeBoardHal.h>

#include <SD.h>
#include <SPI.h>

static cbhal::ComputeBoardHal board;

// Dedicated SPI instance for the SD bus (HSPI), wired to the board's SD pins.
static SPIClass sdSpi(HSPI);

// Recursively print every entry under `dir`. `indent` is the current depth,
// used only to pretty-print the tree.
static void listDir(File dir, int indent) {
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        for (int i = 0; i < indent; ++i) Serial.print("  ");
        if (entry.isDirectory()) {
            Serial.printf("%s/\r\n", entry.name());
            listDir(entry, indent + 1);
        } else {
            Serial.printf("%s  (%u bytes)\r\n", entry.name(),
                          static_cast<unsigned>(entry.size()));
        }
        entry.close();
    }
}

// Mount the card and list its contents. Returns true only if the card was
// mounted and its root directory could be read.
static bool mountAndList() {
    // Bring the SPI bus up on the board's SD pins, then mount the card.
    sdSpi.begin(cbhal::kPinSdSck, cbhal::kPinSdMiso, cbhal::kPinSdMosi, cbhal::kPinSdCs);

    if (!SD.begin(cbhal::kPinSdCs, sdSpi)) {
        Serial.println("[SdCardList] SD.begin() failed -- card missing or not detected\r");
        return false;
    }

    const uint8_t type = SD.cardType();
    if (type == CARD_NONE) {
        Serial.println("[SdCardList] no SD card attached\r");
        return false;
    }

    const char* typeStr = (type == CARD_MMC)    ? "MMC"
                          : (type == CARD_SD)   ? "SDSC"
                          : (type == CARD_SDHC) ? "SDHC"
                                                : "UNKNOWN";
    Serial.printf("[SdCardList] card type: %s  size: %llu MB\r\n", typeStr,
                  SD.cardSize() / (1024ULL * 1024ULL));

    Serial.println("[SdCardList] files:\r");
    File root = SD.open("/");
    if (!root) {
        Serial.println("[SdCardList] failed to open root directory\r");
        return false;
    }
    listDir(root, 0);
    root.close();
    Serial.println("[SdCardList] done\r");
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\r\n[SdCardList] boot\r");

    board.begin();        // release sleep holds, enable VCC_AUX (powers the card)

    // Register the SD bus pins so a later deep sleep would latch them Hi-Z. Not
    // strictly needed for this listing example, but keeps the pin ownership clear.
    board.registerPins(
        {cbhal::kPinSdCs, cbhal::kPinSdSck, cbhal::kPinSdMiso, cbhal::kPinSdMosi});

    // Light the on-board LED (LED1 / GPIO15) when the card reads successfully.
    const bool ok = mountAndList();
    board.setLed(ok);
    Serial.printf("[SdCardList] card readable: %d  (LED %s)\r\n", ok, ok ? "ON" : "OFF");
}

void loop() {
    // Nothing to do after the one-shot listing.
    delay(1000);
}
