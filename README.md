# I.MX6ULL 设备姿态与异常冲击监测器

这是一个面向仓储搬运车、户外控制箱等边缘设备的教学型 Linux 驱动项目。ICM20608 通过 I.MX6ULL ECSPI3 接入，内核驱动使用 IIO 暴露加速度、角速度、温度和带时间戳缓冲数据；用户态服务通过 `epoll` 统一处理传感器、周期状态和退出信号，输出姿态、冲击/异常运动与传感器故障状态。

项目基于正点原子 I.MX6ULL 开发板与 Linux 4.1.15 学习经历设计。它不虚构商业量产经历，也不把简单阈值算法包装成“预测性维护”。

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

