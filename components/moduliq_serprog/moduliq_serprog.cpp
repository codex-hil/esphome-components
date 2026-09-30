// SPDX-License-Identifier: GPL-3.0-only
#include "moduliq_serprog.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cerrno>

namespace esphome::moduliq_serprog {
static const char *const TAG = "moduliq_serprog";
static bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
void Serprog::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled) { listener_.reset(); disconnect_(); }
}
bool Serprog::listen_() {
  auto server = socket::socket_listen(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (!server) return false;
  int reuse = 1;
  server->setsockopt(SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET; address.sin_port = htons(port_); address.sin_addr.s_addr = htonl(INADDR_ANY);
  auto length = sizeof(address);
  if (server->setblocking(false) != 0 ||
      server->bind(reinterpret_cast<sockaddr *>(&address), length) != 0 || server->listen(1) != 0) return false;
  listener_ = std::move(server);
  status_clear_warning();
  return true;
}
bool Serprog::confirm_target_ready(uint32_t session) {
  if (session != session_ || state_ != PREPARING || !client_ || !enabled_) return false;
  if (!flash_->acquire_session(this, true)) { disconnect_(); return false; }
  state_ = ACTIVE;
  return true;
}
Backend::Result Serprog::spi_operation(const uint8_t *tx, size_t write_size, uint8_t *rx, size_t read_size) {
  if (state_ == PREPARING) return WAIT;
  if (state_ != ACTIVE) return ERROR;
  if (!flash_->is_enabled()) { disconnect_(); return ERROR; }
  // Conservative: any raw SPI frame might start a long internal operation.
  transferred_ = true;
  if (!flash_->transfer_session(this, tx, write_size, rx, read_size)) {
    disconnect_(); return ERROR;
  }
  return OK;
}
bool Serprog::cleanup_transfer(uint32_t session, const uint8_t *tx, size_t write_size,
                               uint8_t *rx, size_t read_size) {
  if (session != session_ || state_ != RELEASING || release_ready_) return false;
  return flash_->transfer_session(this, tx, write_size, rx, read_size);
}
bool Serprog::confirm_release_ready(uint32_t session) {
  if (session != session_ || state_ != RELEASING) return false;
  release_ready_ = true;
  return true;
}
void Serprog::disconnect_() {
  client_.reset();
  if (state_ == IDLE || state_ == RELEASING) return;
  state_ = RELEASING;
  release_ready_ = !transferred_;
  // Fires before any ownership handback, even if preparation was cancelled.
  release_requested_trigger_.trigger(session_);
}
void Serprog::finish_release_() {
  if (!release_ready_) return;
  if (!flash_->release_session(this, true) || !flash_->end_session(this)) return;
  state_ = IDLE;
  protocol_.reset();
  status_clear_warning();
  released_trigger_.trigger(session_);  // YAML may now restore target/reset/boot levels.
}
void Serprog::loop() {
  if (state_ == RELEASING) {
    if (millis() - retry_at_ >= 100) { retry_at_ = millis(); finish_release_(); }
  }
  if (enabled_ && !listener_ && millis() - retry_at_ >= 1000) {
    retry_at_ = millis();
    if (!listen_()) status_set_warning();
  }
  if (listener_) {
    // Reject competitors without disturbing the existing memory lease.
    auto incoming = listener_->accept(nullptr, nullptr);
    if (incoming && state_ == IDLE && flash_->reserve_session(this)) {
      if (incoming->setblocking(false) != 0) { flash_->end_session(this); return; }
      int one = 1;
      incoming->setsockopt(IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      incoming->setsockopt(SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
      client_ = std::move(incoming); protocol_.reset();
      state_ = PREPARING; transferred_ = release_ready_ = false;
      if (++session_ == 0) ++session_;
      last_io_ = prepared_at_ = millis();
      prepare_trigger_.trigger(session_);
    }
  }
  if (!client_) return;
  if (millis() - last_io_ >= timeout_ms_ ||
      (state_ == PREPARING && millis() - prepared_at_ >= timeout_ms_)) { disconnect_(); return; }
  // Bounded work, no blocking network calls and no socket waits with CS asserted.
  // Read unmonitored sockets directly: bounded processing cannot violate ready() draining rules.
  size_t budget = 2048;
  while (client_ && budget) {
    protocol_.process();
    if (!client_) return;
    if (protocol_.output_size()) {
      auto size = std::min(budget, protocol_.output_size());
      auto sent = client_->write(protocol_.output(), size);
      if (sent < 0) { if (!would_block()) disconnect_(); return; }
      if (sent == 0) { disconnect_(); return; }
      protocol_.output_consumed(sent); budget -= sent; last_io_ = millis();
      continue;
    }
    if (protocol_.close_requested()) { disconnect_(); return; }
    uint8_t buffer[128];
    auto size = std::min({budget, sizeof(buffer), protocol_.bytes_requested()});
    if (!size) return;  // Await YAML target readiness without acknowledging the SPI command.
    auto received = client_->read(buffer, size);
    if (received < 0) { if (!would_block()) disconnect_(); return; }
    if (!received) { disconnect_(); return; }
    last_io_ = millis(); budget -= received;
    protocol_.consume(buffer, received);
  }
}
void Serprog::on_shutdown() {
  listener_.reset(); disconnect_();
  // No wait, reset, or forced release of a chip that might still be erasing.
  if (state_ == RELEASING && release_ready_) finish_release_();
}
void Serprog::dump_config() {
  ESP_LOGCONFIG(TAG, "Serprog TCP port %u, enabled %s; transfer limits %u/%u", port_, enabled_ ? "yes" : "no",
                unsigned(Protocol::MAX_WRITE), unsigned(Protocol::MAX_READ));
}
}  // namespace esphome::moduliq_serprog
