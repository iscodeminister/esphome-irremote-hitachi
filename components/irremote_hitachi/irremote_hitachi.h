#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <ir_Hitachi.h>

namespace esphome {
namespace irremote_hitachi {

enum HitachiProtocol : uint8_t {
  HITACHI_PROTOCOL_AC1,
  HITACHI_PROTOCOL_AC344,
};

class IRRemoteHitachiClimate : public climate::Climate, public Component {
 public:
  explicit IRRemoteHitachiClimate(uint8_t pin) : ac1_(pin), ac344_(pin) {}

  void setup() override;
  void dump_config() override;
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_protocol(HitachiProtocol protocol) { this->protocol_ = protocol; }
  void set_carrier_duty_percent(uint8_t carrier_duty_percent) {
    this->carrier_duty_percent_ = carrier_duty_percent;
  }
  void set_ac1_model_b(bool model_b) {
    this->ac1_model_ = model_b ? R_LT0541_HTA_B : R_LT0541_HTA_A;
  }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void transmit_state_();
  void transmit_ac1_state_();
  void transmit_ac344_state_();

  IRHitachiAc1 ac1_;
  IRHitachiAc344 ac344_;
  HitachiProtocol protocol_{HITACHI_PROTOCOL_AC344};
  hitachi_ac1_remote_model_t ac1_model_{R_LT0541_HTA_B};
  uint8_t carrier_duty_percent_{50};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace irremote_hitachi
}  // namespace esphome
