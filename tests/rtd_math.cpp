#include "../components/rtd/rtd_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
using esphome::rtd::temperature_celsius;

int main() {
  // Independent published PT100 table points rounded to0.01 Ohm (Analog Devices,
  // Positive Analog Feedback Compensates PT100 Transducer). Quantization alone
  // allows about0.015 C error; these are not precision instrument measurements.
  const double table[][2] = {{-100,60.26},{-50,80.31},{0,100},{50,119.40},{100,138.50}};
  for (float nominal : {100.0f,1000.0f}) {
    for (const auto &row : table)
      assert(std::abs(temperature_celsius(float(row[1]*nominal/100),nominal)-row[0]) < 0.02);
    // Exact model landmarks, including both bounds and the zero crossing.
    const double exact[][2] = {{-200,18.52008},{-100,60.25584},{0,100},{100,138.5055},{500,280.9775},{850,390.481125}};
    for (const auto &row : exact)
      assert(std::abs(temperature_celsius(float(row[1]*nominal/100),nominal)-row[0]) < 0.001);
    const float minimum=float(18.52008*nominal/100);
    const float maximum=float(390.481125*nominal/100);
    assert(std::isnan(temperature_celsius(std::nextafter(minimum,-INFINITY),nominal)));
    assert(std::isnan(temperature_celsius(std::nextafter(maximum,INFINITY),nominal)));
    assert(std::isfinite(temperature_celsius(std::nextafter(minimum,INFINITY),nominal)));
    assert(std::isfinite(temperature_celsius(std::nextafter(maximum,-INFINITY),nominal)));
    assert(temperature_celsius(std::nextafter(nominal,-INFINITY),nominal)<0);
    assert(temperature_celsius(nominal,nominal)==0);
    assert(temperature_celsius(std::nextafter(nominal,INFINITY),nominal)>0);
    for (float bad : {NAN,INFINITY,-INFINITY,-1.0f,0.0f,1.0f,10000.0f})
      assert(std::isnan(temperature_celsius(bad,nominal)));
  }
  for (float bad : {NAN,INFINITY,-100.0f,0.0f,500.0f})
    assert(std::isnan(temperature_celsius(100,bad)));

  // Independent forward polynomial in long double, no production helper used.
  // Exercise 21002 points across the whole range through the actual float API.
  double worst=0;
  for (float nominal : {100.0f,1000.0f}) {
    float previous=-INFINITY;
    for (int n=-2000;n<=8500;n++) {
      const long double t=n/10.0L;
      long double ratio=1 + 0.0039083L*t - 0.0000005775L*t*t;
      if(t<0) ratio -= 0.000000000004183L*(t-100)*t*t*t;
      const float resistance=static_cast<float>(nominal*ratio);
      const float result=temperature_celsius(resistance,nominal);
      assert(std::isfinite(result) && result>previous);
      worst=std::fmax(worst,std::fabs(result-static_cast<double>(t)));
      assert(std::fabs(result-static_cast<double>(t))<0.001);
      previous=result;
    }
  }
  std::printf("PASS: 21002 sweep points plus reference/edge/invalid tests; max float-interface error %.9f C\n",worst);
}
