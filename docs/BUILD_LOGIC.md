# 构建逻辑：从学习例程到可解释项目

## 1. 产品问题先于驱动形式

选择“设备姿态与异常冲击监测”，是因为 ICM20608 的加速度与角速度能直接支持倾斜、运动和撞击判断；这与传感器能力匹配，也便于在无硬件时用确定性数据回放。这里不宣称故障预测，只报告能够从当前数据证实的状态。

## 2. 分层原则

- 内核层只负责设备发现、寄存器访问、采样与 IIO ABI，不承载业务阈值。
- 领域层只接收带时间戳的物理量，能在 x86 主机测试。
- 集成层负责 sysfs 与 `/dev/iio:deviceX`，不复制算法。
- 服务层用一个 `epoll` 循环管理多个 fd，避免“每种事件一个线程”。

后续章节会随 L1–L5 逐层补齐，每层都给出为什么这样设计、如何验证、面试时如何表达。

## 3. L1：先让设备成为标准 IIO 设备

驱动不再复刻学习例程中的私有字符设备 ABI，而是使用 Linux IIO：

1. 设备树负责描述 ECSPI3 上的芯片，`compatible` 只做驱动匹配。
2. SPI core 调用 `probe`，驱动用 devm 分配 IIO 设备和 regmap。
3. `WHO_AM_I` 失败直接终止 probe，避免“节点存在就算成功”。
4. regmap 统一寄存器读写与 SPI 读标志，`read_raw` 只负责 IIO 语义。
5. `remove` 先从 IIO core 注销；devm 资源随后按生命周期自动回收。

这一步先实现直读和采样率 sysfs，便于把“总线、驱动模型、设备树、sysfs、IIO ABI”串成一条可解释链路。缓冲采样留到 L4，防止一次引入过多机制。

## 4. L2：先用确定性数据证明业务规则

`libcondition` 不包含 ROS、Qt、IIO 路径或系统调用。输入只是“带单调时间戳的物理量”，因此可在 PC 上验证：

- 姿态由重力向量计算 roll/pitch，不把原始寄存器值直接当角度。
- 冲击和倾斜都设置连续样本确认窗口，抑制单点毛刺。
- NaN、时间戳倒退和长时间无新样本进入 `sensor_fault`。
- 状态优先级为故障 > 冲击 > 倾斜 > 正常。

模拟器生成固定冲击序列，回放器读取 CSV 并只打印状态变化。相同输入产生相同输出，便于调试和面试现场演示；它不冒充真实传感器噪声模型。

## 5. L3：把 sysfs 配置做成可回滚事务

设备号可能随启动顺序变化，所以用户态遍历 `/sys/bus/iio/devices` 并读取 `name`，而不是写死 `iio:device0`。配置顺序也刻意固定：

1. 先写 `buffer/enable=0`，禁止边采样边改扫描布局。
2. 配置 `sampling_frequency`、buffer length 和所有 scan element。
3. 最后写 `buffer/enable=1`。
4. 任一步失败都尝试恢复 `enable=0`，避免留下“部分配置但仍在运行”的设备。

`libiio_adapter` 接收可注入的 sysfs/dev 根目录，因此在 PC 临时目录中也能验证设备发现、设备号解析、配置顺序的最终状态和关闭行为。

## 6. L4：定时器只发通知，睡眠总线放在线程上下文

开发板原理图没有为 ICM20608 提供可用数据就绪中断，因此驱动注册一个基于 hrtimer 的 IIO trigger。关键上下文边界是：

```text
hrtimer 回调（原子上下文）
    └─ iio_trigger_poll()          只通知，不访问 SPI
          └─ IIO threaded handler（可睡眠上下文）
                 ├─ regmap_bulk_read() 一次读取 14 字节
                 ├─ 添加时间戳并 push 到 IIO buffer
                 └─ iio_trigger_notify_done()
```

SPI 传输可能睡眠，所以绝不能放进 hrtimer 回调。驱动还用 `available_scan_masks` 限定为完整七通道布局，清零对齐填充字节，避免把未初始化内核内存送到用户态。修改采样率时若 buffer 正在运行则返回 `-EBUSY`，保证 timer period 不被无锁并发修改。

## 7. L5：单线程 epoll 服务收口

`conditiond` 把三类事件注册到一个 epoll 实例：

- IIO 字符设备：非阻塞读取 24 字节 scan，处理一次 read 返回多帧或残留半帧。
- `timerfd`：周期检查数据是否超时；模拟模式还用另一个 timerfd 产生样本。
- `signalfd`：将 SIGINT/SIGTERM 变成普通 fd 可读事件，统一走退出路径。

文件描述符用不可复制、可移动的 RAII 类型管理。退出或异常时，局部对象按逆序析构：关闭 epoll/设备 fd，并由 `BufferGuard` 将 IIO buffer 关闭。这样无需信号处理器修改复杂共享状态，也无需为每个事件源创建线程。

模拟模式仍走真实 epoll、timerfd 和状态机，只替换传感器输入，能够验证进程/文件描述符/事件循环知识；它不证明真实 IIO 数据格式与时序已经上板通过。

## 8. 工具链取舍

最初宿主实现使用 C++17 filesystem，但本地 I.MX6ULL Linaro GCC 4.9 无法提供该标准库。最终板端主体改为 C++14，sysfs 枚举使用 `opendir/readdir`，并以 `unique_ptr<DIR, Deleter>` 保留 RAII；只有宿主测试夹具使用 C++17 filesystem。这个取舍不是退步，而是明确区分“产品目标工具链”和“测试便利性”。

