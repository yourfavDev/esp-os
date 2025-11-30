# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP-IDF (Espressif IoT Development Framework) project located within the ESP-IDF repository structure. ESP-IDF is the official development framework for Espressif SoCs (ESP32, ESP32-S2, ESP32-C3, ESP32-S3, ESP32-C6, ESP32-H2, ESP32-P4, etc.).

**Project Location**: `/Users/dev/Documents/esp32/esp-idf/projects/hello`
**ESP-IDF Root**: `/Users/dev/Documents/esp32/esp-idf`

## Build System Architecture

ESP-IDF uses CMake as its build system with a custom component-based architecture:

### Project Structure
- **Top-level CMakeLists.txt**: Must include `$ENV{IDF_PATH}/tools/cmake/project.cmake` and call `project(name)`
- **main/ directory**: Contains the main application component with its own CMakeLists.txt
- **components/ directory** (optional): Additional components specific to this project
- **ESP-IDF components**: Located at `$IDF_PATH/components/` (100+ built-in components like esp_wifi, freertos, driver, etc.)

### Component System
Each component has:
- A CMakeLists.txt using `idf_component_register()`
- SRCS: source files (.c, .cpp)
- INCLUDE_DIRS: header directories
- PRIV_INCLUDE_DIRS: private headers
- REQUIRES/PRIV_REQUIRES: component dependencies

Components automatically link and include each other based on dependencies.

## Environment Setup

Before running any build commands, the ESP-IDF environment must be set up:

```bash
# From ESP-IDF root directory (/Users/dev/Documents/esp32/esp-idf)
# First time only:
./install.sh   # or install.bat on Windows

# Every new shell session:
. ./export.sh  # or .\export.bat on Windows, source export.fish for fish shell
```

This sets up:
- `IDF_PATH` environment variable
- Toolchain paths (xtensa-esp32-elf-gcc, riscv32-esp-elf-gcc, etc.)
- Python virtual environment with ESP-IDF tools
- `idf.py` command availability

## Essential Commands

All commands are run from the project directory using `idf.py` (a wrapper around CMake and build tools):

### Initial Setup
```bash
# Set target chip (must be done first for new projects)
idf.py set-target esp32      # or esp32s2, esp32s3, esp32c3, esp32c6, etc.

# Configure project options
idf.py menuconfig            # Text-based configuration menu
```

### Build Commands
```bash
# Full build (app + bootloader + partition table)
idf.py build

# Build only the application (faster for development)
idf.py app

# Clean build
idf.py fullclean
```

### Flash & Monitor
```bash
# Flash to device
idf.py -p PORT flash         # PORT: /dev/ttyUSB0 (Linux), COM3 (Windows), /dev/cu.usbserial-* (macOS)

# Flash only the app (faster)
idf.py -p PORT app-flash

# Monitor serial output
idf.py -p PORT monitor       # Exit with Ctrl+]

# Build, flash, and monitor in one command
idf.py -p PORT flash monitor

# Erase entire flash
idf.py -p PORT erase-flash
```

### Testing & Debugging
```bash
# Run unit tests (if pytest is configured)
pytest                       # From project directory

# Size analysis
idf.py size                  # Show binary size breakdown
idf.py size-components       # Per-component size
idf.py size-files           # Per-file size
```

## Configuration System (Kconfig)

ESP-IDF uses Kconfig for configuration:
- `idf.py menuconfig` opens the configuration interface
- Settings are saved to `sdkconfig` (tracked in git)
- `sdkconfig.defaults` can provide default configurations
- Components can add their own Kconfig files for custom options
- After changing target (set-target), configuration is automatically regenerated

## Important Paths

- **IDF_PATH**: `/Users/dev/Documents/esp32/esp-idf` - ESP-IDF framework root
- **Build output**: `build/` directory (gitignored)
- **Binary files**: `build/*.bin` (bootloader.bin, partition_table.bin, app_name.bin)
- **Components**: `$IDF_PATH/components/` - 100+ framework components
- **Tools**: `$IDF_PATH/tools/` - Build tools, idf.py, monitor, etc.

## Application Entry Point

The main application entry point is `void app_main(void)` defined in `main/hello.c` (or main/*.c).
- This function is called by FreeRTOS after initialization
- It runs in the context of the "main" task
- The function can return or run forever in a loop
- Other tasks can be created using FreeRTOS APIs (xTaskCreate, etc.)

## Serial Port Detection

Common serial port names:
- **Linux**: `/dev/ttyUSB0`, `/dev/ttyUSB1`, etc.
- **macOS**: `/dev/cu.usbserial-*`, `/dev/cu.SLAB_USBtoUART`
- **Windows**: `COM1`, `COM3`, etc.

If `-p PORT` is omitted, `idf.py flash` will attempt to auto-detect the first available port.

## Multi-Target Support

This project can be built for different ESP32 chip variants:
- Change target with `idf.py set-target <chip>`
- Supported chips: esp32, esp32s2, esp32s3, esp32c2, esp32c3, esp32c5, esp32c6, esp32c61, esp32h2, esp32p4
- Each target has different capabilities (CPU arch, peripherals, memory)
- Check compatibility in `$IDF_PATH/COMPATIBILITY.md`

## Common Development Workflow

1. Set target chip: `idf.py set-target esp32`
2. Configure if needed: `idf.py menuconfig`
3. Write code in `main/` or add components
4. Build: `idf.py build`
5. Flash and monitor: `idf.py -p PORT flash monitor`
6. Debug: Use serial output, GDB, or JTAG debugging

## FreeRTOS Integration

ESP-IDF is built on FreeRTOS:
- All ESP-IDF components use FreeRTOS tasks, queues, semaphores
- Include `<freertos/FreeRTOS.h>` and related headers
- Common APIs: `xTaskCreate()`, `vTaskDelay()`, `xQueueCreate()`, etc.
- Dual-core chips (ESP32, ESP32-S3) support pinning tasks to specific cores

## Documentation

- Online docs: https://docs.espressif.com/projects/esp-idf/
- API reference for all components
- Each release (v5.1, v5.2, v5.3, v5.4, v5.5, v6.0) has its own documentation
- Example projects: `$IDF_PATH/examples/`
