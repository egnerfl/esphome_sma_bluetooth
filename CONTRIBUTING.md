# Contributing

## Prerequisites

- Python 3.11+ with `venv`
- PlatformIO Core (installed in the venv)
- Docker (for ESPHome compilation testing)

## Setup

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install platformio esphome
```

## Project Structure

```
components/sma_bluetooth/     # ESPHome external component
  __init__.py                 # Python codegen (config schema, entity metadata)
  sma_bluetooth_hub.h/.cpp    # Hub: BT stack, discovery, FreeRTOS task, NVS
  sma_inverter_device.h/.cpp  # Per-inverter: protocol, sensors, publishing
  sma_protocol.h              # Constants, LRI codes, status lookup, helpers
test/test_native/             # Native unit tests (no hardware needed)
platformio.ini                # PlatformIO build/test configuration
```

## Running Tests

### Native unit tests (protocol layer)

These test the pure protocol helpers (FCS checksum, byte extraction, NaN detection, unit conversions, status code lookup) on your host machine. No ESP32 needed.

```bash
# Using the venv PlatformIO
.venv/bin/pio test -e native

# Or if pio is on your PATH
pio test -e native
```

Expected output: all tests pass in ~5 seconds.

### ESPHome compile test (Docker)

This compiles the full component against ESPHome using Docker. Mount your local component directory so you can test without pushing.

```bash
docker run --rm \
  -v /path/to/your/config.yaml:/config/config.yaml \
  -v /path/to/esphome_sma_bluetooth:/local_component \
  ghcr.io/esphome/esphome compile /config/config.yaml
```

Minimal test YAML (`test-config.yaml`):

```yaml
esphome:
  name: sma-test

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
  baud_rate: 0

api:
ota:
  - platform: esphome

wifi:
  ssid: "test"
  password: "test1234"

external_components:
  - source:
      type: local
      path: /local_component/components
    components: [sma_bluetooth]

sma_bluetooth:
```

Then compile:

```bash
docker run --rm \
  -v $(pwd)/test-config.yaml:/config/test-config.yaml \
  -v $(pwd):/local_component \
  ghcr.io/esphome/esphome compile /config/test-config.yaml
```

No need to push — changes are picked up directly from your working directory.

### ESPHome compile test (local install)

If you have ESPHome installed locally:

```bash
esphome compile test-config.yaml
```

### Verify generated defines

After a successful ESPHome compile, check that entity metadata defines are present:

```bash
grep -E 'SMA_(DC|UOM|ICON)_IDX' .esphome/build/*/src/esphome/core/defines.h
```

You should see lines like:

```
#define SMA_DC_IDX_POWER 1
#define SMA_UOM_IDX_W 1
#define SMA_ICON_IDX_FLASH 1
```

## Key Design Notes

- **Thread safety**: `App.register_sensor()` is NOT thread-safe. The BT task (FreeRTOS, core 0) sets a flag; the main ESPHome loop (core 1) creates and registers sensors.
- **Entity metadata**: `device_class`, `unit_of_measurement`, and `icon` have no runtime C++ setters. They're packed into a `entity_fields` bitfield via `configure_entity_()`. The Python `__init__.py` registers strings via `EntityStringPool` which generates PROGMEM lookup tables in `main.cpp`.
- **StaticVector capacity**: ESPHome uses `StaticVector<T, N>` with compile-time capacity. `push_back()` silently drops items when full. The `__init__.py` inflates capacity via `CORE.register_platform_component()`.
- **NVS persistence**: Discovered inverter MACs are cached to NVS flash so sensors can be pre-created during `setup()` before the API handshake. Device names are derived from MAC addresses (always stable).

## Code Style

- Follow ESPHome C++ conventions (2-space indent, `snake_case_`, trailing underscore for members)
- Keep `sma_protocol.h` header-only (inline functions, constexpr, static tables)
- Log levels: `ESP_LOGD` for routine operations, `ESP_LOGI` for significant events (setup, first discovery), `ESP_LOGW`/`ESP_LOGE` for errors
