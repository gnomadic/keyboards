# ZMK Keyboard Firmware Configuration Guide

This repository contains ZMK (Zephyr Mechanical Keyboard) firmware configurations for custom split keyboards with dongle support.

## Architecture Overview

**Shield-based Configuration System**: Each keyboard is defined as a "shield" that layers onto microcontroller boards (nice_nano_v2, seeeduino_xiao_ble). The `boards/shields/` directory contains complete keyboard definitions including matrix configuration, pin mappings, and keymaps.

**Split Keyboard Topology**: Most configurations support 3-part split keyboards:
- Left/Right halves (peripherals) - physical keyboard halves with switches
- Dongle (central) - wireless receiver with mock kscan for key aggregation
- Dongles use `mock_kscan` with no physical switches but aggregate inputs from peripherals

**Build Matrix System**: `build.yaml` defines GitHub Actions build matrix with specific board+shield combinations, cmake flags, and artifacts. Commented sections show inactive configurations that can be enabled.

## Key Files and Patterns

### Shield Definition Structure
Each keyboard shield requires these files:
- `{keyboard}.dtsi` - Base device tree with matrix transform and layout
- `{keyboard}_left.overlay` - Left half GPIO pin configuration  
- `{keyboard}_right.overlay` - Right half GPIO pin configuration
- `{keyboard}_dongle.overlay` - Central receiver with mock kscan
- `{keyboard}.keymap` - Key bindings and layer definitions
- `Kconfig.shield` - Build system shield definitions
- `{keyboard}.zmk.yml` - Metadata for ZMK ecosystem

### Configuration Patterns
- **Matrix Transform**: Maps physical switch positions to logical key indices in `.dtsi` files using `RC(row,col)` notation
- **GPIO Configuration**: Pin assignments in `.overlay` files use `&pro_micro` references for standard microcontroller pins
- **Layer System**: Keymaps define multiple layers (BASE, LOWER, RAISE, ADJUST) with `#define` constants
- **Conditional Layers**: Auto-activate layers when multiple modifier layers are pressed simultaneously

### Split Keyboard Configuration
- **Central Role**: Dongles are configured with `CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=2` in `.conf` files
- **Peripheral Role**: Left/right halves built with `-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n` cmake flag
- **Connection Management**: Central supports multiple BT connections via `CONFIG_BT_MAX_CONN` and `CONFIG_BT_MAX_PAIRED`

## Development Workflows

### Adding New Keyboards
1. Create shield directory under `boards/shields/{keyboard_name}/`
2. Define base `.dtsi` with matrix transform matching physical layout
3. Create separate overlays for left, right, and dongle variants
4. Add build entries to `build.yaml` (initially commented for testing)
5. Test with single board+shield combination before enabling full matrix

### Build System
- **Local builds**: Use West build system with ZMK as upstream dependency (defined in `config/west.yml`)
- **CI/CD**: GitHub Actions automatically builds all `build.yaml` configurations on push
- **Dependencies**: External modules like `zmk-dongle-screen` added via West manifest remotes

### Configuration Testing
- **Studio Support**: Many configs include `CONFIG_ZMK_STUDIO=y` for live keymap editing
- **Development flags**: Use `CONFIG_ZMK_IDLE_TIMEOUT` and logging configs for debugging
- **Display integration**: OLED support via `CONFIG_ZMK_DISPLAY=y` and I2C configuration

## Project-Specific Conventions

**Shield Naming**: Use descriptive suffixes (`_left`, `_right`, `_dongle`) rather than generic terms
**Pin Assignments**: Maintain consistent GPIO mappings across keyboard variants for easier troubleshooting  
**Layer Management**: Prefer conditional layers over manual layer switching for modifier combinations
**Configuration Files**: Keep board-specific settings in `.conf` files separate from universal keymap definitions
**Build Artifacts**: Use meaningful `artifact-name` in build.yaml to distinguish between board variants

## Working with Hardware Variants
- **Board Overlays**: Hardware-specific configurations in `boards/{board_name}.overlay` (LED strips, I2C, SPI)
- **Multiple Controllers**: Support different microcontrollers (nice_nano_v2, nrfmicro) via separate overlay files
- **Display Integration**: OLED screens configured through I2C in board-specific overlays with proper pin control

When modifying configurations, always test with a single board combination before adding to the full build matrix to catch configuration errors early.