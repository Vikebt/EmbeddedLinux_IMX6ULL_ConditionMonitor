#include "condition/condition_monitor.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void expect(bool ok, const char* message) {
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
condition::Sample upright(std::int64_t timestamp_ns) {
  condition::Sample sample{};
  sample.timestamp_ns = timestamp_ns;
  sample.accel_z_mps2 = 9.80665;
  sample.temperature_c = 25.0;
  return sample;
}
}  // namespace

int main() {
  condition::Monitor monitor;
  auto result = monitor.update(upright(10'000'000));
  expect(result.state == condition::State::kNormal, "upright sample is normal");
  result = monitor.checkTimeout(10'000'000);
  expect(result.state == condition::State::kNormal,
         "a timeout check at the sample timestamp preserves state");

  auto high = upright(20'000'000);
  high.accel_z_mps2 = 25.0;
  result = monitor.update(high);
  expect(result.state == condition::State::kNormal, "single spike is not impact");
  high.timestamp_ns = 30'000'000;
  result = monitor.update(high);
  expect(result.state == condition::State::kImpact, "confirmed spike is impact");

  for (std::int64_t index = 4; index <= 8; ++index) {
    auto tilted = upright(index * 10'000'000);
    tilted.accel_y_mps2 = 9.80665;
    tilted.accel_z_mps2 = 9.80665;
    result = monitor.update(tilted);
  }
  expect(result.state == condition::State::kTilted, "confirmed tilt is alarm");
  expect(std::abs(result.roll_deg - 45.0) < 0.001, "roll follows gravity vector");

  result = monitor.checkTimeout(700'000'001);
  expect(result.state == condition::State::kSensorFault, "stale data is fault");

  auto invalid = upright(900'000'000);
  invalid.accel_x_mps2 = std::numeric_limits<double>::quiet_NaN();
  result = monitor.update(invalid);
  expect(result.state == condition::State::kSensorFault, "NaN is fault");

  result = monitor.update(upright(80'000'000));
  expect(result.state == condition::State::kSensorFault,
         "non-monotonic timestamp is fault");
  return failures == 0 ? 0 : 1;
}

