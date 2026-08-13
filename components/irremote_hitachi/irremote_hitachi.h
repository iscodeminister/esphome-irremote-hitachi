#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <ir_Hitachi.h>

namespace esphome {
namespace irremote_hitachi {

class IRRemoteHitachiClimate : public climate::Climate, public Component {
 public:
  explicit IRRemoteHitachiClimate(uint8_t pin) : ac_(pin) {}

  void setup() override;
  void dump_config() override;
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void transmit_state_();

  IRHitachiAc344 ac_;
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace irremote_hitachi
}  // namespace esphome
