#include "irremote_hitachi.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <inttypes.h>

namespace esphome {
namespace irremote_hitachi {

static const char *const TAG = "irremote_hitachi.climate";

static constexpr uint32_t HITACHI_CARRIER_FREQUENCY = 38000;
static constexpr uint32_t HITACHI_AC1_HEADER_MARK = 3400;
static constexpr uint32_t HITACHI_AC1_HEADER_SPACE = 3400;
static constexpr uint32_t HITACHI_AC344_HEADER_MARK = 3300;
static constexpr uint32_t HITACHI_AC344_HEADER_SPACE = 1700;
static constexpr uint32_t HITACHI_BIT_MARK = 400;
static constexpr uint32_t HITACHI_ONE_SPACE = 1250;
static constexpr uint32_t HITACHI_ZERO_SPACE = 500;
static constexpr uint32_t HITACHI_MIN_GAP = 100000;
static constexpr uint32_t HITACHI_POWER_FEEDBACK_TIMEOUT_MARGIN = 5000;

void IRRemoteHitachiClimate::setup() {
  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    this->ac1_.stateReset();
    this->ac1_.setModel(this->ac1_model_);
  } else {
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

  if (this->protocol_ == HITACHI_PROTOCOL_AC1 && this->ac1_model_ == R_LT0541_HTA_B) {
    this->ac1_logical_power_ = this->mode != climate::CLIMATE_MODE_OFF;
    if (this->mode != climate::CLIMATE_MODE_OFF)
      this->last_non_off_mode_ = this->mode;

    // IE-06T2 Model B uses a toggle-only power command, not an absolute power state.
    this->ac1_.setPower(false);
    this->ac1_.setPowerToggle(false);

    // IE-06T2 Model B has no swing capability.
    this->ac1_.setSwingToggle(false);
    this->ac1_.setSwingV(false);
    this->ac1_.setSwingH(false);
  } else if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
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

  if (this->power_feedback_enabled_()) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->handle_power_reading_(state);
    });
    if (this->power_sensor_->has_state())
      this->handle_power_reading_(this->power_sensor_->state);
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
  LOG_SENSOR("  ", "Power Sensor", this->power_sensor_);
  if (this->power_feedback_enabled_()) {
    ESP_LOGCONFIG(TAG, "  Power ON threshold: %.1f W", this->power_on_threshold_);
    ESP_LOGCONFIG(TAG, "  Power OFF threshold: %.1f W", this->power_off_threshold_);
    ESP_LOGCONFIG(TAG, "  Power ON delay: %" PRIu32 " ms", this->power_on_delay_);
    ESP_LOGCONFIG(TAG, "  Power OFF delay: %" PRIu32 " ms", this->power_off_delay_);
  }
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
  if (this->protocol_ != HITACHI_PROTOCOL_AC1 || this->ac1_model_ != R_LT0541_HTA_B) {
    traits.set_supported_swing_modes({
        climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_SWING_VERTICAL,
    });
  }
  if (this->protocol_ == HITACHI_PROTOCOL_AC1) {
    traits.set_visual_min_temperature(kHitachiAcMinTemp);
    traits.set_visual_max_temperature(kHitachiAcMaxTemp);
  } else {
    traits.set_visual_min_temperature(kHitachiAc344MinTemp);
    traits.set_visual_max_temperature(kHitachiAc344MaxTemp);
  }
  traits.set_visual_target_temperature_step(1.0f);
  if (this->sensor_ != nullptr)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  return traits;
}

void IRRemoteHitachiClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    if (*call.get_mode() != climate::CLIMATE_MODE_OFF)
      this->last_non_off_mode_ = *call.get_mode();
    this->mode = *call.get_mode();
  }
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  this->transmit_state_();
  this->publish_state();
}

void IRRemoteHitachiClimate::loop() {
  if (!this->power_feedback_enabled_())
    return;

  const uint32_t now = millis();
  if (this->power_feedback_candidate_ != PowerFeedbackCandidate::UNKNOWN &&
      !this->power_feedback_candidate_confirmed_ &&
      now - this->power_feedback_candidate_started_ >=
          this->power_feedback_delay_(this->power_feedback_candidate_)) {
    this->confirm_power_feedback_();
  }

  if (this->power_command_pending_ && now - this->power_command_started_ >= this->power_feedback_timeout_())
    this->handle_power_command_timeout_();
}

bool IRRemoteHitachiClimate::power_feedback_enabled_() const {
  return this->power_sensor_ != nullptr && this->protocol_ == HITACHI_PROTOCOL_AC1 &&
         this->ac1_model_ == R_LT0541_HTA_B;
}

