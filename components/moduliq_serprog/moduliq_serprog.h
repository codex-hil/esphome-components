// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "protocol.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/moduliq_cpld_flash/moduliq_cpld_flash.h"

namespace esphome::moduliq_serprog {
class Serprog : public Component, public Backend {
 public:
  void set_flash(moduliq_cpld_flash::CPLDFlash *flash) { flash_ = flash; }
  void set_port(uint16_t port) { port_ = port; }
  void set_enabled(bool enabled);
  bool is_enabled() const { return enabled_; }
  void set_session_timeout(uint32_t ms) { timeout_ms_ = ms; }
  Trigger<uint32_t> *get_prepare_trigger() { return &prepare_trigger_; }
  Trigger<uint32_t> *get_release_requested_trigger() { return &release_requested_trigger_; }
  Trigger<uint32_t> *get_released_trigger() { return &released_trigger_; }
  // Generation binds asynchronous YAML confirmations to exactly one session.
  bool confirm_target_ready(uint32_t session);
  bool confirm_release_ready(uint32_t session);
  bool cleanup_transfer(uint32_t session, const uint8_t *tx, size_t write_size, uint8_t *rx, size_t read_size);
  uint32_t current_session() const { return session_; }
  bool release_pending() const { return state_ == RELEASING; }
  void setup() override {}
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  Result spi_operation(const uint8_t *tx, size_t write_size, uint8_t *rx, size_t read_size) override;
 protected:
  enum State { IDLE, PREPARING, ACTIVE, RELEASING };
  bool listen_();
  void disconnect_();
  void finish_release_();
  moduliq_cpld_flash::CPLDFlash *flash_{nullptr};
  std::unique_ptr<socket::ListenSocket> listener_;
  std::unique_ptr<socket::Socket> client_;
  Protocol protocol_{this};
  Trigger<uint32_t> prepare_trigger_, release_requested_trigger_, released_trigger_;
  State state_{IDLE};
  bool enabled_{false}, transferred_{false}, release_ready_{false};
  uint16_t port_{6054};
  uint32_t session_{0}, last_io_{0}, prepared_at_{0}, retry_at_{0}, timeout_ms_{30000};
};
}  // namespace esphome::moduliq_serprog
