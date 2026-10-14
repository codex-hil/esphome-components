#pragma once
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

namespace esphome::scpi {
inline std::string upper(std::string s) {
  for (char &c : s) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  return s;
}
inline std::string trim(std::string s) {
  auto first = s.find_first_not_of(" \t");
  if (first == std::string::npos) return {};
  return s.substr(first, s.find_last_not_of(" \t") - first + 1);
}
inline bool matches(const std::string &pattern, const std::string &header) {
  size_t p = 0, h = 0;
  while (true) {
    auto pe = pattern.find(':', p), he = header.find(':', h);
    auto part = pattern.substr(p, pe - p), incoming = upper(header.substr(h, he - h));
    std::string short_form;
    for (char c : part) if (c >= 'A' && c <= 'Z') short_form += c;
    if (incoming != short_form && incoming != upper(part)) return false;
    if (pe == std::string::npos || he == std::string::npos) return pe == he;
    p = pe + 1; h = he + 1;
  }
}
struct Message {
  std::string header, parameter;
  bool query{false};
  bool valid{true};
};
inline Message parse(std::string line) {
  Message m;
  if (!line.empty() && line.back() == '\r') line.pop_back();
  for (unsigned char c : line) if ((c < 32 && c != '\t') || c > 126 || c == ';') m.valid = false;
  line = trim(line);
  auto pos = line.find_first_of(" \t");
  m.header = line.substr(0, pos);
  m.parameter = pos == std::string::npos ? "" : trim(line.substr(pos));
  if (!m.header.empty() && m.header.front() == ':') m.header.erase(0, 1);
  if (!m.header.empty() && m.header.back() == '?') { m.query = true; m.header.pop_back(); }
  if (m.header.empty() || m.header.find('?') != std::string::npos) m.valid = false;
  return m;
}
inline bool decimal(const std::string &s, float &value) {
  // Decimal only: forbid strtof's hex, INF and NAN extensions.
  size_t i = 0, digits = 0;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  }
  if (!digits) return false;
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i; if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    size_t start = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    if (start == i) return false;
  }
  if (i != s.size()) return false;
  errno = 0; char *end;
  value = std::strtof(s.c_str(), &end);
  return errno != ERANGE && std::isfinite(value) && *end == 0;
}
inline const char *error_text(int code) {
  switch (code) {
    case -102: return "Syntax error";
    case -104: return "Invalid parameter";
    case -113: return "Undefined header";
    case -222: return "Data out of range";
    case -230: return "Data unavailable";
    case -350: return "Queue overflow";
    case -363: return "Input buffer overrun";
    default: return "No error";
  }
}
class ErrorQueue {
 public:
  explicit ErrorQueue(size_t capacity = 8) : capacity_(capacity) {}
  void push(int code) {
    if (queue_.size() < capacity_) queue_.push_back(code);
    else queue_.back() = -350; // Preserve oldest entries and sticky overflow in last slot.
  }
  int pop() {
    if (queue_.empty()) return 0;
    int result = queue_.front(); queue_.pop_front(); return result;
  }
 private:
  size_t capacity_;
  std::deque<int> queue_;
};
// Streaming framing with discard-through-LF after an oversized frame.
class Framer {
 public:
  explicit Framer(size_t limit = 256) : limit_(limit) {}
  template<typename Line, typename Error> void feed(char c, Line line, Error error) {
    if (c == '\n') {
      if (!discard_) line(buffer_);
      buffer_.clear(); discard_ = false; active_ = false;
    } else {
      active_ = true;
      if (!discard_) {
        if (buffer_.size() == limit_) { buffer_.clear(); discard_ = true; error(-363); }
        else buffer_ += c;
      }
    }
  }
  bool active() const { return active_; }
 private:
  size_t limit_;
  std::string buffer_;
  bool discard_{false}, active_{false};
};
} // namespace esphome::scpi
