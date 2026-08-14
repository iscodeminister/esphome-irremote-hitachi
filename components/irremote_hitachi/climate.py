import esphome.codegen as cg
from esphome.components import climate, remote_base, sensor
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_PROTOCOL, CONF_SENSOR

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

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(IRRemoteHitachiClimate)
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
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

    cg.add_library("crankyoldgit/IRremoteESP8266", "2.9.0")
