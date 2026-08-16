import math
import unittest

import esphome.config_validation as cv

from components.irremote_hitachi import climate


class PowerFeedbackSchemaTest(unittest.TestCase):
    def base_config(self):
        return {
            climate.CONF_PROTOCOL: climate.PROTOCOLS["HITACHI_AC1"],
            climate.CONF_MODEL: "R_LT0541_HTA_B",
        }

    def feedback_config(self):
        return {
            **self.base_config(),
            climate.CONF_POWER_SENSOR: object(),
            climate.CONF_POWER_ON_THRESHOLD: 15.0,
            climate.CONF_POWER_OFF_THRESHOLD: 5.0,
        }

    def test_valid_feedback_config(self):
        config = self.feedback_config()
        self.assertIs(climate.validate_power_feedback(config), config)

    def test_sensor_requires_both_thresholds(self):
        config = self.base_config()
        config[climate.CONF_POWER_SENSOR] = object()
        config[climate.CONF_POWER_ON_THRESHOLD] = 15.0
        with self.assertRaises(cv.Invalid):
            climate.validate_power_feedback(config)

    def test_feedback_options_require_sensor(self):
        for option, value in (
            (climate.CONF_POWER_ON_THRESHOLD, 15.0),
            (climate.CONF_POWER_OFF_THRESHOLD, 5.0),
            (climate.CONF_POWER_ON_DELAY, object()),
            (climate.CONF_POWER_OFF_DELAY, object()),
        ):
            config = self.base_config()
            config[option] = value
            with self.subTest(option=option), self.assertRaises(cv.Invalid):
                climate.validate_power_feedback(config)

    def test_on_threshold_must_be_greater(self):
        config = self.feedback_config()
        config[climate.CONF_POWER_ON_THRESHOLD] = 5.0
        with self.assertRaises(cv.Invalid):
            climate.validate_power_feedback(config)

    def test_thresholds_must_be_finite(self):
        for option in (
            climate.CONF_POWER_ON_THRESHOLD,
            climate.CONF_POWER_OFF_THRESHOLD,
        ):
            config = self.feedback_config()
            config[option] = math.inf
            with self.subTest(option=option), self.assertRaises(cv.Invalid):
                climate.validate_power_feedback(config)

    def test_feedback_is_limited_to_model_b(self):
        config = self.feedback_config()
        config[climate.CONF_MODEL] = "R_LT0541_HTA_A"
        with self.assertRaises(cv.Invalid):
            climate.validate_power_feedback(config)

        config = self.feedback_config()
        config[climate.CONF_PROTOCOL] = climate.PROTOCOLS["HITACHI_AC344"]
        with self.assertRaises(cv.Invalid):
            climate.validate_power_feedback(config)


if __name__ == "__main__":
    unittest.main()
