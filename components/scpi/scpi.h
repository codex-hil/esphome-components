#pragma once
#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "protocol.h"
#include <functional>
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
namespace esphome::scpi {
class SCPIComponent : public Component {
 public:
  void configure(std::string idn, uint16_t port, size_t clients, size_t message, size_t tx, size_t errors, uint32_t timeout);
  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
#ifdef USE_SENSOR
  void add_sensor(std::string pattern, sensor::Sensor *entity);
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(std::string pattern, binary_sensor::BinarySensor *entity);
#endif
#ifdef USE_SWITCH
  void add_switch(std::string pattern, switch_::Switch *entity, bool writable);
#endif
#ifdef USE_NUMBER
  void add_number(std::string pattern, number::Number *entity, bool writable);
#endif
#ifdef USE_BUTTON
  void add_button(std::string pattern, button::Button *entity);
#endif
 protected:
  struct Client {
    std::unique_ptr<socket::Socket> socket;
    Framer framer;
    ErrorQueue errors;
    std::string pending, tx;
    uint32_t activity, frame_start{0}, tx_progress{0};
    Client(std::unique_ptr<socket::Socket> s, size_t message, size_t errors, uint32_t now)
      : socket(std::move(s)), framer(message), errors(errors), activity(now) {}
  };
  struct Binding {
    std::string pattern;
    std::function<std::string(Client &, const Message &)> execute;
  };
  std::string fail_(Client &c, const Message &m, int code);
  std::string value_(Client &c, const Message &m, bool available, float value);
  std::string execute_(Client &c, const std::string &line);
  void service_(Client &c, uint32_t now);
  std::vector<Binding> bindings_;
  std::vector<std::unique_ptr<Client>> clients_;
  std::unique_ptr<socket::ListenSocket> listener_;
  std::string idn_;
  uint16_t port_{5025};
  size_t max_clients_{2}, message_limit_{256}, tx_limit_{1024}, error_limit_{8}, cursor_{0};
  uint32_t timeout_{10000};
};
}
