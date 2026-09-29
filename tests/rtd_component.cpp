// Execute the production callback adapter against a minimal sensor-event stub.
#include "../components/rtd/rtd.h"
#include <cassert>
#include <cmath>
using namespace esphome;
int main(){
  sensor::Sensor source;
  rtd::RTDSensor pt100,pt1000;
  pt100.set_sensor(&source);pt100.set_nominal_resistance(100);
  pt1000.set_sensor(&source);pt1000.set_nominal_resistance(1000);
  pt100.setup();pt1000.setup();
  assert(!pt100.has_state() && !pt1000.has_state());
  source.publish_state(138.5055f);
  assert(std::fabs(pt100.state-100)<0.001 && !pt100.warning);
  assert(std::isnan(pt1000.state) && pt1000.warning);
  source.publish_state(NAN);
  assert(std::isnan(pt100.state) && pt100.warning);
  source.publish_state(INFINITY);assert(std::isnan(pt100.state));
  source.publish_state(0);assert(std::isnan(pt100.state));
  source.publish_state(100);
  assert(pt100.state==0 && !pt100.warning && pt100.publications.size()==5);
  source.publish_state(602.5584f);
  assert(std::isnan(pt100.state));
  assert(std::fabs(pt1000.state+100)<0.001 && !pt1000.warning);
  // Setup after the source has already published consumes its current state.
  rtd::RTDSensor late;
  late.set_sensor(&source);late.set_nominal_resistance(1000);late.setup();
  assert(late.publications.size()==1 && std::fabs(late.state+100)<0.001);
  // Bad initial source state propagates, then recovers on the next valid event.
  sensor::Sensor failed_source;failed_source.publish_state(NAN);
  rtd::RTDSensor recovery;recovery.set_sensor(&failed_source);recovery.setup();
  assert(std::isnan(recovery.state) && recovery.warning);
  failed_source.publish_state(100);
  assert(recovery.state==0 && !recovery.warning);
}