bool IRRemoteHitachiClimate::effective_power_state_() const {
  if (this->power_command_pending_)
    return this->pending_power_target_;
  if (this->power_feedback_valid_)
    return this->power_feedback_on_;
  return this->ac1_logical_power_;
}

uint32_t IRRemoteHitachiClimate::power_feedback_delay_(PowerFeedbackCandidate candidate) const {
  return candidate == PowerFeedbackCandidate::ON ? this->power_on_delay_ : this->power_off_delay_;
}

uint32_t IRRemoteHitachiClimate::power_feedback_timeout_() const {
  return std::max(this->power_on_delay_, this->power_off_delay_) + HITACHI_POWER_FEEDBACK_TIMEOUT_MARGIN;
}

void IRRemoteHitachiClimate::handle_power_reading_(float watts) {
  if (!std::isfinite(watts)) {
    ESP_LOGD(TAG, "Ignoring invalid power sensor value");
    return;
  }

  PowerFeedbackCandidate candidate = PowerFeedbackCandidate::UNKNOWN;
  if (watts >= this->power_on_threshold_)
    candidate = PowerFeedbackCandidate::ON;
  else if (watts <= this->power_off_threshold_)
    candidate = PowerFeedbackCandidate::OFF;

  const char *candidate_name = candidate == PowerFeedbackCandidate::ON
                                   ? "ON"
                                   : candidate == PowerFeedbackCandidate::OFF ? "OFF" : "UNKNOWN";
  ESP_LOGV(TAG, "Power feedback: %.1f W, candidate %s", watts, candidate_name);

  if (candidate == PowerFeedbackCandidate::UNKNOWN) {
    if (this->power_feedback_candidate_ != PowerFeedbackCandidate::UNKNOWN) {
      this->power_feedback_candidate_ = PowerFeedbackCandidate::UNKNOWN;
      this->power_feedback_candidate_confirmed_ = false;
      ESP_LOGD(TAG, "Power feedback returned to hysteresis region");
    }
    return;
  }

  if (candidate != this->power_feedback_candidate_) {
    this->power_feedback_candidate_ = candidate;
    this->power_feedback_candidate_started_ = millis();
    this->power_feedback_candidate_confirmed_ = false;
    ESP_LOGD(TAG, "Power feedback candidate %s", candidate_name);
  }

  if (!this->power_feedback_candidate_confirmed_ &&
      millis() - this->power_feedback_candidate_started_ >= this->power_feedback_delay_(candidate))
    this->confirm_power_feedback_();
}

void IRRemoteHitachiClimate::confirm_power_feedback_() {
  if (this->power_feedback_candidate_ == PowerFeedbackCandidate::UNKNOWN ||
      this->power_feedback_candidate_confirmed_)
    return;

  this->power_feedback_candidate_confirmed_ = true;
  const bool power_on = this->power_feedback_candidate_ == PowerFeedbackCandidate::ON;
  const bool physical_changed = !this->power_feedback_valid_ || this->power_feedback_on_ != power_on;
  this->power_feedback_valid_ = true;
  this->power_feedback_on_ = power_on;

  if (this->power_command_pending_) {
    if (this->pending_power_target_ == power_on) {
      this->power_command_pending_ = false;
      ESP_LOGD(TAG, "Power feedback confirmed pending target %s", power_on ? "ON" : "OFF");
    } else {
      ESP_LOGD(TAG, "Power feedback confirmed AC %s while toggle target %s remains pending",
               power_on ? "ON" : "OFF", this->pending_power_target_ ? "ON" : "OFF");
      return;
    }
  }

  if (physical_changed)
    ESP_LOGD(TAG, "Power feedback confirmed AC %s", power_on ? "ON" : "OFF");
  this->synchronize_power_feedback_(power_on);
}

void IRRemoteHitachiClimate::synchronize_power_feedback_(bool power_on) {
  const bool logical_changed = this->ac1_logical_power_ != power_on;
  if (logical_changed) {
    ESP_LOGD(TAG, "Synchronizing AC1 logical power: %s -> %s",
             this->ac1_logical_power_ ? "ON" : "OFF", power_on ? "ON" : "OFF");
    this->ac1_logical_power_ = power_on;
  }

  bool climate_changed = false;
  if (power_on) {
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      if (this->last_non_off_mode_ == climate::CLIMATE_MODE_OFF)
        this->last_non_off_mode_ = climate::CLIMATE_MODE_COOL;
      this->mode = this->last_non_off_mode_;
      climate_changed = true;
    }
  } else if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->last_non_off_mode_ = this->mode;
    this->mode = climate::CLIMATE_MODE_OFF;
    climate_changed = true;
  }

  if (logical_changed || climate_changed)
    this->publish_state();
}

