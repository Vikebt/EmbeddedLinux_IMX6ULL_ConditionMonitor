#include "iio_adapter/iio_sysfs.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void expect(bool ok, const char* message) {
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
void put(const iio_filesystem::path& path, const std::string& value = "0") {
  iio_filesystem::create_directories(path.parent_path());
  std::ofstream(path.string()) << value;
}
std::string get(const iio_filesystem::path& path) {
  std::ifstream input(path.string());
  std::string value;
  input >> value;
  return value;
}
}  // namespace

int main() {
  const auto unique = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root = iio_filesystem::temp_directory_path() /
                    ("condition-iio-test-" + unique);
  const auto device_path = root / "sys" / "iio_device7";
  const auto scan = device_path / "scan_elements";
  const auto buffer = device_path / "buffer";

  put(device_path / "name", "icm20608\n");
  put(device_path / "sampling_frequency");
  put(buffer / "enable");
  put(buffer / "length");
  for (const char* name : {"in_accel_x_en", "in_accel_y_en", "in_accel_z_en",
                           "in_temp_en", "in_anglvel_x_en", "in_anglvel_y_en",
                           "in_anglvel_z_en", "in_timestamp_en"}) {
    put(scan / name);
  }

  try {
    const auto device = iio_adapter::findDevice(root / "sys", root / "dev",
                                                 "icm20608");
    expect(device.index == 7, "device index is parsed");
    expect(device.character_path.filename() == "iio:device7",
           "character device path uses IIO ABI name");
    iio_adapter::configureBuffer(device, {100, 64});
    expect(get(device_path / "sampling_frequency") == "100", "rate configured");
    expect(get(buffer / "length") == "64", "buffer length configured");
    expect(get(buffer / "enable") == "1", "buffer enabled last");
    expect(get(scan / "in_timestamp_en") == "1", "timestamp enabled");
    iio_adapter::disableBuffer(device);
    expect(get(buffer / "enable") == "0", "buffer can be disabled");
  } catch (const std::exception& error) {
    std::cerr << "unexpected exception: " << error.what() << '\n';
    ++failures;
  }

  std::error_code cleanup_error;
  iio_filesystem::remove_all(root, cleanup_error);
  return failures == 0 ? 0 : 1;
}

