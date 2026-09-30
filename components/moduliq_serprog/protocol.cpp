// SPDX-License-Identifier: GPL-3.0-only
#include "protocol.h"
#include <algorithm>
#include <cstring>

namespace esphome::moduliq_serprog {
static uint32_t le24(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16; }
void Protocol::reset() {
  input_size_ = output_size_ = output_sent_ = 0; expected_ = 1; waiting_ = close_ = false;
}
void Protocol::respond_(bool ack, size_t payload) {
  output_[0] = ack ? 0x06 : 0x15; output_size_ = 1 + payload; output_sent_ = 0;
  input_size_ = 0; expected_ = 1; waiting_ = false;
}
void Protocol::output_consumed(size_t size) {
  output_sent_ += std::min(size, output_size());
  if (output_sent_ == output_size_) output_sent_ = output_size_ = 0;
}
void Protocol::consume(const uint8_t *data, size_t size) {
  if (!data || size > bytes_requested()) return;
  std::copy_n(data, size, input_.begin() + input_size_); input_size_ += size;
  if (input_size_ == 1) {
    if (input_[0] == 0x12) expected_ = 2;
    if (input_[0] == 0x13) expected_ = 7;
  }
  if (input_[0] == 0x13 && input_size_ == 7) {
    auto w = le24(&input_[1]), r = le24(&input_[4]);
    if (w > MAX_WRITE || r > MAX_READ || w + r == 0) {
      // Never interpret a rejected payload as commands or allocate its claimed length.
      close_ = true; respond_(false); return;
    }
    expected_ = 7 + w;
  }
  if (input_size_ == expected_) { waiting_ = true; process(); }
}
void Protocol::process() {
  if (!waiting_ || output_size_) return;
  switch (input_[0]) {
    case 0x00: respond_(true); break;
    case 0x01: output_[1] = 1; output_[2] = 0; respond_(true, 2); break;
    case 0x02: {
      std::fill_n(output_.begin() + 1, 32, 0);
      for (uint8_t cmd : {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x10, 0x11, 0x12, 0x13})
        output_[1 + cmd / 8] |= 1U << (cmd % 8);
      respond_(true, 32); break;
    }
    case 0x03:
      std::memcpy(&output_[1], "moduliq-serprog\0\0", 16); respond_(true, 16); break;
    case 0x04:  // TCP backpressure; no serial ring buffer or opbuf streaming.
      output_[1] = 0xFF; output_[2] = 0xFF; respond_(true, 2); break;
    case 0x05: output_[1] = 8; respond_(true, 1); break;
    case 0x08:
    case 0x11: {
      size_t limit = input_[0] == 0x08 ? MAX_WRITE : MAX_READ;
      output_[1] = limit; output_[2] = limit >> 8; output_[3] = limit >> 16;
      respond_(true, 3); break;
    }
    case 0x10: output_[1] = 0x06; respond_(false, 1); break;
    case 0x12: respond_(input_[1] == 8); break;
    case 0x13: {
      auto result = backend_->spi_operation(&input_[7], le24(&input_[1]), &output_[1], le24(&input_[4]));
      if (result != Backend::WAIT) respond_(result == Backend::OK, result == Backend::OK ? le24(&input_[4]) : 0);
      break;
    }
    default:
      // Unsupported command lengths are unknown. NAK, then terminate this stream.
      close_ = true; respond_(false); break;
  }
}
}  // namespace esphome::moduliq_serprog
