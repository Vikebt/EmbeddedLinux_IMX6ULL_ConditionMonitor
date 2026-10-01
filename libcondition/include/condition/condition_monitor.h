#pragma once

#include <cstdint>
#include <string_view>

namespace condition {

struct Sample {
  std::int64_t timestamp_ns{};
  double accel_x_mps2{};
  double accel_y_mps2{};
  double accel_z_mps2{};
  double gyro_x_radps{};
  double gyro_y_radps{};
  double gyro_z_radps{};
  double temperature_c{};
};

enum class State { kNormal, kTilted, kImpact, kSensorFault };

struct Assessment {
  State state{State::kSensorFault};
  double roll_deg{};
  double pitch_deg{};
  double accel_magnitude_mps2{};
  std::int64_t timestamp_ns{};
};

struct Config {
  double tilt_threshold_deg{25.0};
  double impact_threshold_mps2{20.0};
  std::uint32_t tilt_confirm_samples{5};
  std::uint32_t impact_confirm_samples{2};
  std::int64_t stale_timeout_ns{500'000'000};
};

class Monitor {
 public:
  explicit Monitor(Config config = {});
  Assessment update(const Sample& sample);
  Assessment checkTimeout(std::int64_t now_ns) const;

 private:
  Config config_;
  Assessment last_{};
  std::int64_t last_timestamp_ns_{};
  std::uint32_t tilt_count_{};
  std::uint32_t impact_count_{};
  bool has_sample_{};
};

std::string_view toString(State state);

}  // namespace condition

