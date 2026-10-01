#include "condition/condition_monitor.h"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
bool parseSample(const std::string& line, condition::Sample* sample) {
  std::array<double, 8> values{};
  std::stringstream input(line);
  std::string field;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (!std::getline(input, field, ',')) return false;
    try {
      values[index] = std::stod(field);
    } catch (const std::exception&) {
      return false;
    }
  }
  sample->timestamp_ns = static_cast<std::int64_t>(values[0]);
  sample->accel_x_mps2 = values[1];
  sample->accel_y_mps2 = values[2];
  sample->accel_z_mps2 = values[3];
  sample->gyro_x_radps = values[4];
  sample->gyro_y_radps = values[5];
  sample->gyro_z_radps = values[6];
  sample->temperature_c = values[7];
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: condition-replay samples.csv\n";
    return 2;
  }
  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open " << argv[1] << '\n';
    return 2;
  }

  condition::Monitor monitor;
  condition::State previous = condition::State::kSensorFault;
  std::string line;
  std::getline(input, line);
  std::size_t accepted = 0;
  std::size_t rejected = 0;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    condition::Sample sample{};
    if (!parseSample(line, &sample)) {
      ++rejected;
      continue;
    }
    ++accepted;
    const auto assessment = monitor.update(sample);
    if (assessment.state != previous) {
      std::cout << assessment.timestamp_ns << ','
                << condition::toString(assessment.state) << ','
                << assessment.roll_deg << ',' << assessment.pitch_deg << ','
                << assessment.accel_magnitude_mps2 << '\n';
      previous = assessment.state;
    }
  }
  std::cerr << "accepted=" << accepted << " rejected=" << rejected << '\n';
  return accepted == 0 ? 1 : 0;
}

