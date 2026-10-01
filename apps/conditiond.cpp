#include "condition/condition_monitor.h"
#include "iio_adapter/iio_sysfs.h"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

namespace {

class UniqueFd {
 public:
  explicit UniqueFd(int fd = -1) : fd_(fd) {}
  ~UniqueFd() { if (fd_ >= 0) ::close(fd_); }
  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;
  UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      if (fd_ >= 0) ::close(fd_);
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }
  int get() const { return fd_; }

 private:
  int fd_;
};

struct BufferGuard {
  iio_adapter::Device device;
  bool enabled{};
  ~BufferGuard() { if (enabled) iio_adapter::disableBuffer(device); }
};

struct alignas(8) RawScan {
  std::array<std::uint8_t, 14> sensor;
  std::array<std::uint8_t, 2> padding;
  std::int64_t timestamp_ns;
};
static_assert(sizeof(RawScan) == 24, "IIO scan layout must be 24 bytes");

std::int64_t realtimeNs() {
  timespec value;
  std::memset(&value, 0, sizeof(value));
  if (::clock_gettime(CLOCK_REALTIME, &value) != 0) {
    throw std::system_error(errno, std::generic_category(), "clock_gettime");
  }
  return static_cast<std::int64_t>(value.tv_sec) * 1'000'000'000LL + value.tv_nsec;
}

int makeTimer(std::int64_t interval_ns) {
  const int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0) throw std::system_error(errno, std::generic_category(), "timerfd_create");
  itimerspec spec;
  std::memset(&spec, 0, sizeof(spec));
  spec.it_interval.tv_sec = interval_ns / 1'000'000'000LL;
  spec.it_interval.tv_nsec = interval_ns % 1'000'000'000LL;
  spec.it_value = spec.it_interval;
  if (::timerfd_settime(fd, 0, &spec, nullptr) != 0) {
    const int saved = errno;
    ::close(fd);
    throw std::system_error(saved, std::generic_category(), "timerfd_settime");
  }
  return fd;
}

void addEpoll(int epoll_fd, int fd) {
  epoll_event event;
  std::memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  event.data.fd = fd;
  if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) != 0) {
    throw std::system_error(errno, std::generic_category(), "epoll_ctl");
  }
}

