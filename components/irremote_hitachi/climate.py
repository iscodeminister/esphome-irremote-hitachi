import math

import esphome.codegen as cg
from esphome.components import climate, remote_base, sensor
from esphome.core import EnumValue
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_PROTOCOL, CONF_SENSOR

CONF_POWER_SENSOR = "power_sensor"
CONF_POWER_ON_THRESHOLD = "power_on_threshold"
CONF_POWER_OFF_THRESHOLD = "power_off_threshold"
CONF_POWER_ON_DELAY = "power_on_delay"
CONF_POWER_OFF_DELAY = "power_off_delay"

DEFAULT_POWER_ON_DELAY_MS = 3000
DEFAULT_POWER_OFF_DELAY_MS = 30000

AUTO_LOAD = ["climate", "remote_base", "sensor"]
DEPENDENCIES = ["esp32", "remote_transmitter"]

irremote_hitachi_ns = cg.esphome_ns.namespace("irremote_hitachi")
IRRemoteHitachiClimate = irremote_hitachi_ns.class_(
    "IRRemoteHitachiClimate",
    climate.Climate,
    cg.Component,
    remote_base.RemoteTransmittable,
)
HitachiProtocol = irremote_hitachi_ns.enum("HitachiProtocol")

PROTOCOLS = {
    "HITACHI_AC1": HitachiProtocol.HITACHI_PROTOCOL_AC1,
    "HITACHI_AC344": HitachiProtocol.HITACHI_PROTOCOL_AC344,
}

AC1_MODELS = [
    "R_LT0541_HTA_A",
    "R_LT0541_HTA_B",
]


def validate_power_feedback(config):
    power_options = {
        CONF_POWER_ON_THRESHOLD,
        CONF_POWER_OFF_THRESHOLD,
        CONF_POWER_ON_DELAY,
        CONF_POWER_OFF_DELAY,
    }
    if CONF_POWER_SENSOR not in config:
        if power_options.intersection(config):
            raise cv.Invalid("power feedback options require power_sensor")
        return config

    protocol = (
        config[CONF_PROTOCOL].enum_value
        if isinstance(config[CONF_PROTOCOL], EnumValue)
        else config[CONF_PROTOCOL]
    )
    if protocol is not PROTOCOLS["HITACHI_AC1"] or config[CONF_MODEL] != "R_LT0541_HTA_B":
        raise cv.Invalid(
            "power feedback is supported only for HITACHI_AC1 R_LT0541_HTA_B"
        )

    missing = [
        key
        for key in (CONF_POWER_ON_THRESHOLD, CONF_POWER_OFF_THRESHOLD)
        if key not in config
    ]
    if missing:
        raise cv.Invalid(
            f"power_sensor requires {', '.join(missing)}"
        )

    if not math.isfinite(config[CONF_POWER_ON_THRESHOLD]) or not math.isfinite(
        config[CONF_POWER_OFF_THRESHOLD]
    ):
        raise cv.Invalid("power thresholds must be finite")

    if config[CONF_POWER_ON_THRESHOLD] <= config[CONF_POWER_OFF_THRESHOLD]:
        raise cv.Invalid("power_on_threshold must be greater than power_off_threshold")
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(IRRemoteHitachiClimate)
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_POWER_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_POWER_ON_THRESHOLD): cv.float_range(min=0),
            cv.Optional(CONF_POWER_OFF_THRESHOLD): cv.float_range(min=0),
            cv.Optional(CONF_POWER_ON_DELAY): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_POWER_OFF_DELAY): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PROTOCOL, default="HITACHI_AC344"): cv.enum(
                PROTOCOLS, upper=True
            ),
            cv.Optional(CONF_MODEL, default="R_LT0541_HTA_B"): cv.one_of(
                *AC1_MODELS, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA),
    validate_power_feedback,
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await remote_base.register_transmittable(var, config)

    cg.add(var.set_protocol(config[CONF_PROTOCOL]))
    cg.add(var.set_ac1_model_b(config[CONF_MODEL] == "R_LT0541_HTA_B"))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_POWER_SENSOR in config:
        power_sensor = await cg.get_variable(config[CONF_POWER_SENSOR])
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_power_on_threshold(config[CONF_POWER_ON_THRESHOLD]))
        cg.add(var.set_power_off_threshold(config[CONF_POWER_OFF_THRESHOLD]))
        cg.add(
            var.set_power_on_delay(
                config[CONF_POWER_ON_DELAY].total_milliseconds
                if CONF_POWER_ON_DELAY in config
                else DEFAULT_POWER_ON_DELAY_MS
            )
        )
        cg.add(
            var.set_power_off_delay(
                config[CONF_POWER_OFF_DELAY].total_milliseconds
                if CONF_POWER_OFF_DELAY in config
                else DEFAULT_POWER_OFF_DELAY_MS
            )
        )

    cg.add_library("crankyoldgit/IRremoteESP8266", "2.9.0")
