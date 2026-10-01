#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  int samples = 100;
  if (argc == 3 && std::string_view(argv[1]) == "--samples") {
    samples = std::atoi(argv[2]);
  }
  if (samples <= 0) {
    std::cerr << "samples must be positive\n";
    return 2;
  }

  std::cout << "timestamp_ns,ax,ay,az,gx,gy,gz,temp_c\n";
  for (int index = 0; index < samples; ++index) {
    const auto timestamp = static_cast<long long>(index + 1) * 10'000'000LL;
    const bool impact = index == samples / 2 || index == samples / 2 + 1;
    std::cout << timestamp << ",0,0," << (impact ? 24.0 : 9.80665)
              << ",0,0,0,25\n";
  }
  return 0;
}

