// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::moduliq_serprog {
// Each limit applies to the complete phase, including command/address bytes.
// Two maximum phases fit the Flash driver's 4096-byte frame limit.
class Backend {
 public:
  enum Result { OK, WAIT, ERROR };
  virtual ~Backend() = default;
  virtual Result spi_operation(const uint8_t *tx, size_t write_size, uint8_t *rx, size_t read_size) = 0;
};
class Protocol {
 public:
  static constexpr size_t MAX_WRITE = 1024, MAX_READ = 1024;
  explicit Protocol(Backend *backend) : backend_(backend) {}
  void reset();
  // Caller provides only bytes_requested(); stops reading until response is drained.
  size_t bytes_requested() const { return output_size_ || waiting_ || close_ ? 0 : expected_ - input_size_; }
  void consume(const uint8_t *data, size_t size);
  void process();
  const uint8_t *output() const { return output_.data() + output_sent_; }
  size_t output_size() const { return output_size_ - output_sent_; }
  void output_consumed(size_t size);
  bool close_requested() const { return close_ && !output_size_; }
 protected:
  void respond_(bool ack, size_t payload = 0);
  Backend *backend_;
  std::array<uint8_t, 7 + MAX_WRITE> input_{};
  std::array<uint8_t, 1 + MAX_READ> output_{};
  size_t input_size_{0}, expected_{1}, output_size_{0}, output_sent_{0};
  bool waiting_{false}, close_{false};
};
}  // namespace esphome::moduliq_serprog
