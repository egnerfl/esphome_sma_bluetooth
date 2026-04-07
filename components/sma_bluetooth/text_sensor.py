import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import sma_bluetooth_ns, SmaBluetoothHub, CONF_SMA_BLUETOOTH_ID

SmaInverterDevice = sma_bluetooth_ns.class_("SmaInverterDevice")

CONF_MAC_ADDRESS = "mac_address"
CONF_INVERTER_STATUS = "inverter_status"
CONF_SERIAL_NUMBER = "serial_number"
CONF_SOFTWARE_VERSION = "software_version"
CONF_DEVICE_TYPE = "device_type"
CONF_DEVICE_CLASS = "device_class"
CONF_INVERTER_TIME = "inverter_time"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmaInverterDevice),
        cv.GenerateID(CONF_SMA_BLUETOOTH_ID): cv.use_id(SmaBluetoothHub),
        cv.Required(CONF_MAC_ADDRESS): cv.string,
        cv.Optional(CONF_INVERTER_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_DEVICE_TYPE): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_DEVICE_CLASS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_INVERTER_TIME): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMA_BLUETOOTH_ID])

    # Find existing device by MAC or create new
    # For now, we create a new instance — the hub will deduplicate by MAC
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_mac_address(config[CONF_MAC_ADDRESS]))
    cg.add(hub.register_device(var))

    if CONF_INVERTER_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_INVERTER_STATUS])
        cg.add(var.set_inverter_status_sensor(sens))

    if CONF_SERIAL_NUMBER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])
        cg.add(var.set_serial_number_sensor(sens))

    if CONF_SOFTWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION])
        cg.add(var.set_software_version_sensor(sens))

    if CONF_DEVICE_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_TYPE])
        cg.add(var.set_device_type_sensor(sens))

    if CONF_DEVICE_CLASS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_CLASS])
        cg.add(var.set_device_class_sensor(sens))

    if CONF_INVERTER_TIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_INVERTER_TIME])
        cg.add(var.set_inverter_time_sensor(sens))
