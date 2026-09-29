// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cmath>

namespace esphome::rtd {
// IEC 60751 platinum, alpha=0.00385. Coefficients: TI SBAA275 section1.
// Same CVD model as Wizath's ADS124S08 converter; independent bounded inverse.
inline double resistance_ratio(double temperature) {
  constexpr double A = 3.9083e-3;
  constexpr double B = -5.775e-7;
  constexpr double C = -4.183e-12;
  double ratio = 1.0 + temperature * (A + B * temperature);
  if (temperature < 0.0)
    ratio += C * (temperature - 100.0) * temperature * temperature * temperature;
  return ratio;
}

// Input/output match ESPHome's float sensor interface. Intermediate math is double.
// Out-of-range values produce NaN; no extrapolation or guessed lead correction.
inline float temperature_celsius(float resistance, float nominal_resistance) {
  if (!std::isfinite(resistance) || !std::isfinite(nominal_resistance) ||
      (nominal_resistance != 100.0f && nominal_resistance != 1000.0f))
    return NAN;
  // Round the endpoint resistances to the same float representation as the input.
  // This accepts exact representable endpoints without arbitrary range tolerance.
  const float minimum = static_cast<float>(nominal_resistance * resistance_ratio(-200.0));
  const float maximum = static_cast<float>(nominal_resistance * resistance_ratio(850.0));
  if (resistance < minimum || resistance > maximum)
    return NAN;
  if (resistance == minimum)
    return -200.0f;
  if (resistance == maximum)
    return 850.0f;

  const double ratio = static_cast<double>(resistance) / nominal_resistance;
  if (ratio >= 1.0) {
    constexpr double A = 3.9083e-3;
    constexpr double B = -5.775e-7;
    // Rationalized quadratic root avoids cancellation near zero Celsius.
    return static_cast<float>(2.0 * (ratio - 1.0) / (A + std::sqrt(A * A + 4.0 * B * (ratio - 1.0))));
  }
  // CVD is strictly increasing on [-200,0]. Bisection has a fixed iteration
  // bound and cannot diverge like an unchecked Newton iteration.
  double lower = -200.0;
  double upper = 0.0;
  for (unsigned i = 0; i < 32; ++i) {
    const double middle = (lower + upper) * 0.5;
    if (resistance_ratio(middle) < ratio)
      lower = middle;
    else
      upper = middle;
  }
  return static_cast<float>((lower + upper) * 0.5);
}
}  // namespace esphome::rtd
