# Compute-Board Mini HAL -- developer tasks.
#
# Unit tests run on the host; examples are compiled/uploaded to a real ESP32.
# Select the example and (for upload) the serial port on the command line:
#
#   make test
#   make build EXAMPLE=LedBlink
#   make upload EXAMPLE=LedBlink PORT=/dev/ttyUSB0
#   make build-all
#
# The example sources live in examples/<name>/<name>.ino. The `example`
# PlatformIO env (see platformio.ini) builds whichever directory
# PLATFORMIO_SRC_DIR points at, with this repo linked in as the library.

EXAMPLE ?= LedBlink
BOARD   ?= esp32dev
PORT    ?=

PIO         := platformio
EXAMPLE_DIR := examples/$(EXAMPLE)
UPLOAD_PORT := $(if $(PORT),--upload-port $(PORT),)
SRC_FILES   := $(shell find src examples test -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.ino' \))

.PHONY: all help test build upload clean build-all format check \
	sdcard-example upload-sdcard-example

all: build  ## Default: build the selected example

help:  ## List available targets
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  %-12s %s\n", $$1, $$2}'

test:  ## Run the host unit tests (no board required)
	$(PIO) test -e native

build:  ## Compile EXAMPLE for the board
	PLATFORMIO_SRC_DIR=$(EXAMPLE_DIR) $(PIO) run -e example

upload:  ## Compile + flash EXAMPLE to the board (set PORT=/dev/tty...)
	PLATFORMIO_SRC_DIR=$(EXAMPLE_DIR) $(PIO) run -e example -t upload $(UPLOAD_PORT)

sdcard-example:  ## Compile the SdCardList example for the board
	$(MAKE) build EXAMPLE=SdCardList

upload-sdcard-example:  ## Compile + flash the SdCardList example (set PORT=/dev/tty...)
	$(MAKE) upload EXAMPLE=SdCardList

clean:  ## Remove PlatformIO build output
	PLATFORMIO_SRC_DIR=$(EXAMPLE_DIR) $(PIO) run -e example -t clean || true
	rm -rf .pio

build-all:  ## Compile every example (what CI does)
	@for ex in examples/*/; do \
		name=$$(basename $$ex); \
		echo ">> building $$name"; \
		PLATFORMIO_SRC_DIR=$$ex $(PIO) run -e example || exit 1; \
	done

format:  ## Apply clang-format in place
	clang-format -i $(SRC_FILES)

check:  ## Verify clang-format is clean (CI gate)
	clang-format --dry-run --Werror $(SRC_FILES)
