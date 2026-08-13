#include "irremote_hitachi.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace irremote_hitachi {

static const char *const TAG = "irremote_hitachi.climate";

void IRRemoteHitachiClimate::setup() {
  this->ac_.begin();
  this->ac_.stateReset();

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 24.0f;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  if (this->sensor_ != nullptr) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  }

  this->publish_state();
}

void IRRemoteHitachiClimate::dump_config() {
  LOG_CLIMATE("IRremoteESP8266 Hitachi AC344", "Hitachi AC", this);
  ESP_LOGCONFIG(TAG, "  Protocol: HITACHI_AC344");
  LOG_SENSOR("  ", "Temperature Sensor", this->sensor_);
}

climate::ClimateTraits IRRemoteHitachiClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  });
  traits.set_visual_min_temperature(kHitachiAc344MinTemp);
  traits.set_visual_max_temperature(kHitachiAc344MaxTemp);
  traits.set_visual_target_temperature_step(1.0f);
  if (this->sensor_ != nullptr)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  return traits;
}

void IRRemoteHitachiClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  this->transmit_state_();
  this->publish_state();
}

void IRRemoteHitachiClimate::transmit_state_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->ac_.setMode(kHitachiAc344Cool);
      this->ac_.on();
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->ac_.setMode(kHitachiAc344Heat);
      this->ac_.on();
      break;
    case climate::CLIMATE_MODE_DRY:
      this->ac_.setMode(kHitachiAc344Dry);
      this->ac_.on();
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->ac_.setMode(kHitachiAc344Fan);
      this->ac_.on();
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      this->ac_.off();
      break;
  }

  if (!std::isnan(this->target_temperature)) {
    auto temperature = static_cast<uint8_t>(std::lround(this->target_temperature));
    temperature = std::max<uint8_t>(temperature, kHitachiAc344MinTemp);
    temperature = std::min<uint8_t>(temperature, kHitachiAc344MaxTemp);
    this->target_temperature = temperature;
    this->ac_.setTemp(temperature);
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      this->ac_.setFan(kHitachiAc344FanLow);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      this->ac_.setFan(kHitachiAc344FanMedium);
      break;
    case climate::CLIMATE_FAN_HIGH:
      this->ac_.setFan(kHitachiAc344FanHigh);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      this->ac_.setFan(kHitachiAc344FanAuto);
      break;
  }

  this->ac_.setSwingV(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);

  this->ac_.send();
}

}  // namespace irremote_hitachi
}  // namespace esphome
