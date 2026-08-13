#include "irremote_hitachi.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace irremote_hitachi {

static const char *const TAG = "irremote_hitachi.climate";

void IRRemoteHitachiClimate::setup() {
  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    this->ac1_.begin();
    this->ac1_.stateReset();
    this->ac1_.setModel(this->ac1_model_);
  } else {
    this->ac344_.begin();
    this->ac344_.stateReset();
  }

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 24.0f;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    const bool power_on = this->mode != climate::CLIMATE_MODE_OFF;
    const bool swing_on = this->swing_mode == climate::CLIMATE_SWING_VERTICAL;

    this->ac1_.setPower(power_on);
    this->ac1_.setSwingV(swing_on);

    // Initialization is state synchronization, not a remote-control action.
    this->ac1_.setPowerToggle(false);
    this->ac1_.setSwingToggle(false);
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
  LOG_CLIMATE("IRremoteESP8266 Hitachi", "Hitachi AC", this);
  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    ESP_LOGCONFIG(TAG, "  Protocol: HITACHI_AC1");
    ESP_LOGCONFIG(TAG, "  Remote variant: %s",
                  this->ac1_model_ == R_LT0541_HTA_B ? "R_LT0541_HTA_B" : "R_LT0541_HTA_A");
  } else {
    ESP_LOGCONFIG(TAG, "  Protocol: HITACHI_AC344");
  }
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
  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    traits.set_visual_min_temperature(kHitachiAcMinTemp);
    traits.set_visual_max_temperature(kHitachiAcMaxTemp);
  } else {
    traits.set_visual_min_temperature(kHitachiAc344MinTemp);
    traits.set_visual_max_temperature(kHitachiAc344MaxTemp);
  }
  traits.set_visual_target_temperature_step(1.0f);
  if (this->sensor_ != nullptr)
    traits.set_supports_current_temperature(true);
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
  if (this->protocol_ == HITACHI_PROTOCOL_AC1)
    this->transmit_ac1_state_();
  else
    this->transmit_ac344_state_();
}

void IRRemoteHitachiClimate::transmit_ac1_state_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->ac1_.setMode(kHitachiAc1Cool);
      this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->ac1_.setMode(kHitachiAc1Heat);
      this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_DRY:
      this->ac1_.setMode(kHitachiAc1Dry);
      this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->ac1_.setMode(kHitachiAc1Fan);
      this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      this->ac1_.off();
      break;
  }

  if (!std::isnan(this->target_temperature)) {
    auto temperature = static_cast<uint8_t>(std::lround(this->target_temperature));
    temperature = std::max<uint8_t>(temperature, kHitachiAcMinTemp);
    temperature = std::min<uint8_t>(temperature, kHitachiAcMaxTemp);
    this->target_temperature = temperature;
    this->ac1_.setTemp(temperature);
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      this->ac1_.setFan(kHitachiAc1FanLow);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      this->ac1_.setFan(kHitachiAc1FanMed);
      break;
    case climate::CLIMATE_FAN_HIGH:
      this->ac1_.setFan(kHitachiAc1FanHigh);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      this->ac1_.setFan(kHitachiAc1FanAuto);
      break;
  }

  this->ac1_.setSwingV(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);

  auto *raw = this->ac1_.getRaw();
  ESP_LOGD(TAG,
           "AC1 TX: %02X %02X %02X %02X %02X %02X %02X "
           "%02X %02X %02X %02X %02X %02X",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9], raw[10], raw[11],
           raw[12]);

  this->ac1_.send();
}

void IRRemoteHitachiClimate::transmit_ac344_state_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->ac344_.setMode(kHitachiAc344Cool);
      this->ac344_.on();
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->ac344_.setMode(kHitachiAc344Heat);
      this->ac344_.on();
      break;
    case climate::CLIMATE_MODE_DRY:
      this->ac344_.setMode(kHitachiAc344Dry);
      this->ac344_.on();
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->ac344_.setMode(kHitachiAc344Fan);
      this->ac344_.on();
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      this->ac344_.off();
      break;
  }

  if (!std::isnan(this->target_temperature)) {
    auto temperature = static_cast<uint8_t>(std::lround(this->target_temperature));
    temperature = std::max<uint8_t>(temperature, kHitachiAc344MinTemp);
    temperature = std::min<uint8_t>(temperature, kHitachiAc344MaxTemp);
    this->target_temperature = temperature;
    this->ac344_.setTemp(temperature);
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      this->ac344_.setFan(kHitachiAc344FanLow);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      this->ac344_.setFan(kHitachiAc344FanMedium);
      break;
    case climate::CLIMATE_FAN_HIGH:
      this->ac344_.setFan(kHitachiAc344FanHigh);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      this->ac344_.setFan(kHitachiAc344FanAuto);
      break;
  }

  this->ac344_.setSwingV(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);
  this->ac344_.send();
}

}  // namespace irremote_hitachi
}  // namespace esphome
