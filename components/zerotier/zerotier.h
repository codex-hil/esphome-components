#pragma once
#include "esphome/core/component.h"
#include "esphome/components/ethernet/ethernet_component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
extern "C" {
#include "zerotier_native.h"
}
namespace esphome::zerotier {
class ZeroTierComponent : public Component {
 public:
  void set_ethernet(ethernet::EthernetComponent *v) { ethernet_ = v; }
  void set_network_id(uint64_t v) { network_id_ = v; }
  void set_node_id_sensor(text_sensor::TextSensor *v) { node_sensor_ = v; }
  void set_address_sensor(text_sensor::TextSensor *v) { address_sensor_ = v; }
  void set_connected_sensor(binary_sensor::BinarySensor *v) { connected_sensor_ = v; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
 protected:
  ethernet::EthernetComponent *ethernet_{nullptr};
  text_sensor::TextSensor *node_sensor_{nullptr};
  text_sensor::TextSensor *address_sensor_{nullptr};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint64_t network_id_{0}, last_node_{0};
  uint32_t last_poll_{0}, last_ip_{0};
  bool started_{false}, stopping_{false}, ready_{false};
};
}