void IRRemoteHitachiClimate::handle_power_command_timeout_() {
  if (!this->power_command_pending_)
    return;

  this->power_command_pending_ = false;
  if (this->power_feedback_valid_) {
    ESP_LOGW(TAG, "Power toggle feedback timed out; physical state remains %s",
             this->power_feedback_on_ ? "ON" : "OFF");
    this->synchronize_power_feedback_(this->power_feedback_on_);
  } else {
    ESP_LOGW(TAG, "Power toggle feedback timed out; no valid physical state was confirmed");
  }
}

void IRRemoteHitachiClimate::transmit_state_() {
  if (this->protocol_ == HITACHI_PROTOCOL_AC1)
    this->transmit_ac1_state_();
  else
    this->transmit_ac344_state_();
}

void IRRemoteHitachiClimate::transmit_ac1_state_() {
  const bool is_ie06t2 = this->ac1_model_ == R_LT0541_HTA_B;
  const bool requested_power = this->mode != climate::CLIMATE_MODE_OFF;
  const bool feedback_enabled = this->power_feedback_enabled_();
  const bool effective_power = feedback_enabled ? this->effective_power_state_() : this->ac1_logical_power_;
  const bool power_changed = requested_power != effective_power;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->ac1_.setMode(kHitachiAc1Cool);
      if (!is_ie06t2)
        this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->ac1_.setMode(kHitachiAc1Heat);
      if (!is_ie06t2)
        this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_DRY:
      this->ac1_.setMode(kHitachiAc1Dry);
      if (!is_ie06t2)
        this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->ac1_.setMode(kHitachiAc1Fan);
      if (!is_ie06t2)
        this->ac1_.on();
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      if (!is_ie06t2)
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

  if (is_ie06t2) {
    // IE-06T2 Model B sends only a momentary power toggle and has no swing.
    this->ac1_.setPower(false);
    this->ac1_.setPowerToggle(power_changed);
    this->ac1_.setSwingToggle(false);
    this->ac1_.setSwingV(false);
    this->ac1_.setSwingH(false);
  } else {
    this->ac1_.setSwingV(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);
  }

  auto *raw = this->ac1_.getRaw();
  ESP_LOGD(TAG,
           "AC1 TX: %02X %02X %02X %02X %02X %02X %02X "
           "%02X %02X %02X %02X %02X %02X",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9], raw[10], raw[11],
           raw[12]);

  if (is_ie06t2 && feedback_enabled && power_changed) {
    this->power_command_pending_ = true;
    this->pending_power_target_ = requested_power;
    this->power_command_started_ = millis();
    this->power_feedback_candidate_ = PowerFeedbackCandidate::UNKNOWN;
    this->power_feedback_candidate_confirmed_ = false;
    ESP_LOGD(TAG, "Power toggle pending: target %s", requested_power ? "ON" : "OFF");
  } else if (is_ie06t2 && feedback_enabled) {
    ESP_LOGD(TAG, "Power toggle not required: physical AC already %s", requested_power ? "ON" : "OFF");
  }

  this->transmit_ac1_frame_(raw);
  this->ac1_.setPowerToggle(false);
  this->ac1_.setSwingToggle(false);
  if (is_ie06t2)
    this->ac1_logical_power_ = requested_power;
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
  this->transmit_ac344_frame_(this->ac344_.getRaw());
}

void IRRemoteHitachiClimate::transmit_ac1_frame_(const uint8_t *raw) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(HITACHI_CARRIER_FREQUENCY);
  data->reserve(2 + kHitachiAc1StateLength * 16 + 2);

  data->item(HITACHI_AC1_HEADER_MARK, HITACHI_AC1_HEADER_SPACE);
  for (uint16_t i = 0; i < kHitachiAc1StateLength; i++) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      data->item(HITACHI_BIT_MARK, raw[i] & mask ? HITACHI_ONE_SPACE : HITACHI_ZERO_SPACE);
    }
  }
  data->item(HITACHI_BIT_MARK, HITACHI_MIN_GAP);

  transmit.perform();
}

void IRRemoteHitachiClimate::transmit_ac344_frame_(const uint8_t *raw) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(HITACHI_CARRIER_FREQUENCY);
  data->reserve(2 + kHitachiAc344StateLength * 16 + 2);

  data->item(HITACHI_AC344_HEADER_MARK, HITACHI_AC344_HEADER_SPACE);
  for (uint16_t i = 0; i < kHitachiAc344StateLength; i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->item(HITACHI_BIT_MARK, raw[i] & (1U << bit) ? HITACHI_ONE_SPACE : HITACHI_ZERO_SPACE);
    }
  }
  data->item(HITACHI_BIT_MARK, HITACHI_MIN_GAP);

  transmit.perform();
}

}  // namespace irremote_hitachi
}  // namespace esphome