std::int16_t be16(const std::uint8_t* bytes) {
  return static_cast<std::int16_t>(
      (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
}

condition::Sample decode(const RawScan& raw) {
  constexpr double kAccelScale = 9.80665 / 16384.0;
  constexpr double kGyroScale = 3.14159265358979323846 / (180.0 * 131.0);
  condition::Sample sample{};
  sample.timestamp_ns = raw.timestamp_ns;
  sample.accel_x_mps2 = be16(&raw.sensor[0]) * kAccelScale;
  sample.accel_y_mps2 = be16(&raw.sensor[2]) * kAccelScale;
  sample.accel_z_mps2 = be16(&raw.sensor[4]) * kAccelScale;
  sample.temperature_c = be16(&raw.sensor[6]) / 326.8 + 25.0;
  sample.gyro_x_radps = be16(&raw.sensor[8]) * kGyroScale;
  sample.gyro_y_radps = be16(&raw.sensor[10]) * kGyroScale;
  sample.gyro_z_radps = be16(&raw.sensor[12]) * kGyroScale;
  return sample;
}

void consumeTimer(int fd) {
  std::uint64_t expirations{};
  if (::read(fd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
    if (errno != EAGAIN) throw std::system_error(errno, std::generic_category(), "timer read");
  }
}

void printIfChanged(const condition::Assessment& value,
                    condition::State* previous) {
  if (value.state == *previous) return;
  std::cout << "{\"timestamp_ns\":" << value.timestamp_ns
            << ",\"state\":\"" << condition::toString(value.state)
            << "\",\"roll_deg\":" << value.roll_deg
            << ",\"pitch_deg\":" << value.pitch_deg
            << ",\"accel_mps2\":" << value.accel_magnitude_mps2
            << "}\n";
  *previous = value.state;
}

}  // namespace

int main(int argc, char** argv) try {
  bool simulate = false;
  std::size_t sample_limit = 0;
  for (int index = 1; index < argc; ++index) {
    const std::string argument(argv[index]);
    if (argument == "--simulate") {
      simulate = true;
    } else if (argument == "--samples" && index + 1 < argc) {
      sample_limit = static_cast<std::size_t>(std::stoul(argv[++index]));
    } else {
      std::cerr << "usage: conditiond [--simulate] [--samples N]\n";
      return 2;
    }
  }
  if (!simulate && sample_limit != 0) {
    std::cerr << "--samples is only valid with --simulate\n";
    return 2;
  }

  sigset_t mask;
  ::sigemptyset(&mask);
  ::sigaddset(&mask, SIGINT);
  ::sigaddset(&mask, SIGTERM);
  if (::sigprocmask(SIG_BLOCK, &mask, nullptr) != 0) {
    throw std::system_error(errno, std::generic_category(), "sigprocmask");
  }

  UniqueFd signal_fd(::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC));
  if (signal_fd.get() < 0) throw std::system_error(errno, std::generic_category(), "signalfd");
  UniqueFd status_fd(makeTimer(250'000'000));
  UniqueFd simulation_fd(simulate ? makeTimer(10'000'000) : -1);
  UniqueFd sensor_fd;
  BufferGuard buffer_guard{};

  if (!simulate) {
    buffer_guard.device = iio_adapter::findDevice(
        "/sys/bus/iio/devices", "/dev", "icm20608");
    iio_adapter::configureBuffer(buffer_guard.device,
                                 iio_adapter::BufferConfig(100, 128));
    buffer_guard.enabled = true;
    sensor_fd = UniqueFd(::open(buffer_guard.device.character_path.c_str(),
                                O_RDONLY | O_NONBLOCK | O_CLOEXEC));
    if (sensor_fd.get() < 0) {
      throw std::system_error(errno, std::generic_category(), "open IIO device");
    }
  }

  UniqueFd epoll_fd(::epoll_create1(EPOLL_CLOEXEC));
  if (epoll_fd.get() < 0) throw std::system_error(errno, std::generic_category(), "epoll_create1");
  addEpoll(epoll_fd.get(), signal_fd.get());
  addEpoll(epoll_fd.get(), status_fd.get());
  addEpoll(epoll_fd.get(), simulate ? simulation_fd.get() : sensor_fd.get());

  condition::Monitor monitor;
  condition::State previous = condition::State::kSensorFault;
  std::size_t generated = 0;
  std::vector<std::uint8_t> pending;
  bool running = true;
  while (running) {
    std::array<epoll_event, 4> events;
    std::memset(events.data(), 0, sizeof(events));
    const int count = ::epoll_wait(epoll_fd.get(), events.data(), events.size(), -1);
    if (count < 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(), "epoll_wait");
    }
    for (int index = 0; index < count; ++index) {
      const int fd = events[index].data.fd;
      if (fd == signal_fd.get()) {
        signalfd_siginfo info;
        std::memset(&info, 0, sizeof(info));
        const ssize_t received = ::read(fd, &info, sizeof(info));
        if (received == static_cast<ssize_t>(sizeof(info))) {
          running = false;
        } else if (received < 0 && errno != EAGAIN) {
          throw std::system_error(errno, std::generic_category(), "signalfd read");
        } else if (received >= 0) {
          throw std::runtime_error("short signalfd read");
        }
      } else if (fd == status_fd.get()) {
        consumeTimer(fd);
        printIfChanged(monitor.checkTimeout(realtimeNs()), &previous);
      } else if (simulate && fd == simulation_fd.get()) {
        consumeTimer(fd);
        ++generated;
        condition::Sample sample{};
        sample.timestamp_ns = realtimeNs();
        sample.accel_z_mps2 =
            (sample_limit > 3 && (generated == sample_limit / 2 ||
                                  generated == sample_limit / 2 + 1))
                ? 24.0 : 9.80665;
        sample.temperature_c = 25.0;
        printIfChanged(monitor.update(sample), &previous);
        if (sample_limit != 0 && generated >= sample_limit) running = false;
      } else if (!simulate && fd == sensor_fd.get()) {
        std::array<std::uint8_t, sizeof(RawScan) * 16> bytes;
        bytes.fill(0);
        for (;;) {
          const ssize_t received = ::read(fd, bytes.data(), bytes.size());
          if (received > 0) {
            pending.insert(pending.end(), bytes.begin(), bytes.begin() + received);
          } else if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
          } else if (received == 0) {
            throw std::runtime_error("IIO device returned end-of-file");
          } else {
            throw std::system_error(errno, std::generic_category(), "read IIO device");
          }
        }
        std::size_t consumed = 0;
        while (pending.size() - consumed >= sizeof(RawScan)) {
          RawScan raw;
          std::memset(&raw, 0, sizeof(raw));
          std::memcpy(&raw, pending.data() + consumed, sizeof(raw));
          printIfChanged(monitor.update(decode(raw)), &previous);
          consumed += sizeof(raw);
        }
        if (consumed != 0) pending.erase(pending.begin(), pending.begin() + consumed);
      }
    }
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "conditiond: " << error.what() << '\n';
  return 1;
}

