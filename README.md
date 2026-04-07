# ESPHome SMA Bluetooth Component

ESPHome external component for reading data from SMA solar inverters via Bluetooth Classic. Supports **multiple inverters** on a single ESP32 with **autodiscovery**.

## Features

- **Multi-inverter support** — connect to 1-10+ SMA inverters sequentially from one ESP32
- **Autodiscovery** — automatically finds SMA inverters via Bluetooth Classic GAP scanning
- **Smart password handling** — configurable default password list, per-inverter overrides, auto-caching
- **Full sensor set** — AC/DC power, voltage, current, frequency, energy, temperature, status
- **Auto-sensor creation** — discovered inverters automatically get all sensors named by serial number
- **Zero-config mode** — just add `sma_bluetooth:` and it works

## Requirements

- **ESP32 (original only)** — ESP32-S2/S3/C3/C6 do NOT support Bluetooth Classic
- **ESP-IDF framework** (not Arduino)
- **ESPHome 2024.2+**

## Quick Start

### Plug and Play (zero config)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/egnerfl/esphome_sma_bluetooth
    components: [sma_bluetooth]

sma_bluetooth:
```

This will:
1. Scan for all nearby SMA inverters
2. Try default passwords ("0000", "1111")
3. Auto-create all sensors per inverter (named "SMA {serial} {metric}")

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

Your YAML must include ESP-IDF with Bluetooth Classic enabled:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BT_ENABLED: "y"
      CONFIG_BT_BLUEDROID_ENABLED: "y"
      CONFIG_BT_CLASSIC_ENABLED: "y"
      CONFIG_BT_SPP_ENABLED: "y"
      CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY: "y"
      CONFIG_BT_BLE_ENABLED: "n"
```

## Hub Configuration

| Option | Default | Description |
|--------|---------|-------------|
| `discovery` | `true` | Enable BT Classic autodiscovery |
| `passwords` | `["0000", "1111"]` | Default passwords to try |
| `inverters` | (empty) | List of known inverters (MAC, name, password) |
| `query_delay` | `200ms` | Delay between data queries to one inverter |
| `inter_inverter_delay` | `5s` | Delay between polling different inverters |

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
