#include "iio_adapter/iio_sysfs.h"

#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace iio_adapter {
namespace {

std::string readText(const iio_filesystem::path& path) {
  std::ifstream input(path.string());
  std::string value;
  if (!input || !std::getline(input, value)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

void writeText(const iio_filesystem::path& path, const std::string& value) {
  std::ofstream output(path.string());
  if (!output || !(output << value)) {
    throw std::runtime_error("cannot write " + path.string());
  }
}

unsigned int trailingIndex(const std::string& name) {
  std::size_t first_digit = name.size();
  while (first_digit > 0 && std::isdigit(
             static_cast<unsigned char>(name[first_digit - 1]))) {
    --first_digit;
  }
  if (first_digit == name.size()) {
    throw std::runtime_error("IIO directory has no numeric suffix: " + name);
  }
  return static_cast<unsigned int>(std::stoul(name.substr(first_digit)));
}

}  // namespace

Device findDevice(const iio_filesystem::path& iio_root,
                  const iio_filesystem::path& dev_root,
                  const std::string& expected_name) {
  for (const auto& entry : iio_filesystem::directory_iterator(iio_root)) {
    if (!iio_filesystem::is_directory(entry.path())) continue;
    const auto name_path = entry.path() / "name";
    if (!iio_filesystem::exists(name_path) || readText(name_path) != expected_name) {
      continue;
    }
    const unsigned int index = trailingIndex(entry.path().filename().string());
    return {entry.path(), dev_root / ("iio:device" + std::to_string(index)), index};
  }
  throw std::runtime_error("IIO device not found: " + expected_name);
}

void configureBuffer(const Device& device, const BufferConfig& config) {
  if (config.sampling_hz == 0 || config.length < 2) {
    throw std::invalid_argument("sampling_hz must be positive and length >= 2");
  }

  const auto buffer = device.sysfs_path / "buffer";
  const auto scan = device.sysfs_path / "scan_elements";
  const std::array<const char*, 8> channels = {
      "in_accel_x_en", "in_accel_y_en", "in_accel_z_en", "in_temp_en",
      "in_anglvel_x_en", "in_anglvel_y_en", "in_anglvel_z_en",
      "in_timestamp_en"};

  writeText(buffer / "enable", "0");
  try {
    writeText(device.sysfs_path / "sampling_frequency",
              std::to_string(config.sampling_hz));
    writeText(buffer / "length", std::to_string(config.length));
    for (const char* channel : channels) writeText(scan / channel, "1");
    writeText(buffer / "enable", "1");
  } catch (...) {
    disableBuffer(device);
    throw;
  }
}

void disableBuffer(const Device& device) noexcept {
  std::ofstream output((device.sysfs_path / "buffer" / "enable").string());
  if (output) output << '0';
}

}  // namespace iio_adapter

