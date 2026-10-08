# 从例程到项目：可亲手复现的 Linux 驱动学习实验

这份实验单补充 [构建逻辑](BUILD_LOGIC.md) 和[面试讲义的驱动章节](interview-handbook/05-linux-driver.md)。目标不是背出“probe、IIO、epoll”几个词，而是能沿着一条样本从设备树走到状态输出，指出每一步由谁负责、失败时会怎样。当前没有 I.MX6ULL 和 ICM20608 实物，因此以下 L0–L5 中的可运行实验均属 **Host**；内核交叉构建属 **Source**；真实 SPI、IIO buffer 与长稳效果仍为 **TODO-HIL**。

所有命令除特别说明外，在仓库根目录的 Linux/WSL Bash 中执行。Windows 的 MinGW 路径有中文目录限制，不能把 Windows 上无法构建 `conditiond` 误判为业务代码失败。第一次练习先不改源码：按“先预测 → 运行 → 对照输出 → 用自己的话解释”的顺序做。若要故意改坏代码观察失败，请在单独副本或实验分支进行，并保留原始测试记录。

## L0：先建立能重复的基线

```bash
./scripts/validate_userspace.sh
```

脚本执行 Release 构建、CTest 和 200 个模拟样本；若提供 `CROSS_COMPILE`，还会尝试 ARM 用户态交叉构建。没有交叉工具链时不要设置它。也可拆开运行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
(cd build && ctest --output-on-failure)
```

Linux 上应能看到 `condition-monitor`、`replay-fixture`、`scan-assembler`、`iio-sysfs`、`conditiond-simulated`、`conditiond-process` 六个测试。若只看到两项，先确认是否在 Linux 环境及 `BUILD_TESTING` 是否启用，不要直接写“六项通过”。

自己回答：CMake 为什么把 `libcondition` 放在 Linux 限定之外，却把 `libiio` 和 `conditiond` 的进程级测试放在 Linux 限定内？到 [根 CMakeLists](../CMakeLists.txt) 和 [测试 CMakeLists](../tests/CMakeLists.txt) 指出对应条件。

## L1：画出设备到用户态的责任链（无板时先做源码追踪）

按顺序阅读 [设备树节点](../deploy/imx6ull-alientek-emmc-icm20608.dtsi)、[内核驱动](../kernel/icm20608_iio.c)、[IIO 适配器](../libiio/src/iio_sysfs.cpp) 与 [服务入口](../apps/conditiond.cpp)，把下列链路写在自己的笔记里：

```text
ECSPI3 设备树节点 → SPI 匹配/probe → WHO_AM_I → IIO 设备注册
→ sysfs name 查找 → 配置 scan/buffer → /dev/iio:deviceX 读帧
→ 物理量转换 → Monitor 状态更新 → JSON 输出
```

逐点提问自己：`compatible` 只负责匹配还是能证明芯片确实在线？`WHO_AM_I` 不符应在哪层失败？设备号变成 `iio:device7` 时用户态为什么仍能找到它？答案必须能指向代码，而不是“Linux 会自动处理”。

无硬件时只可检查代码与隔离编译记录，不能声称完成 probe、`insmod` 或真实采样。内核配置依赖和构建顺序见 [vendor 4.1.15 集成指南](../deploy/KERNEL_INTEGRATION.md)。

## L2：用确定性输入解释状态机

```bash
(cd build && ctest -R '^(condition-monitor|replay-fixture)$' --output-on-failure)
./build/apps/condition-replay tests/data/normal_then_impact.csv
```

[CSV 输入](../tests/data/normal_then_impact.csv)有 5 行有效样本：两行正常、两行连续高加速度、最后回到正常。先预测状态变化，再核对输出为 `normal → impact → normal`。在 [状态机测试](../tests/test_condition_monitor.cpp)中找出“单个毛刺不报警”“连续样本确认”“倾斜约 45°”“超时、NaN、时间倒退进入故障”“失联后重新累计确认窗口”的断言。

自己回答：为什么不能把一次 `az=24` 直接当冲击？为什么恢复后的第一次高加速度不能继承失联前的累计次数？这里的 `timestamp_ns` 要求单调，与系统墙上时间有什么区别？

面试表达边界：这是可复现的规则状态机，不是基于真实工况标定的预测性维护算法；倾角和冲击阈值尚需实物标定。

## L3：用伪 sysfs 练“配置事务”

```bash
(cd build && ctest -R '^iio-sysfs$' --output-on-failure)
```

[测试](../tests/test_iio_sysfs.cpp)在临时目录造出 `iio_device7`，随后调用 [适配器](../libiio/src/iio_sysfs.cpp)。先从代码预测写入顺序：先 `buffer/enable=0`，再采样率、长度和扫描元素，最后 `enable=1`。测试还把采样率文件指向 `/dev/full`，检查写入异常被报告且 buffer 保持关闭。

自己回答：为什么不能写死 `iio:device0`？若配置第三项失败，为什么不能保留 `enable=1`？`BufferGuard` 在异常退出时负责什么？

这个测试只验证伪文件树的用户态控制流程，不证明实际内核 IIO 属性存在、写入可用或设备能产生数据。

## L4：把任意 read 边界还原成完整 scan

```bash
(cd build && ctest -R '^scan-assembler$' --output-on-failure)
```

[组帧测试](../tests/test_scan_assembler.cpp)把 49 字节分成 `1 + 22 + 26` 字节送入 [ScanAssembler](../libiio/include/iio_adapter/scan_assembler.h)：前 23 字节不构成一帧，最后一次追加得到两帧并留下 1 字节；再补 23 字节得到第三帧。先手算每次 `pendingBytes()`，再运行测试。单次 `read()` 不保证正好一帧；服务循环还限制一次事件最多读 16 次，避免持续输入饿死 `timerfd`/`signalfd`。

自己回答：为什么测试用 24 字节一帧，而七个传感器原始通道只有 14 字节？检查 [内核驱动](../kernel/icm20608_iio.c) 的扫描布局、对齐填充和时间戳，不要猜测。`hrtimer` 回调为何只调用 trigger poll，SPI 读取为何放在线程化 handler？

## L5：证明进程、文件描述符与退出路径

```bash
./build/apps/conditiond --simulate --samples 30
(cd build && ctest -R '^conditiond-process$' --output-on-failure)
```

先从 [服务入口](../apps/conditiond.cpp)画出一个 `epoll` 实例监听的三类 fd：模拟/真实传感器输入、`timerfd`、`signalfd`。再阅读 [进程测试](../tests/test_conditiond_process.py)：有限样本检查 JSON 状态序列，另起进程分别发送 SIGTERM、SIGINT，要求正常返回；非法参数应返回 2。

自己回答：为什么信号先被阻塞再交给 `signalfd`，而不在异步信号处理器里做复杂清理？`UniqueFd` 和 `BufferGuard` 分别负责哪些资源？进程级测试证明了什么，为什么仍不能证明板端拔线或长期运行安全？

## Source 门槛与真正的 HIL 门槛

拿到与开发板运行版本匹配的 vendor Linux 4.1.15 干净副本后，依照 [集成指南](../deploy/KERNEL_INTEGRATION.md)配置 Kconfig、defconfig 和设备树，再编译内核、模块。确认 `CONFIG_ICM20608_IIO=m` 与 `CONFIG_IIO_TRIGGERED_BUFFER=m` 均出现在最终 `.config`，检查 `Module.symvers` 和模块链接没有未解析符号。这些是 **Source** 证据；只看见 `.ko` 文件不够，更不能替代板端加载。

有板后按 [五项目验证矩阵](FIVE_PROJECT_VALIDATION_MATRIX.md)记录硬件版本、内核/DTB/工具链、接线、`modinfo`/`dmesg`、raw/scale、buffer 帧、倾斜/冲击/断流、SIGTERM 和长稳原始日志。当前没有硬件，以上仍是 **TODO-HIL**，不要在简历或面试中写成已完成。

## 最后做一次“能讲清楚”自检

不看文档，按“产品问题 → 驱动匹配与 probe → IIO ABI → 用户态配置与读帧 → 状态机 → epoll 退出 → 已验证与未验证”顺序讲一遍。每一段至少指出一个本仓库文件和一项对应测试或尚缺的板端证据。如果只能说术语却找不到代码位置，回到上面对应实验；如果只能描述代码却说不出失败路径，回到测试断言。
