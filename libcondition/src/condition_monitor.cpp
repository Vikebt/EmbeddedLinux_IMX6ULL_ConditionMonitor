#include "condition/condition_monitor.h"

#include <cmath>
#include <stdexcept>

namespace condition {
namespace {
constexpr double kRadiansToDegrees = 57.29577951308232;

bool finite(const Sample& sample) {
  return std::isfinite(sample.accel_x_mps2) &&
         std::isfinite(sample.accel_y_mps2) &&
         std::isfinite(sample.accel_z_mps2) &&
         std::isfinite(sample.gyro_x_radps) &&
         std::isfinite(sample.gyro_y_radps) &&
         std::isfinite(sample.gyro_z_radps) &&
         std::isfinite(sample.temperature_c);
}
}  // namespace

Monitor::Monitor(Config config) : config_(config) {
  if (config_.tilt_threshold_deg <= 0.0 ||
      config_.impact_threshold_mps2 <= 0.0 ||
      config_.tilt_confirm_samples == 0 ||
      config_.impact_confirm_samples == 0 ||
      config_.stale_timeout_ns <= 0) {
    throw std::invalid_argument("monitor configuration must be positive");
  }
}

Assessment Monitor::update(const Sample& sample) {
  Assessment next{};
  next.timestamp_ns = sample.timestamp_ns;
  if (sample.timestamp_ns <= 0 ||
      (has_sample_ && sample.timestamp_ns <= last_timestamp_ns_) ||
      !finite(sample)) {
    next.state = State::kSensorFault;
    last_ = next;
    tilt_count_ = 0;
    impact_count_ = 0;
    return last_;
  }

  next.accel_magnitude_mps2 = std::sqrt(
      sample.accel_x_mps2 * sample.accel_x_mps2 +
      sample.accel_y_mps2 * sample.accel_y_mps2 +
      sample.accel_z_mps2 * sample.accel_z_mps2);
  next.roll_deg = std::atan2(sample.accel_y_mps2, sample.accel_z_mps2) *
                  kRadiansToDegrees;
  next.pitch_deg = std::atan2(
      -sample.accel_x_mps2,
      std::hypot(sample.accel_y_mps2, sample.accel_z_mps2)) *
      kRadiansToDegrees;

  const bool impact = next.accel_magnitude_mps2 >= config_.impact_threshold_mps2;
  const bool tilted = std::abs(next.roll_deg) >= config_.tilt_threshold_deg ||
                      std::abs(next.pitch_deg) >= config_.tilt_threshold_deg;
  impact_count_ = impact ? impact_count_ + 1 : 0;
  tilt_count_ = tilted ? tilt_count_ + 1 : 0;

  if (impact_count_ >= config_.impact_confirm_samples) {
    next.state = State::kImpact;
  } else if (tilt_count_ >= config_.tilt_confirm_samples) {
    next.state = State::kTilted;
  } else {
    next.state = State::kNormal;
  }

  has_sample_ = true;
  last_timestamp_ns_ = sample.timestamp_ns;
  last_ = next;
  return last_;
}

Assessment Monitor::checkTimeout(std::int64_t now_ns) const {
  Assessment result = last_;
  if (!has_sample_ || now_ns < last_timestamp_ns_ ||
      now_ns - last_timestamp_ns_ > config_.stale_timeout_ns) {
    result.state = State::kSensorFault;
    result.timestamp_ns = now_ns;
  }
  return result;
}

const char* toString(State state) {
  switch (state) {
    case State::kNormal: return "normal";
    case State::kTilted: return "tilted";
    case State::kImpact: return "impact";
    case State::kSensorFault: return "sensor_fault";
  }
  return "sensor_fault";
}

}  // namespace condition

