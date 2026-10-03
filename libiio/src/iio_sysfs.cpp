#include "iio_adapter/iio_sysfs.h"

#include <array>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace iio_adapter {
namespace {

std::string join(const std::string& parent, const std::string& child) {
  if (parent.empty() || parent[parent.size() - 1] == '/') return parent + child;
  return parent + '/' + child;
}

bool tryReadText(const std::string& path, std::string* value) {
  std::ifstream input(path.c_str());
  if (!input || !std::getline(input, *value)) return false;
  while (!value->empty() &&
         std::isspace(static_cast<unsigned char>(value->back()))) {
    value->pop_back();
  }
  return true;
}

void writeText(const std::string& path, const std::string& value) {
  std::ofstream output(path.c_str());
  if (!output) {
    throw std::runtime_error("cannot open " + path);
  }
  output << value;
  output.close();
  if (!output) {
    throw std::runtime_error("cannot write " + path);
  }
}

unsigned int trailingIndex(const std::string& name) {
  std::size_t first_digit = name.size();
  while (first_digit > 0 &&
         std::isdigit(static_cast<unsigned char>(name[first_digit - 1]))) {
    --first_digit;
  }
  if (first_digit == name.size()) {
    throw std::runtime_error("IIO directory has no numeric suffix: " + name);
  }
  return static_cast<unsigned int>(std::stoul(name.substr(first_digit)));
}

struct DirectoryCloser {
  void operator()(DIR* directory) const {
    if (directory) ::closedir(directory);
  }
};

}  // namespace

Device findDevice(const std::string& iio_root,
                  const std::string& dev_root,
                  const std::string& expected_name) {
  std::unique_ptr<DIR, DirectoryCloser> directory(::opendir(iio_root.c_str()));
  if (!directory) throw std::runtime_error("cannot open " + iio_root);

  while (dirent* entry = ::readdir(directory.get())) {
    const std::string directory_name(entry->d_name);
    if (directory_name == "." || directory_name == "..") continue;
    const std::string sysfs_path = join(iio_root, directory_name);
    std::string actual_name;
    if (!tryReadText(join(sysfs_path, "name"), &actual_name) ||
        actual_name != expected_name) {
      continue;
    }
    const unsigned int index = trailingIndex(directory_name);
    return Device(sysfs_path,
                  join(dev_root, "iio:device" + std::to_string(index)), index);
  }
  throw std::runtime_error("IIO device not found: " + expected_name);
}

void configureBuffer(const Device& device, const BufferConfig& config) {
  if (config.sampling_hz == 0 || config.length < 2) {
    throw std::invalid_argument("sampling_hz must be positive and length >= 2");
  }
  const std::string buffer = join(device.sysfs_path, "buffer");
  const std::string scan = join(device.sysfs_path, "scan_elements");
  const std::array<const char*, 8> channels = {
      "in_accel_x_en", "in_accel_y_en", "in_accel_z_en", "in_temp_en",
      "in_anglvel_x_en", "in_anglvel_y_en", "in_anglvel_z_en",
      "in_timestamp_en"};

  writeText(join(buffer, "enable"), "0");
  try {
    writeText(join(device.sysfs_path, "sampling_frequency"),
              std::to_string(config.sampling_hz));
    writeText(join(buffer, "length"), std::to_string(config.length));
    for (const char* channel : channels) writeText(join(scan, channel), "1");
    writeText(join(buffer, "enable"), "1");
  } catch (...) {
    disableBuffer(device);
    throw;
  }
}

void disableBuffer(const Device& device) noexcept {
  std::ofstream output(join(join(device.sysfs_path, "buffer"), "enable").c_str());
  if (output) output << '0';
}

}  // namespace iio_adapter

