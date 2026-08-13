import esphome.codegen as cg
from esphome.components import climate, sensor
import esphome.config_validation as cv
from esphome.const import CONF_PIN, CONF_SENSOR
from esphome import pins

AUTO_LOAD = ["climate", "sensor"]
DEPENDENCIES = ["esp32"]

irremote_hitachi_ns = cg.esphome_ns.namespace("irremote_hitachi")
IRRemoteHitachiClimate = irremote_hitachi_ns.class_(
    "IRRemoteHitachiClimate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(IRRemoteHitachiClimate)
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(config):
    var = await climate.new_climate(config, config[CONF_PIN])
    await cg.register_component(var, config)

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    cg.add_library("crankyoldgit/IRremoteESP8266", "2.9.0")
