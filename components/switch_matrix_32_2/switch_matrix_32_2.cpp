#include "switch_matrix_32_2.h"
#include "esphome/core/log.h"

namespace esphome::switch_matrix_32_2 {
static const char *const TAG = "switch_matrix_32_2";

void Matrix::setup() {
  // /OE is active low. Keep relay drivers disabled until both latches are zero.
  oe_->setup();
  oe_->digital_write(true);
  cs_a_->setup();
  cs_b_->setup();
  cs_a_->digital_write(true);
  cs_b_->digital_write(true);
  spi_setup();
  write_masks_(0, 0);
  oe_->digital_write(false);
  ready_ = true;
  publish_();
}

void Matrix::dump_config() {
  ESP_LOGCONFIG(TAG, "Switch Matrix 32:2 (commanded state, no relay feedback)");
  LOG_PIN("  CSn A: ", cs_a_);
  LOG_PIN("  CSn B: ", cs_b_);
  LOG_PIN("  /OE: ", oe_);
  ESP_LOGCONFIG(TAG, "  Break before make: %u ms", (unsigned) break_ms_);
}

void Matrix::write_bank_(GPIOPin *latch, uint32_t mask) {
  // Both chains shift on every clock. Only the selected RCLK gets a rising edge.
  enable();
  latch->digital_write(false);
  for (int shift = 24; shift >= 0; shift -= 8)
    write_byte(static_cast<uint8_t>(mask >> shift));
  latch->digital_write(true);
  disable();
}

void Matrix::write_masks_(uint32_t a, uint32_t b) {
  write_bank_(cs_a_, a);
  write_bank_(cs_b_, b);
  actual_a_ = a;
  actual_b_ = b;
}

bool Matrix::set_masks(uint32_t a, uint32_t b) {
  if (!ready_ || (a & b) != 0) {
    ESP_LOGW(TAG, "Rejected masks: not ready or overlapping A/B bits");
    return false;
  }
  desired_a_ = a;
  desired_b_ = b;
  cancel_timeout("make");
  // Drop removed routes first; retain unchanged routes. A new command during
  // the delay restarts the interval and supersedes the previous target.
  write_masks_(actual_a_ & a, actual_b_ & b);
  publish_();
  set_timeout("make", break_ms_, [this]() {
    write_masks_(desired_a_, desired_b_);
    publish_();
  });
  return true;
}

bool Matrix::parse_mask(const std::string &text, uint32_t &value) {
  size_t start = 0;
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) start = 2;
  if (text.size() <= start || text.size() - start > 8) return false;
  uint32_t parsed = 0;
  for (size_t i = start; i < text.size(); ++i) {
    const char c = text[i];
    unsigned digit;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    else return false;
    parsed = (parsed << 4) | digit;
  }
  value = parsed;
  return true;
}

bool Matrix::set_masks(const std::string &a, const std::string &b) {
  uint32_t parsed_a, parsed_b;
  if (!parse_mask(a, parsed_a) || !parse_mask(b, parsed_b)) {
    ESP_LOGW(TAG, "Rejected malformed hex masks (expected 1-8 hex digits)");
    return false;
  }
  return set_masks(parsed_a, parsed_b);
}

bool Matrix::set_channel(uint8_t channel, const std::string &option) {
  if (channel < 1 || channel > 32) return false;
  const uint32_t bit = uint32_t(1) << (channel - 1);
  uint32_t a = desired_a_ & ~bit, b = desired_b_ & ~bit;
  if (option == "A") a |= bit;
  else if (option == "B") b |= bit;
  else if (option != "BYPASS") return false;
  return set_masks(a, b);
}

void Matrix::publish_() {
  for (auto *entity : selects_) {
    const uint32_t bit = uint32_t(1) << (entity->channel() - 1);
    entity->publish_state((actual_a_ & bit) ? "A" : (actual_b_ & bit) ? "B" : "BYPASS");
  }
}

void Matrix::on_shutdown() {
  ready_ = false;
  cancel_timeout("make");
  oe_->digital_write(true);
}
}  // namespace esphome::switch_matrix_32_2
