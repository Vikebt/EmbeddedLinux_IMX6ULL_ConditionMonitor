#include "iio_adapter/scan_assembler.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { std::cerr << "failed: " #expr << '\n'; return 1; } } while (0)

int main() {
  iio_adapter::ScanAssembler assembler;
  std::array<std::uint8_t, 49> input{};
  for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<std::uint8_t>(i);
  std::size_t count = 0;
  bool frames_valid = true;
  auto on_frame = [&](const iio_adapter::RawScan& frame) {
    if (count < 2) {
      frames_valid &= frame.sensor[0] == (count == 0 ? 0 : 24);
      frames_valid &= frame.sensor[13] == (count == 0 ? 13 : 37);
    }
    ++count;
  };

  assembler.append(input.data(), 1, on_frame);
  CHECK(count == 0);
  CHECK(assembler.pendingBytes() == 1);
  assembler.append(input.data() + 1, 22, on_frame);
  CHECK(count == 0);
  CHECK(assembler.pendingBytes() == 23);
  assembler.append(input.data() + 23, 26, on_frame);
  CHECK(count == 2);
  CHECK(assembler.pendingBytes() == 1);
  assembler.append(input.data() + 1, 23, on_frame);
  CHECK(count == 3);
  CHECK(frames_valid);
  CHECK(assembler.pendingBytes() == 0);
  return 0;
}
