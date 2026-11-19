import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import yuhui_mppt_ns, YuhuiMPPT, CONF_YUHUI_MPPT_ID, YUHUI_MPPT_COMPONENT_SCHEMA

DEPENDENCIES = ["yuhui_mppt"]

CONF_RUNNING_STATUS = "running_status"
CONF_BATTERY_STATUS = "battery_status"
CONF_FAN_STATUS = "fan_status"
CONF_TEMPERATURE_STATUS = "temperature_status"
CONF_DC_OUTPUT_STATUS = "dc_output_status"
CONF_INTERNAL_TEMPERATURE1_STATUS = "internal_temperature1_status"
CONF_INTERNAL_TEMPERATURE2_STATUS = "internal_temperature2_status"
CONF_EXTERNAL_TEMPERATURE1_STATUS = "external_temperature1_status"
# 下面两项只有 0xB1 / 0xB2 帧里才有
CONF_BATTERY_TYPE = "battery_type"
CONF_LAST_ERROR = "last_error"

ICON_FLASH = "mdi:flash"
ICON_BATTERY_ALERT_VARIANT = "mdi:battery-alert-variant"
ICON_FAN = "mdi:fan"
ICON_THERMOMETER_ALERT = "mdi:thermometer-alert"
ICON_DC_OUTPUT = "mdi:export"
ICON_ALERT_CIRCLE = "mdi:alert-circle"
ICON_BATTERY = "mdi:car-battery"
ICON_TERMINAL = "mdi:console"

CONFIG_SCHEMA = YUHUI_MPPT_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_RUNNING_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_ALERT_CIRCLE
        ),
        cv.Optional(CONF_BATTERY_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_BATTERY_ALERT_VARIANT
        ),
        cv.Optional(CONF_FAN_STATUS): text_sensor.text_sensor_schema(icon=ICON_FAN),
        cv.Optional(CONF_TEMPERATURE_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_THERMOMETER_ALERT
        ),
        cv.Optional(CONF_DC_OUTPUT_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_DC_OUTPUT
        ),
        cv.Optional(CONF_INTERNAL_TEMPERATURE1_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_THERMOMETER_ALERT
        ),
        cv.Optional(CONF_INTERNAL_TEMPERATURE2_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_THERMOMETER_ALERT
        ),
        cv.Optional(CONF_EXTERNAL_TEMPERATURE1_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_THERMOMETER_ALERT
        ),
        cv.Optional(CONF_BATTERY_TYPE): text_sensor.text_sensor_schema(
            icon=ICON_BATTERY
        ),
        cv.Optional(CONF_LAST_ERROR): text_sensor.text_sensor_schema(
            icon=ICON_TERMINAL
        ),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_YUHUI_MPPT_ID])

    if CONF_RUNNING_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_RUNNING_STATUS])
        cg.add(var.set_running_status_text_sensor(sens))
    if CONF_BATTERY_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BATTERY_STATUS])
        cg.add(var.set_battery_status_text_sensor(sens))
    if CONF_FAN_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FAN_STATUS])
        cg.add(var.set_fan_status_text_sensor(sens))
    if CONF_TEMPERATURE_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_TEMPERATURE_STATUS])
        cg.add(var.set_temperature_status_text_sensor(sens))
    if CONF_DC_OUTPUT_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DC_OUTPUT_STATUS])
        cg.add(var.set_dc_output_status_text_sensor(sens))
    if CONF_INTERNAL_TEMPERATURE1_STATUS in config:
        sens = await text_sensor.new_text_sensor(
            config[CONF_INTERNAL_TEMPERATURE1_STATUS]
        )
        cg.add(var.set_internal_temperature1_status_text_sensor(sens))
    if CONF_INTERNAL_TEMPERATURE2_STATUS in config:
        sens = await text_sensor.new_text_sensor(
            config[CONF_INTERNAL_TEMPERATURE2_STATUS]
        )
        cg.add(var.set_internal_temperature2_status_text_sensor(sens))
    if CONF_EXTERNAL_TEMPERATURE1_STATUS in config:
        sens = await text_sensor.new_text_sensor(
            config[CONF_EXTERNAL_TEMPERATURE1_STATUS]
        )
        cg.add(var.set_external_temperature1_status_text_sensor(sens))
    if CONF_BATTERY_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BATTERY_TYPE])
        cg.add(var.set_battery_type_text_sensor(sens))
    if CONF_LAST_ERROR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_ERROR])
        cg.add(var.set_error_text_sensor(sens))
