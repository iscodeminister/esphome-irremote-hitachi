#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <ir_Hitachi.h>

namespace esphome {
namespace irremote_hitachi {

enum HitachiProtocol : uint8_t {
  HITACHI_PROTOCOL_AC1,
  HITACHI_PROTOCOL_AC344,
};

class IRRemoteHitachiClimate : public climate::Climate,
                               public Component,
                               public remote_base::RemoteTransmittable {
 public:
  IRRemoteHitachiClimate() : ac1_(0), ac344_(0) {}

  void setup() override;
  void dump_config() override;
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_power_on_threshold(float threshold) { this->power_on_threshold_ = threshold; }
  void set_power_off_threshold(float threshold) { this->power_off_threshold_ = threshold; }
  void set_power_on_delay(uint32_t delay) { this->power_on_delay_ = delay; }
  void set_power_off_delay(uint32_t delay) { this->power_off_delay_ = delay; }
  void set_protocol(HitachiProtocol protocol) { this->protocol_ = protocol; }
  void set_ac1_model_b(bool model_b) {
    this->ac1_model_ = model_b ? R_LT0541_HTA_B : R_LT0541_HTA_A;
  }

 protected:
  enum class PowerFeedbackCandidate : uint8_t {
    UNKNOWN,
    ON,
    OFF,
  };

  climate::ClimateTraits traits() override;
  void loop() override;
  void control(const climate::ClimateCall &call) override;
  void transmit_state_();
  void transmit_ac1_state_();
  void transmit_ac344_state_();
  void transmit_ac1_frame_(const uint8_t *raw);
  void transmit_ac344_frame_(const uint8_t *raw);
  bool power_feedback_enabled_() const;
  bool effective_power_state_() const;
  uint32_t power_feedback_delay_(PowerFeedbackCandidate candidate) const;
  uint32_t power_feedback_timeout_() const;
  void handle_power_reading_(float watts);
  void confirm_power_feedback_();
  void synchronize_power_feedback_(bool power_on);
  void handle_power_command_timeout_();

  IRHitachiAc1 ac1_;
  IRHitachiAc344 ac344_;
  HitachiProtocol protocol_{HITACHI_PROTOCOL_AC344};
  hitachi_ac1_remote_model_t ac1_model_{R_LT0541_HTA_B};
  bool ac1_logical_power_{false};
  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  float power_on_threshold_{0.0f};
  float power_off_threshold_{0.0f};
  uint32_t power_on_delay_{3000};
  uint32_t power_off_delay_{30000};
  bool power_feedback_valid_{false};
  bool power_feedback_on_{false};
  PowerFeedbackCandidate power_feedback_candidate_{PowerFeedbackCandidate::UNKNOWN};
  uint32_t power_feedback_candidate_started_{0};
  bool power_feedback_candidate_confirmed_{false};
  bool power_command_pending_{false};
  bool pending_power_target_{false};
  uint32_t power_command_started_{0};
  climate::ClimateMode last_non_off_mode_{climate::CLIMATE_MODE_COOL};
};

}  // namespace irremote_hitachi
}  // namespace esphome
