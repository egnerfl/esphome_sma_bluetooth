import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.core.entity_helpers import (
    register_device_class,
    register_unit_of_measurement,
    register_icon,
)

CODEOWNERS = ["@egnerfl"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]
MULTI_CONF = False  # One BT stack per ESP32

sma_bluetooth_ns = cg.esphome_ns.namespace("sma_bluetooth")
SmaBluetoothHub = sma_bluetooth_ns.class_("SmaBluetoothHub", cg.Component)

CONF_SMA_BLUETOOTH_ID = "sma_bluetooth_id"
CONF_DISCOVERY = "discovery"
CONF_QUERY_DELAY = "query_delay"
CONF_INTER_INVERTER_DELAY = "inter_inverter_delay"
CONF_PASSWORDS = "passwords"
CONF_INVERTERS = "inverters"
CONF_MAC_ADDRESS = "mac_address"
CONF_PASSWORD = "password"
CONF_NAME = "name"
CONF_MAX_DEVICES = "max_devices"

InverterConfig = sma_bluetooth_ns.struct("InverterConfig")

INVERTER_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MAC_ADDRESS): cv.string,
        cv.Optional(CONF_NAME, default=""): cv.string,
        cv.Optional(CONF_PASSWORD, default=""): cv.string,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmaBluetoothHub),
        cv.Optional(CONF_DISCOVERY, default=True): cv.boolean,
        cv.Optional(CONF_QUERY_DELAY, default="200ms"): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_INTER_INVERTER_DELAY, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PASSWORDS): cv.ensure_list(cv.string),
        cv.Optional(CONF_INVERTERS): cv.ensure_list(INVERTER_CONFIG_SCHEMA),
        cv.Optional(CONF_MAX_DEVICES, default=10): cv.int_range(min=1, max=50),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_discovery_enabled(config[CONF_DISCOVERY]))
    cg.add(
        var.set_query_delay_ms(config[CONF_QUERY_DELAY].total_milliseconds)
    )
    cg.add(
        var.set_inter_inverter_delay_ms(
            config[CONF_INTER_INVERTER_DELAY].total_milliseconds
        )
    )

    if CONF_PASSWORDS in config:
        for pw in config[CONF_PASSWORDS]:
            cg.add(var.add_password(pw))

    if CONF_INVERTERS in config:
        for entry in config[CONF_INVERTERS]:
            inv_cfg = cg.StructInitializer(
                InverterConfig,
                ("mac_string", entry[CONF_MAC_ADDRESS]),
                ("name", entry.get(CONF_NAME, "")),
                ("password", entry.get(CONF_PASSWORD, "")),
            )
            cg.add(var.add_inverter_config(inv_cfg))

    # Add BT Classic sdkconfig options for ESP-IDF
    from esphome.components.esp32 import add_idf_sdkconfig_option
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_CLASSIC_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_SPP_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_ENABLED", False)

    # Enable sub-device support so each inverter appears as a separate
    # device in Home Assistant (requires ESPHome >= 2025.7.0).
    max_devices = config[CONF_MAX_DEVICES]
    cg.add_define("USE_DEVICES")
    cg.add_define("ESPHOME_DEVICE_COUNT", max_devices + 5)

    # Reserve StaticVector capacity for runtime-created entities.
    # Autodiscovery creates ~22 sensors + 6 text + 1 binary per inverter.
    for _ in range(22 * max_devices):
        CORE.register_platform_component("sensor", None)
    for _ in range(6 * max_devices):
        CORE.register_platform_component("text_sensor", None)
    for _ in range(1 * max_devices):
        CORE.register_platform_component("binary_sensor", None)

    # Register entity metadata strings for runtime-created sensors.
    # These get added to ESPHome's PROGMEM lookup tables; indices are emitted
    # as C++ #define constants so create_auto_sensors() can pack them into
    # the entity_fields bitfield of configure_entity_().

    # Device classes (sensor + binary_sensor share the same lookup table)
    cg.add_define("SMA_DC_IDX_POWER", register_device_class("power"))
    cg.add_define("SMA_DC_IDX_ENERGY", register_device_class("energy"))
    cg.add_define("SMA_DC_IDX_VOLTAGE", register_device_class("voltage"))
    cg.add_define("SMA_DC_IDX_CURRENT", register_device_class("current"))
    cg.add_define("SMA_DC_IDX_TEMPERATURE", register_device_class("temperature"))
    cg.add_define("SMA_DC_IDX_FREQUENCY", register_device_class("frequency"))
    cg.add_define("SMA_DC_IDX_SIGNAL_STRENGTH", register_device_class("signal_strength"))
    cg.add_define("SMA_DC_IDX_DURATION", register_device_class("duration"))

    # Units of measurement
    cg.add_define("SMA_UOM_IDX_W", register_unit_of_measurement("W"))
    cg.add_define("SMA_UOM_IDX_KWH", register_unit_of_measurement("kWh"))
    cg.add_define("SMA_UOM_IDX_V", register_unit_of_measurement("V"))
    cg.add_define("SMA_UOM_IDX_A", register_unit_of_measurement("A"))
    cg.add_define("SMA_UOM_IDX_HZ", register_unit_of_measurement("Hz"))
    cg.add_define("SMA_UOM_IDX_C", register_unit_of_measurement("\u00b0C"))
    cg.add_define("SMA_UOM_IDX_PERCENT", register_unit_of_measurement("%"))
    cg.add_define("SMA_UOM_IDX_H", register_unit_of_measurement("h"))

    # Icons
    cg.add_define("SMA_ICON_IDX_FLASH", register_icon("mdi:flash"))
    cg.add_define("SMA_ICON_IDX_SINE", register_icon("mdi:sine-wave"))
    cg.add_define("SMA_ICON_IDX_THERMO", register_icon("mdi:thermometer"))
    cg.add_define("SMA_ICON_IDX_BT", register_icon("mdi:bluetooth"))
    cg.add_define("SMA_ICON_IDX_TIMER", register_icon("mdi:timer-outline"))
    cg.add_define("SMA_ICON_IDX_RELAY", register_icon("mdi:electric-switch"))
    cg.add_define("SMA_ICON_IDX_SOLAR", register_icon("mdi:solar-power"))

    # Enable entity metadata features
    cg.add_define("USE_ENTITY_DEVICE_CLASS")
    cg.add_define("USE_ENTITY_UNIT_OF_MEASUREMENT")
    cg.add_define("USE_ENTITY_ICON")
