import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.components import custom_modbus

DEPENDENCIES = ["custom_modbus"]
AUTO_LOAD = ["binary_sensor", "sensor", "switch", "text_sensor", "button"]
MULTI_CONF = True

CONF_YUHUI_MPPT_ID = "yuhui_mppt_id"
CONF_RX_TIMEOUT = "rx_timeout"
CONF_DEVICE_ADDRESS = "device_address"
CONF_RETRY_COUNT = "retry_count"
CONF_POLL_FULL_STATUS = "poll_full_status"
CONF_CLOCK_BROADCAST = "clock_broadcast"

yuhui_mppt_ns = cg.esphome_ns.namespace("yuhui")
YuhuiMPPT = yuhui_mppt_ns.class_(
    "YuhuiMPPT", cg.PollingComponent, custom_modbus.ModbusDevice
)

YUHUI_MPPT_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_YUHUI_MPPT_ID): cv.use_id(YuhuiMPPT),
    }
)


def _validate_update_interval(config):
    """协议规定上位机两次查询之间必须间隔 1 秒以上。"""
    interval_ms = config[CONF_UPDATE_INTERVAL].total_milliseconds
    if interval_ms < 1000:
        raise cv.Invalid(
            f"update_interval must be at least 1s, got {interval_ms} ms. "
            "The MPPT protocol requires >= 1s between two queries."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(YuhuiMPPT),
            cv.Optional(
                CONF_RX_TIMEOUT, default="300ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DEVICE_ADDRESS, default=0x01): cv.hex_uint8_t,
            cv.Optional(CONF_RETRY_COUNT, default=2): cv.int_range(min=0, max=5),
            cv.Optional(CONF_POLL_FULL_STATUS, default=False): cv.boolean,
            cv.Optional(CONF_CLOCK_BROADCAST, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(custom_modbus.modbus_device_schema(0x01)),
    _validate_update_interval,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await custom_modbus.register_modbus_device(var, config)

    cg.add(var.set_device_address(config[CONF_DEVICE_ADDRESS]))
    cg.add(var.set_rx_timeout(config[CONF_RX_TIMEOUT]))
    cg.add(var.set_retry_count(config[CONF_RETRY_COUNT]))
    cg.add(var.set_poll_full_status(config[CONF_POLL_FULL_STATUS]))
    cg.add(var.set_clock_broadcast(config[CONF_CLOCK_BROADCAST]))
