#pragma once

#include <string>
#include <utility>

namespace iio_adapter {

struct Device {
  std::string sysfs_path;
  std::string character_path;
  unsigned int index;

  Device() : index(0) {}
  Device(std::string sysfs, std::string character, unsigned int number)
      : sysfs_path(std::move(sysfs)),
        character_path(std::move(character)),
        index(number) {}
};

struct BufferConfig {
  unsigned int sampling_hz;
  unsigned int length;

  BufferConfig(unsigned int rate = 100, unsigned int buffer_length = 128)
      : sampling_hz(rate), length(buffer_length) {}
};

Device findDevice(const std::string& iio_root,
                  const std::string& dev_root,
                  const std::string& expected_name);

void configureBuffer(const Device& device, const BufferConfig& config);
void disableBuffer(const Device& device) noexcept;

}  // namespace iio_adapter

