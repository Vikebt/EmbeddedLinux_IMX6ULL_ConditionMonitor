# I.MX6ULL 设备姿态与异常冲击监测器

这是一个面向仓储搬运车、户外控制箱等边缘设备的教学型 Linux 驱动项目。ICM20608 通过 I.MX6ULL ECSPI3 接入，内核驱动使用 IIO 暴露加速度、角速度、温度和带时间戳缓冲数据；用户态服务通过 `epoll` 统一处理传感器、周期状态和退出信号，输出姿态、冲击/异常运动与传感器故障状态。

项目基于正点原子 I.MX6ULL 开发板与 Linux 4.1.15 学习经历设计。板端主体采用 C++14，兼容随板资料常见的 Linaro GCC 4.9；C++17 只用于宿主测试夹具。它不虚构商业量产经历，也不把简单阈值算法包装成“预测性维护”。

## 学习路线

| 层级 | 目标 | 可独立验证的结果 |
| --- | --- | --- |
| L0 | 建立可重复构建基线 | CMake 配置、CTest 入口、目录边界 |
| L1 | SPI/regmap/IIO 直读驱动 | probe/remove、WHO_AM_I、`read_raw` |
| L2 | 领域算法与回放 | 姿态、冲击、失联状态机和单元测试 |
| L3 | sysfs 集成 | 自动发现 IIO 设备并配置扫描元素 |
| L4 | IIO 缓冲采样 | hrtimer 只触发 threaded poll，SPI 不进原子上下文 |
| L5 | epoll 服务 | 传感器、timerfd、signalfd 单线程事件循环 |

构建逻辑、代码证据和验证边界分别见 [BUILD_LOGIC.md](docs/BUILD_LOGIC.md)、[INTERVIEW_EVIDENCE.md](docs/INTERVIEW_EVIDENCE.md) 与 [VALIDATION.md](docs/VALIDATION.md)。

## 宿主机构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/conditiond --simulate --samples 200
./build/apps/condition-replay tests/data/normal_then_impact.csv
```

内核模块目标明确限定为正点原子 i.MX6ULL vendor Linux 4.1.15；主线内核 API 不在本仓库承诺范围内。

旧版 CTest 不支持 `--test-dir` 时，改为 `cd build && ctest --output-on-failure`。

## ARM 用户态交叉编译

```bash
export CROSS_COMPILE=/opt/gcc-linaro/bin/arm-linux-gnueabihf-
cmake -S . -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/imx6ull-gcc.cmake \
  -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm
file build-arm/apps/conditiond
```

预期产物是 32 位 ARM EABI5 ELF，动态加载器为 `/lib/ld-linux-armhf.so.3`；仍需确认板端 rootfs 的 glibc/libstdc++ 与工具链一致。

## 板端部署顺序

1. 在 vendor 4.1.15 内核合入 `kernel/Kconfig`/`Makefile`，应用 defconfig fragment 与设备树节点，重编内核、DTB 和模块。
2. 启动后确认 `/sys/bus/iio/devices/iio:device*/name` 为 `icm20608`。
3. 交叉编译并安装 `conditiond`，先运行 `conditiond --simulate --samples 200`，再运行真实设备模式。
4. 完成冷启动、倾斜、两次连续冲击、拔除传感器/停流和长稳测试后，才把 HIL 项标记为通过。

`deploy/conditiond.service` 提供最小 systemd 单元；部署脚本不会替你覆盖内核或设备树，避免把板端差异隐藏在自动化里。

