import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_TIMESTAMP,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_KILOWATT,
    UNIT_PERCENT,
    UNIT_KILOWATT_HOURS,
    UNIT_HOUR,
)
from . import sma_bluetooth_ns, SmaBluetoothHub, CONF_SMA_BLUETOOTH_ID

SmaInverterDevice = sma_bluetooth_ns.class_("SmaInverterDevice")

CONF_MAC_ADDRESS = "mac_address"
CONF_PASSWORD = "password"
CONF_NAME_PREFIX = "name"

CONF_PHASE_A = "phase_a"
CONF_PHASE_B = "phase_b"
CONF_PHASE_C = "phase_c"
CONF_PV1 = "pv1"
CONF_PV2 = "pv2"

CONF_ENERGY_PRODUCTION_DAY = "energy_production_day"
CONF_TOTAL_ENERGY_PRODUCTION = "total_energy_production"
CONF_INVERTER_MODULE_TEMP = "inverter_module_temp"
CONF_BT_SIGNAL_STRENGTH = "bt_signal_strength"
CONF_TODAY_GENERATION_TIME = "today_generation_time"
CONF_TOTAL_GENERATION_TIME = "total_generation_time"
CONF_WAKEUP_TIME = "wakeup_time"

ICON_CURRENT_DC = "mdi:current-dc"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_SOLAR_POWER = "mdi:solar-power"
ICON_LIGHTNING_BOLT = "mdi:lightning-bolt"
ICON_TIMER = "mdi:timer"
ICON_CLOCK = "mdi:clock"
ICON_SIGNAL = "mdi:signal-distance-variant"
ICON_THERMOMETER = "mdi:thermometer"
ICON_FLASH = "mdi:flash"

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        icon=ICON_SINE_WAVE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        icon="mdi:current-ac",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT,
        icon=ICON_LIGHTNING_BOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PV_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        icon=ICON_SINE_WAVE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        icon=ICON_CURRENT_DC,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT,
        icon=ICON_SOLAR_POWER,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PHASE_SCHEMA = cv.Schema(
    {cv.Optional(s): schema for s, schema in PHASE_SENSORS.items()}
)
PV_SCHEMA = cv.Schema(
    {cv.Optional(s): schema for s, schema in PV_SENSORS.items()}
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmaInverterDevice),
        cv.GenerateID(CONF_SMA_BLUETOOTH_ID): cv.use_id(SmaBluetoothHub),
        cv.Required(CONF_MAC_ADDRESS): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_NAME_PREFIX, default=""): cv.string,
        cv.Optional("update_interval", default="60s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
        cv.Optional(CONF_PV1): PV_SCHEMA,
        cv.Optional(CONF_PV2): PV_SCHEMA,
        cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            icon=ICON_SINE_WAVE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY_PRODUCTION_DAY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon=ICON_FLASH,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_TOTAL_ENERGY_PRODUCTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon=ICON_FLASH,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_INVERTER_MODULE_TEMP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BT_SIGNAL_STRENGTH): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_SIGNAL,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TODAY_GENERATION_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            icon=ICON_TIMER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_TOTAL_GENERATION_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            icon=ICON_TIMER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_WAKEUP_TIME): sensor.sensor_schema(
            icon=ICON_CLOCK,
            device_class=DEVICE_CLASS_TIMESTAMP,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMA_BLUETOOTH_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_mac_address(config[CONF_MAC_ADDRESS]))

    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))

    if config.get(CONF_NAME_PREFIX):
        cg.add(var.set_name_prefix(config[CONF_NAME_PREFIX]))

    cg.add(
        var.set_update_interval_ms(
            config["update_interval"].total_milliseconds
        )
    )

    cg.add(hub.register_device(var))

    # Frequency
    if CONF_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_grid_frequency_sensor(sens))

    # Energy
    if CONF_ENERGY_PRODUCTION_DAY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY_PRODUCTION_DAY])
        cg.add(var.set_today_production_sensor(sens))

    if CONF_TOTAL_ENERGY_PRODUCTION in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_ENERGY_PRODUCTION])
        cg.add(var.set_total_energy_production_sensor(sens))

    # Temperature
    if CONF_INVERTER_MODULE_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_INVERTER_MODULE_TEMP])
        cg.add(var.set_inverter_module_temp_sensor(sens))

    # BT signal
    if CONF_BT_SIGNAL_STRENGTH in config:
        sens = await sensor.new_sensor(config[CONF_BT_SIGNAL_STRENGTH])
        cg.add(var.set_bt_signal_strength_sensor(sens))

    # Generation time
    if CONF_TODAY_GENERATION_TIME in config:
        sens = await sensor.new_sensor(config[CONF_TODAY_GENERATION_TIME])
        cg.add(var.set_today_generation_time_sensor(sens))

    if CONF_TOTAL_GENERATION_TIME in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_GENERATION_TIME])
        cg.add(var.set_total_generation_time_sensor(sens))

    # Wakeup time
    if CONF_WAKEUP_TIME in config:
        sens = await sensor.new_sensor(config[CONF_WAKEUP_TIME])
        cg.add(var.set_wakeup_time_sensor(sens))

    # Phase sensors
    for i, phase in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase not in config:
            continue
        phase_config = config[phase]
        for sensor_type in PHASE_SENSORS:
            if sensor_type in phase_config:
                sens = await sensor.new_sensor(phase_config[sensor_type])
                cg.add(getattr(var, f"set_{sensor_type}_sensor")(i, sens))

    # PV sensors
    for i, pv in enumerate([CONF_PV1, CONF_PV2]):
        if pv not in config:
            continue
        pv_config = config[pv]
        for sensor_type in PV_SENSORS:
            if sensor_type in pv_config:
                sens = await sensor.new_sensor(pv_config[sensor_type])
                cg.add(getattr(var, f"set_{sensor_type}_sensor_pv")(i, sens))
