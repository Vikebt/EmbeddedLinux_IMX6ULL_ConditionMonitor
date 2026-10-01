#pragma once

#include <string>

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#include <experimental/filesystem>
namespace iio_filesystem = std::experimental::filesystem;
#else
#include <filesystem>
namespace iio_filesystem = std::filesystem;
#endif

namespace iio_adapter {

struct Device {
  iio_filesystem::path sysfs_path;
  iio_filesystem::path character_path;
  unsigned int index{};
};

struct BufferConfig {
  unsigned int sampling_hz{100};
  unsigned int length{128};
};

Device findDevice(const iio_filesystem::path& iio_root,
                  const iio_filesystem::path& dev_root,
                  const std::string& expected_name);

void configureBuffer(const Device& device, const BufferConfig& config);
void disableBuffer(const Device& device) noexcept;

}  // namespace iio_adapter

