import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import CONF_YUHUI_MPPT_ID, YuhuiMPPT

CODEOWNERS = ["@Depressboy"]

CONF_PV_VOLTAGE = "pv_voltage"
CONF_BATTERY_VOLTAGE = "battery_voltage"
CONF_CHARGING_CURRENT = "charging_current"
CONF_INTERNAL_TEMPERATURE = "internal_temperature"
CONF_EXTERNAL_TEMPERATURE = "external_temperature"
CONF_DAILY_ENERGY = "daily_energy"
CONF_TOTAL_ENERGY = "total_energy"
CONF_CHARGING_POWER = "charging_power"
# 以下三项只有 0xB1 全量帧（或 0xB2 设置帧）里才有
CONF_RATED_VOLTAGE = "rated_voltage"
CONF_MAX_CHARGE_CURRENT = "max_charge_current"

ICON_PV_VOLTAGE = "mdi:solar-power"
ICON_BATTERY_VOLTAGE = "mdi:battery"
ICON_CHARGING_CURRENT = "mdi:current-dc"
ICON_INTERNAL_TEMPERATURE = "mdi:thermometer"
ICON_EXTERNAL_TEMPERATURE = "mdi:thermometer-lines"
ICON_DAILY_ENERGY = "mdi:lightning-bolt-outline"
ICON_TOTAL_ENERGY = "mdi:lightning-bolt-outline"
ICON_CHARGING_POWER = "mdi:flash"
ICON_RATED_VOLTAGE = "mdi:car-battery"
ICON_MAX_CHARGE_CURRENT = "mdi:current-dc"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_YUHUI_MPPT_ID): cv.use_id(YuhuiMPPT),
        cv.Optional(CONF_PV_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_PV_VOLTAGE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_BATTERY_VOLTAGE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGING_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon=ICON_CHARGING_CURRENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_INTERNAL_TEMPERATURE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_EXTERNAL_TEMPERATURE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DAILY_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon=ICON_DAILY_ENERGY,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_TOTAL_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon=ICON_TOTAL_ENERGY,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_CHARGING_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            icon=ICON_CHARGING_POWER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RATED_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_RATED_VOLTAGE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MAX_CHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon=ICON_MAX_CHARGE_CURRENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_YUHUI_MPPT_ID])

    if CONF_PV_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_PV_VOLTAGE])
        cg.add(var.set_pv_voltage_sensor(sens))
    if CONF_BATTERY_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(var.set_battery_voltage_sensor(sens))
    if CONF_CHARGING_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CHARGING_CURRENT])
        cg.add(var.set_charging_current_sensor(sens))
    if CONF_INTERNAL_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_INTERNAL_TEMPERATURE])
        cg.add(var.set_internal_temperature_sensor(sens))
    if CONF_EXTERNAL_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_EXTERNAL_TEMPERATURE])
        cg.add(var.set_external_temperature_sensor(sens))
    if CONF_DAILY_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_DAILY_ENERGY])
        cg.add(var.set_daily_energy_sensor(sens))
    if CONF_TOTAL_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_ENERGY])
        cg.add(var.set_total_energy_sensor(sens))
    if CONF_CHARGING_POWER in config:
        sens = await sensor.new_sensor(config[CONF_CHARGING_POWER])
        cg.add(var.set_charging_power_sensor(sens))
    if CONF_RATED_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_RATED_VOLTAGE])
        cg.add(var.set_rated_voltage_sensor(sens))
    if CONF_MAX_CHARGE_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_MAX_CHARGE_CURRENT])
        cg.add(var.set_max_charge_current_sensor(sens))
