import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

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
    }
).extend(cv.COMPONENT_SCHEMA)


def _parse_mac(mac_str):
    """Parse MAC string into list of 6 ints."""
    parts = mac_str.split(":")
    if len(parts) != 6:
        raise cv.Invalid(f"Invalid MAC address: {mac_str}")
    return [int(p, 16) for p in parts]


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
            mac_bytes = _parse_mac(entry[CONF_MAC_ADDRESS])
            inv_cfg = cg.StructInitializer(
                InverterConfig,
                ("mac", mac_bytes),
                ("mac_string", entry[CONF_MAC_ADDRESS]),
                ("name", entry.get(CONF_NAME, "")),
                ("password", entry.get(CONF_PASSWORD, "")),
            )
            cg.add(var.add_inverter_config(inv_cfg))

    # Required sdkconfig options for BT Classic
    if cg.CORE.using_esp_idf:
        cg.add_define("CONFIG_BT_ENABLED")
        cg.add_define("CONFIG_BT_BLUEDROID_ENABLED")
        cg.add_define("CONFIG_BT_CLASSIC_ENABLED")
        cg.add_define("CONFIG_BT_SPP_ENABLED")
