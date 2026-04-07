# ESPHome SMA Bluetooth Component

ESPHome external component for reading data from SMA solar inverters via Bluetooth Classic. Supports **multiple inverters** on a single ESP32 with **autodiscovery** — each inverter appears as a **separate sub-device** in Home Assistant.

## Features

- **Multi-inverter support** — connect to 1-10+ SMA inverters sequentially from one ESP32
- **Autodiscovery** — automatically finds SMA inverters via Bluetooth Classic GAP scanning
- **Sub-devices** — each inverter appears as its own device in Home Assistant (requires ESPHome 2025.7.0+)
- **Persistent discovery** — discovered inverter MACs are cached in NVS flash and survive reboots (including overnight when inverters are off)
- **Smart password handling** — configurable default password list, per-inverter overrides, auto-caching of working passwords
- **Full sensor set** — AC/DC power, voltage, current, frequency, energy, temperature, status
- **Auto-sensor creation** — discovered inverters automatically get all sensors named by serial number
- **Zero-config mode** — just add `sma_bluetooth:` and it works

## Requirements

- **ESP32 (original only)** — ESP32-S2/S3/C3/C6 do NOT support Bluetooth Classic
- **ESP-IDF framework** (not Arduino)
- **ESPHome 2025.7.0+** (for sub-device support)
- **Home Assistant 2025.7.0+** (for sub-device display)

## Quick Start

### Plug and Play (zero config)

```yaml
external_components:
  - source: github://egnerfl/esphome_sma_bluetooth
    components: [sma_bluetooth]

sma_bluetooth:
```

This will:
1. Scan for all nearby SMA inverters
2. Try default passwords ("0000", "1111")
3. Auto-create all sensors per inverter (named "SMA {serial} {metric}")
4. Register each inverter as a separate sub-device in Home Assistant
5. Cache discovered MACs to flash so sensors persist across reboots

### With Inverter Config

```yaml
sma_bluetooth:
  passwords:
    - "0000"
    - "1111"
  inverters:
    - mac_address: "00:80:25:AA:BB:CC"
      name: "East Roof"
    - mac_address: "00:80:25:DD:EE:FF"
      name: "West Roof"
    - mac_address: "00:80:25:11:22:33"
      name: "Garage"
      password: "1234"
```

When `inverters` is set, only those MAC addresses are accepted. Per-inverter `password` overrides the default list.

### Manual Mode (explicit sensors)

```yaml
sma_bluetooth:
  id: sma_hub
  discovery: false

sensor:
  - platform: sma_bluetooth
    sma_bluetooth_id: sma_hub
    mac_address: "00:80:25:AA:BB:CC"
    password: "0000"
    update_interval: 60s
    pv1:
      voltage:
        name: "DC Voltage"
      active_power:
        name: "DC Power"
    frequency:
      name: "Grid Frequency"
    energy_production_day:
      name: "Daily Energy"
    total_energy_production:
      name: "Total Energy"
```

See `examples/` for more configurations.

## ESP32 Configuration

Your YAML must use the ESP-IDF framework (BT Classic sdkconfig options are set automatically by the component):

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
```

## Hub Configuration

| Option | Default | Description |
|--------|---------|-------------|
| `discovery` | `true` | Enable BT Classic autodiscovery |
| `passwords` | `["0000", "1111"]` | Default passwords to try |
| `inverters` | (empty) | List of known inverters (MAC, name, password) |
| `query_delay` | `200ms` | Delay between data queries to one inverter |
| `inter_inverter_delay` | `5s` | Delay between polling different inverters |
| `max_devices` | `10` | Max number of inverters (reserves memory for sensors) |

### `max_devices`

Controls how much memory is pre-allocated for runtime-created sensors. Each inverter creates ~22 sensors, 6 text sensors, and 1 binary sensor. The default of 10 is sufficient for most installations. Increase if you have more inverters:

```yaml
sma_bluetooth:
  max_devices: 20
```

## Available Sensors

### Sensor Platform (numeric values)

| Key | Unit | Description |
|-----|------|-------------|
| `phase_a/b/c.voltage` | V | AC voltage per phase |
| `phase_a/b/c.current` | A | AC current per phase |
| `phase_a/b/c.active_power` | kW | AC power per phase |
| `pv1/pv2.voltage` | V | DC voltage per MPPT |
| `pv1/pv2.current` | A | DC current per MPPT |
| `pv1/pv2.active_power` | kW | DC power per MPPT |
| `frequency` | Hz | Grid frequency |
| `energy_production_day` | kWh | Daily energy yield |
| `total_energy_production` | kWh | Lifetime energy yield |
| `inverter_module_temp` | °C | Module temperature |
| `bt_signal_strength` | % | Bluetooth signal |
| `today_generation_time` | h | Daily operation time |
| `total_generation_time` | h | Lifetime feed-in time |

### Text Sensor Platform

| Key | Description |
|-----|-------------|
| `inverter_status` | Operating status |
| `serial_number` | Inverter serial number |
| `software_version` | Firmware version |
| `device_type` | Device type label |
| `device_class` | Device class |
| `inverter_time` | Inverter clock |

### Binary Sensor Platform

| Key | Description |
|-----|-------------|
| `grid_relay` | Grid relay state (on = closed) |

## How It Works

The ESP32 can only maintain one Bluetooth Classic SPP connection at a time. This component polls inverters **sequentially**:

```
[Discover] → [Connect Inv1] → [Read] → [Disconnect] → [Connect Inv2] → ...
```

A full cycle for 3 inverters takes ~45-60 seconds.

### Discovery and Persistence

1. **First boot**: The component runs a GAP discovery scan, finds SMA inverters, connects to each, and creates sensors dynamically.
2. **NVS caching**: After discovery, inverter MACs and names are saved to flash (NVS). The ESP32 then reboots once so Home Assistant sees all sensors in the initial entity list.
3. **Subsequent boots**: Cached inverters are pre-created during `setup()` before the API connects, so sensors are immediately available to Home Assistant — even if inverters are offline (e.g. nighttime).
4. **Periodic re-discovery**: Every 5 minutes, the component scans for new inverters. Newly found inverters trigger a one-time reboot to register with HA.

### Sub-Devices

Each inverter is registered as a separate ESPHome `Device` with a stable ID derived from its MAC address. In Home Assistant, this means:
- Each inverter appears as its own device under the ESP32 hub
- Sensors are grouped per inverter
- The parent ESP32 device shows as the "via device"

## Development

### Run protocol tests (no hardware needed)

```bash
pio test -e native
```

### Compile for ESP32

```bash
esphome compile examples/minimal.yaml
```

## Credits

Based on protocol implementations from:
- [keerekeerweere/esphome_smabluetooth](https://github.com/keerekeerweere/esphome_smabluetooth)
- [darrylb123/ESP32_SMA-Inverter-MQTT](https://github.com/darrylb123/ESP32_SMA-Inverter-MQTT)
- [stuartpittaway/SMASolarMQTT](https://github.com/stuartpittaway/SMASolarMQTT)

## License

MIT
