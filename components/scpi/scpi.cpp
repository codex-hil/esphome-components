#include "scpi.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstdio>

namespace esphome::scpi {
static const char *const TAG = "scpi";
static bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
void SCPIComponent::configure(std::string idn, uint16_t port, size_t clients, size_t message, size_t tx,
                              size_t errors, uint32_t timeout) {
  idn_ = std::move(idn); port_ = port; max_clients_ = clients; message_limit_ = message;
  tx_limit_ = tx; error_limit_ = errors; timeout_ = timeout;
}
void SCPIComponent::setup() {
  // Poll bounded nonblocking sockets from loop(); no ready()/drain contract needed.
  listener_ = socket::socket_listen(AF_INET, SOCK_STREAM, 0);
  if (!listener_) { mark_failed(); return; }
  int yes = 1;
  listener_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port_);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (listener_->setblocking(false) != 0 || listener_->bind(reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
      listener_->listen(static_cast<int>(max_clients_)) != 0) {
    ESP_LOGE(TAG, "Listener setup failed"); listener_.reset(); mark_failed(); return;
  }
  clients_.resize(max_clients_);
}
void SCPIComponent::dump_config() { ESP_LOGCONFIG(TAG, "SCPI: port=%u, clients=%u, message=%u, tx=%u", port_,
  static_cast<unsigned>(max_clients_), static_cast<unsigned>(message_limit_), static_cast<unsigned>(tx_limit_)); }
void SCPIComponent::on_shutdown() { clients_.clear(); listener_.reset(); }
std::string SCPIComponent::fail_(Client &c, const Message &m, int code) {
  c.errors.push(code); return m.query ? "NAN" : "";
}
std::string SCPIComponent::value_(Client &c, const Message &m, bool available, float value) {
  if (!available || !std::isfinite(value)) return fail_(c, m, -230);
  char buffer[32]; std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value)); return buffer;
}
#ifdef USE_SENSOR
void SCPIComponent::add_sensor(std::string pattern, sensor::Sensor *entity) {
  bindings_.push_back({std::move(pattern), [this, entity](Client &c, const Message &m) {
    if (!m.query) return fail_(c, m, -113);
    if (!m.parameter.empty()) return fail_(c, m, -104);
    return value_(c, m, entity->has_state(), entity->state);
  }});
}
#endif
#ifdef USE_BINARY_SENSOR
void SCPIComponent::add_binary_sensor(std::string pattern, binary_sensor::BinarySensor *entity) {
  bindings_.push_back({std::move(pattern), [this, entity](Client &c, const Message &m) {
    if (!m.query) return fail_(c, m, -113);
    if (!m.parameter.empty()) return fail_(c, m, -104);
    return value_(c, m, entity->has_state(), entity->state ? 1 : 0);
  }});
}
#endif
#ifdef USE_SWITCH
void SCPIComponent::add_switch(std::string pattern, switch_::Switch *entity, bool writable) {
  bindings_.push_back({std::move(pattern), [this, entity, writable](Client &c, const Message &m) {
    if (m.query) {
      if (!m.parameter.empty()) return fail_(c, m, -104);
      return value_(c, m, true, entity->state ? 1 : 0);
    }
    if (!writable) return fail_(c, m, -113);
    auto p = upper(m.parameter);
    if (p == "1" || p == "ON") entity->turn_on();
    else if (p == "0" || p == "OFF") entity->turn_off();
    else return fail_(c, m, -104);
    return std::string{};
  }});
}
#endif
#ifdef USE_NUMBER
void SCPIComponent::add_number(std::string pattern, number::Number *entity, bool writable) {
  bindings_.push_back({std::move(pattern), [this, entity, writable](Client &c, const Message &m) {
    if (m.query) {
      if (!m.parameter.empty()) return fail_(c, m, -104);
      return value_(c, m, entity->has_state(), entity->state);
    }
    if (!writable) return fail_(c, m, -113);
    float value;
    if (!decimal(m.parameter, value)) return fail_(c, m, -104);
    const auto &traits = entity->traits;
    if (value < traits.get_min_value() || value > traits.get_max_value()) return fail_(c, m, -222);
    auto call = entity->make_call(); call.set_value(value); call.perform();
    return std::string{};
  }});
}
#endif
#ifdef USE_BUTTON
void SCPIComponent::add_button(std::string pattern, button::Button *entity) {
  bindings_.push_back({std::move(pattern), [this, entity](Client &c, const Message &m) {
    if (m.query) return fail_(c, m, -113);
    if (!m.parameter.empty()) return fail_(c, m, -104);
    entity->press(); return std::string{};
  }});
}
#endif
std::string SCPIComponent::execute_(Client &c, const std::string &line) {
  auto content = line;
  if (!content.empty() && content.back() == '\r') content.pop_back();
  if (trim(content).empty()) return {};
  auto m = parse(line);
  if (!m.valid) return fail_(c, m, -102);
  if (upper(m.header) == "*IDN" && m.query) {
    if (!m.parameter.empty()) return fail_(c, m, -104);
    return idn_;
  }
  if (matches("SYSTem:ERRor", m.header) && m.query) {
    if (!m.parameter.empty()) return fail_(c, m, -104);
    int error = c.errors.pop(); return std::to_string(error) + ",\"" + error_text(error) + "\"";
  }
  for (auto &binding : bindings_) if (matches(binding.pattern, m.header)) return binding.execute(c, m);
  return fail_(c, m, -113);
}
void SCPIComponent::service_(Client &c, uint32_t now) {
  if (now - c.activity >= timeout_ || (c.framer.active() && now - c.frame_start >= timeout_) ||
      (!c.tx.empty() && now - c.tx_progress >= timeout_)) { c.socket.reset(); return; }
  // Keep room for the largest single response before invoking entity operations.
  if (c.tx.size() <= tx_limit_ - 194) {
    if (c.pending.empty()) {
      char data[128]; auto n = c.socket->read(data, sizeof(data));
      if (n == 0 || (n < 0 && !would_block())) { c.socket.reset(); return; }
      if (n > 0) { c.pending.assign(data, n); c.activity = now; }
    }
    size_t consumed = 0, lines = 0;
    while (consumed < c.pending.size() && lines < 4 && c.tx.size() <= tx_limit_ - 194) {
      char ch = c.pending[consumed++];
      if (!c.framer.active() && ch != '\n') c.frame_start = now;
      c.framer.feed(ch, [&](const std::string &line) {
        ++lines; auto reply = execute_(c, line);
        if (!reply.empty()) {
          if (c.tx.empty()) c.tx_progress = now;
          c.tx += reply; c.tx += '\n';
        }
      }, [&](int error) { c.errors.push(error); });
    }
    c.pending.erase(0, consumed);
  }
  if (!c.tx.empty()) {
    // MSG_NOSIGNAL prevents a disconnected Linux peer from killing the process.
    auto sent = c.socket->send(c.tx.data(), std::min<size_t>(c.tx.size(), 512), MSG_NOSIGNAL);
    if (sent > 0) { c.tx.erase(0, sent); c.tx_progress = now; c.activity = now; }
    else if (sent == 0 || !would_block()) c.socket.reset();
  }
}
void SCPIComponent::loop() {
  if (!listener_) return;
  uint32_t now = millis();
  // At most one accept per loop. Full server accepts then closes the excess peer.
  auto incoming = listener_->accept(nullptr, nullptr);
  if (incoming && incoming->setblocking(false) == 0) {
    auto free = std::find_if(clients_.begin(), clients_.end(), [](const auto &c) { return !c; });
    if (free != clients_.end()) *free = std::make_unique<Client>(std::move(incoming), message_limit_, error_limit_, now);
  }
  for (size_t i = 0; i < clients_.size(); ++i) {
    size_t slot = (cursor_ + i) % clients_.size();
    auto &client = clients_[slot];
    if (client) { service_(*client, now); if (!client->socket) client.reset(); }
  }
  cursor_ = (cursor_ + 1) % clients_.size();
}
} // namespace esphome::scpi
