#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace iio_adapter {

struct alignas(8) RawScan {
  std::array<std::uint8_t, 14> sensor;
  std::array<std::uint8_t, 2> padding;
  std::int64_t timestamp_ns;
};
static_assert(sizeof(RawScan) == 24, "IIO scan layout must be 24 bytes");

// read(2) boundaries need not align with IIO scan boundaries. Keep only a
// partial scan, so memory use never grows with an active sensor stream.
class ScanAssembler {
 public:
  std::size_t pendingBytes() const { return used_; }

  template <typename Handler>
  void append(const std::uint8_t* data, std::size_t size, Handler&& on_frame) {
    while (size != 0) {
      const std::size_t count = std::min(size, pending_.size() - used_);
      std::memcpy(pending_.data() + used_, data, count);
      used_ += count;
      data += count;
      size -= count;
      if (used_ == pending_.size()) {
        RawScan frame;
        std::memcpy(&frame, pending_.data(), sizeof(frame));
        used_ = 0;
        on_frame(frame);
      }
    }
  }

 private:
  // All bytes are filled before a complete frame is copied out.
  std::array<std::uint8_t, sizeof(RawScan)> pending_;
  std::size_t used_{};
};

}  // namespace iio_adapter
