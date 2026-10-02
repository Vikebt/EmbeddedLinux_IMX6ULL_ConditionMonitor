# 06｜五个项目怎样讲成可信的工程故事

项目介绍不是把技术名词串成一段，而是回答：**产品问题是什么、原设计哪里会失败、我怎样重构、用什么证据证明、还剩什么边界。**

## P1｜手持体温检测仪

### 产品与数据流

```text
温度传感器 ─┐
             ├─> FreeRTOS queues ─> 融合策略 ─> 显示/记录/上传
RFID 身份 ───┘                         │
                                     └─> Flash 配置与 CRC
```

目标不是“读取几个传感器”，而是只有新鲜温度、卡片在场和非零 UID 同时成立时，才形成有效业务记录。

### 重构前的风险

1. 空的 SVC handler 破坏 FreeRTOS 首任务启动链。
2. 队列满时行为不明确，生产任务可能被旧数据拖住。
3. 临界区包围慢 RTC/I2C 操作会增加中断延迟。
4. Flash CRC 覆盖不完整或版本校验不足，损坏配置可能被误用。
5. ADC/SPI 自检用假 PASS，使诊断结论不可信。
6. 默认配置和日志可能泄露网络凭据。

### 重构后的结构

- `SVC_Handler → vPortSVCHandler`，恢复 RTOS 正确启动路径。
- `prvSendLatest` 在队列满时丢旧保新，确保传感器链路有界。
- I2C 用 mutex，短临界区只复制标量，RTC 轮询移到外部。
- Flash 记录用 magic/version/tail/完整 CRC，自检无能力的项目明确 `SKIP`。
- `FusionPolicy_IsMeasurementReady` 成为纯函数并接受宿主测试。
- 仓库不存 SSID/密码，未配置网络时显式失败。

### 代码证据

| 证据 | 当前代码 | 固定证据 |
| --- | --- | --- |
| RTOS 启动 | [main](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/blob/main/Core/stm32f10x_it.c) | [study-step-1](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/blob/study-step-1-runtime-correctness/Core/stm32f10x_it.c) |
| 最新值队列 | [main](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/blob/main/Core/main.c) | [study-step-2](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/blob/study-step-2-testable-fusion/Core/main.c) |
| 融合与测试 | [fusion policy](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/tree/main/APP/DATA_FUSION) | [host test](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/blob/main/tests/host/test_fusion_policy.c) |

证据等级：`CODE`、`HOST`；外设真实时序与 WiFi 链路为 `TODO-HIL`。

### 30 秒介绍

> 这是 STM32F103 + FreeRTOS 的多源体温检测仪。我重构的核心是运行时正确性和数据一致性：修正 SVC 启动入口，传感器用有界最新值队列，I2C 用 mutex，临界区只保护短快照；温度与有效 RFID 同时满足才生成记录。Flash 加入版本和完整 CRC，敏感配置不入库。融合策略与 CRC 已通过宿主测试，外设时序仍按 HIL 管理。

### 2 分钟展开

先说业务记录为什么不能复用旧 UID，再画任务/队列数据流；解释 queue/mutex/critical section 的选择；最后讲 CRC 只能检测损坏，若继续提高掉电一致性会增加双槽、序号和提交标志。

### 典型追问

- **为什么不用全局变量？** 队列同时传递数据和同步，所有权与新鲜度更清楚。
- **CRC 能保证原子写吗？** 不能，只能检测；原子提交需双槽或日志结构。
- **为什么队列满丢旧？** 测温显示重视最新值；关键记录不能用同一策略。
- **mutex 和临界区怎么选？** 慢总线用 mutex，几个标量复制用短临界区。

### 不能说的内容

不能说已经证明传感器精度、WiFi 长稳、Flash 掉电原子性或整机量产可靠性。

## P2｜智能送药小车

### 产品与数据流

```text
OpenMV USART ISR ─> 最新视觉帧 ─┐
HX711 任务 ───────> 最新重量 ───┼─> 逻辑输入 ─> 非阻塞 FSM ─> 单一控制任务 ─> 电机
灰度传感器 ─────────────────────┘
```

### 重构前的风险

- ISR 里混入业务判断，延迟不可控。
- 多任务可能同时写电机，输出取决于抢占时序。
- 长 delay 完成转向，无法及时响应新事件。
- HX711 断线会无限等待 DOUT。
- Tick 绝对值比较在回绕时失效。

### 重构后的结构

- USART ISR 只收两字节、组帧、`xQueueOverwriteFromISR`。
- `vControlTask` 是电机唯一写入者。
- `CartState_Step` 以状态和进入时间推进转向/提示/掉头。
- HX711 所有等待有超时并返回有效位。
- `prvElapsed` 使用无符号差值，测试覆盖回绕。

### 代码证据

| 证据 | 当前代码 | 固定证据 |
| --- | --- | --- |
| ISR 边界 | [ISR](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/main/Core/stm32f10x_it.c) | [study-step-1](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/study-step-1-isr-boundary/Core/stm32f10x_it.c) |
| FSM | [cart_state.c](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/main/APP/cart_control/cart_state.c) | [study-step-2](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/study-step-2-nonblocking-fsm/APP/cart_control/cart_state.c) |
| 超时 | [hx711.c](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/main/APP/hx711/hx711.c) | [host test](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/blob/main/tests/host/test_cart_state.c) |

证据等级：`CODE`、`HOST`；机械参数、PID/阈值和 HX711 标定为 `TODO-HIL`。

### 30 秒介绍

> 这是 STM32F103 + FreeRTOS 的视觉循迹送药车。我把 USART 中断缩到组帧和队列通知，电机只由控制任务写入，再用时间驱动 FSM 替代长阻塞 delay。HX711 增加超时，Tick 计算覆盖回绕。宿主测试验证状态转移和输出命令，但实际转向时长、重量标定和循迹阈值仍需实车标定。

### 2 分钟展开

重点讲“单写者”如何消除竞争、FSM 如何提升可测试性，再给出 Tick 回绕数值例子。最后解释为什么 host test 不能验证机械惯量。

### 典型追问

- **为什么长度 1 队列？** 视觉/重量是状态观测，处理旧样本没有价值。
- **为什么不用多个任务分别控制左右电机？** 组合动作需要一致输出，单写者更容易维持不变量。
- **超时后怎么办？** 返回 invalid，状态机进入安全/保持策略；具体安全动作需产品定义。
- **FSM 会不会太复杂？** 状态增加，但逻辑可枚举、可测试、可中断，复杂度从时间隐式关系变成显式结构。

## P3｜三维 LiDAR 感知

### 产品与数据流

```text
UDP socket / PCAP
        ↓
Input 层（epoll、长度/来源检查）
        ↓
BoundedQueue<packet>  -- DropOldest / Block
        ↓
解析 / 点云处理 / ROS 发布
        ↓
metrics 快照与受控 shutdown
```

### 重构前的风险

- socket/epoll 构造中途失败可能泄漏 fd。
- PCAP 截断帧按固定偏移读取，EOF 使用 abort。
- 无界队列把持续过载变成内存增长和延迟失控。
- 条件变量未把 shutdown 放进谓词，线程退出可能挂住。
- 指标跨线程读写普通结构体，存在 data race。

### 重构后的结构

- 非阻塞 socket 和 epoll，所有失败分支与析构配对关闭。
- PCAP 对 caplen、过滤结果和 EOF 返回状态。
- 通用 `BoundedQueue` 支持 Block/RejectNewest/DropOldest。
- push/pop 用带谓词 condition_variable，shutdown notify_all。
- 指标在同一 mutex 下快照；流水线按依赖顺序 stop/join。

### 代码证据

| 证据 | 当前代码 | 固定证据 |
| --- | --- | --- |
| I/O 生命周期 | [input.cc](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/blob/main/src/lslidar_ls_driver/src/input.cc) | [study-step-1](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/blob/study-step-1-io-lifecycle/src/lslidar_ls_driver/src/input.cc) |
| 有界队列 | [bounded_queue.h](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/blob/main/src/lslidar_ls_driver/include/lslidar_ls_driver/core/bounded_queue.h) | [study-step-2](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/blob/study-step-2-overload-policy/src/lslidar_ls_driver/include/lslidar_ls_driver/core/bounded_queue.h) |
| 测试 | [test_bounded_queue.cpp](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/blob/main/src/lslidar_ls_driver/tests/host/test_bounded_queue.cpp) | 同路径 |

证据等级：`CODE`、`HOST`；ROS/PCL 全量构建和 LS1550/Jetson 性能为 `TODO-HIL`。

### 30 秒介绍

> 这是 ROS1/PCL 的 LiDAR 数据接入与点云流水线。我重构了 UDP/epoll 生命周期、PCAP 边界检查和并发队列：所有 fd 失败路径可回收，级间队列有容量与 DropOldest/Block 策略，shutdown 进入条件变量谓词，线程可被唤醒并 join。宿主测试覆盖过载和关闭语义，没有设备时不声称真实吞吐和丢包率。

### 2 分钟展开

先说明实时感知为什么选择丢旧，再讲 condition_variable 谓词与 stop 顺序；最后解释单 socket 使用 epoll 是架构一致性，而不是性能宣传。

### 典型追问

- **epoll 一定比 poll 好？** 不；当前单 fd 收益小，主要为了扩展统一事件循环。
- **DropOldest 会不会丢关键数据？** 会，因此只用于可被新帧替代的实时观测，且要记录 dropped。
- **为什么 EOF 不能 abort？** EOF 是离线输入生命周期的一部分，需让上层正常收尾。
- **如何验证没有 fd 泄漏？** 重复构造/销毁、错误注入并监控 `/proc/pid/fd`。

## P4｜多源光电数据融合

### 产品与数据流

```text
ROS callbacks ─┐
GNSS / gimbal ─┼─> mutex-protected latest snapshots ─> QNode 处理/显示/发送
pressure/radio ┤
RTSP ----------┘          │
                           └─> bounded point-save queue
```

### 重构前的风险

- detached spinner/工作线程可能在对象析构后继续访问。
- ROS AsyncSpinner 与 spinOnce 混用，回调所有权不清。
- 复合结果普通读写导致 data race 和跨帧字段。
- 四元数 x/y/z 被直接当欧拉角。
- RTSP 断流立即循环重连形成风暴。
- 点云落盘速度慢时队列可能无界增长。

### 重构后的结构

- QNode 成为 ROS 回调单一所有者，不使用 detached 工作线程。
- 单值控制标志用 atomic，复合传感器结果用 mutex 快照。
- tf2 正确完成 quaternion→Euler。
- RTSP 使用 250 ms 到 5 s 的封顶指数退避。
- 点云保存队列最多 3 帧，满时丢最旧。
- 所有线程 stop→join/wait→释放设备。

### 代码证据

| 证据 | 当前代码 | 固定证据 |
| --- | --- | --- |
| 生命周期与快照 | [qnode.cpp](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/blob/main/src/mainwindow/src/qnode.cpp) | [study-step-1](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/blob/study-step-1-owned-lifetimes/src/mainwindow/src/qnode.cpp) |
| RTSP 退避 | [rtsp_capture.cpp](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/blob/main/src/mainwindow/src/rtsp_capture.cpp) | [study-step-2](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/blob/study-step-2-consistent-snapshots/src/mainwindow/src/rtsp_capture.cpp) |
| 测试 | [host test](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/tree/main/src/mainwindow/tests/host) | 同路径 |

证据等级：`CODE`、`HOST`；ROS/Qt/OpenCV/PCL/串口/RTSP 联调为 `TODO-HIL`。

### 30 秒介绍

> 这是 ROS、Qt、RTSP、串口与点云融合应用。重构重点是线程生命周期和快照一致性：移除 detached 与混合回调调度，单值标志用 atomic，复合传感器结果用 mutex 整体复制；RTSP 使用封顶退避，点云落盘队列有容量。宿主测试覆盖退避，完整依赖环境和真实时间同步仍需目标系统验证。

### 2 分钟展开

用“GNSS 经度来自新帧、纬度来自旧帧”的反例讲快照；再用 destructor 与 detached 的时序讲悬空；最后说明为什么重连等待分小步以响应 stop。

### 典型追问

- **atomic 能代替 mutex 吗？** 只能保证单对象，不能保证一组字段不变量。
- **为什么不 detach？** 成员线程必须受对象生命周期控制，否则会 use-after-free。
- **为什么点云队列只有 3？** 限制慢磁盘对内存和实时链路的影响，保留较新帧。
- **四元数为什么不能直接取 xyz？** xyz 是虚部，不是欧拉角；需按旋转矩阵转换并明确坐标系。

## P5｜I.MX6ULL 设备姿态与异常冲击监测器

### 产品边界

这是基于正点原子 I.MX6ULL 学习经历独立构建的教学/作品项目，场景是仓储搬运车或户外控制箱的姿态、冲击和传感器失联监测。它不是公司量产经历，也不是预测性维护 AI。

### 分层结构

```text
DTS / ECSPI3
      ↓
SPI driver + regmap
      ↓
IIO direct / trigger / buffer
      ↓
sysfs + /dev/iio:deviceX
      ↓
conditiond: epoll(IIO, timerfd, signalfd)
      ↓
libcondition: tilt / impact / sensor_fault
```

### 为什么这样构建

1. 先做标准 IIO direct mode，证明设备模型和 ABI。
2. 把算法拆成纯库，用确定 CSV 测试，不等硬件。
3. sysfs 设备发现按 name，不写死 device0；配置形成可回滚事务。
4. 没有 DRDY 线时用 hrtimer trigger，但 SPI 读放线程化 handler。
5. 用户态用 epoll 收口三个 fd，减少线程与退出复杂度。
6. 板端选择 C++14，服从 Linaro GCC 4.9，而不是追逐 C++17。

### 代码证据

| 层级 | 当前代码 | 固定证据 |
| --- | --- | --- |
| L1 IIO direct | [driver](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/kernel/icm20608_iio.c) | [study-step-1](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/study-step-1-iio-direct-mode/kernel/icm20608_iio.c) |
| L2 算法回放 | [libcondition](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/main/libcondition) | [study-step-2](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/study-step-2-domain-replay/libcondition) |
| L3 sysfs | [libiio](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/main/libiio) | [study-step-3](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/study-step-3-sysfs-integration/libiio) |
| L4 trigger/buffer | [driver](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/kernel/icm20608_iio.c) | [study-step-4](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/study-step-4-threaded-buffer/kernel/icm20608_iio.c) |
| L5 epoll | [conditiond](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/apps/conditiond.cpp) | [study-step-5](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/study-step-5-epoll-service/apps/conditiond.cpp) |

证据等级：`CODE`、`HOST`、`CROSS`；可加载 `.ko` 和实板采样为 `TODO-HIL`。

### 30 秒介绍

> 这是我基于 I.MX6ULL 学习经历独立构建的 Linux 驱动作品。ICM20608 通过 SPI/regmap 接入 IIO，hrtimer 只发 trigger，可能睡眠的 bulk read 在线程化 handler；用户态按 sysfs name 找设备，用 epoll 统一 IIO、timerfd、signalfd，纯状态机判断倾斜、冲击和失联。Linux 4/4 测试、ARM 用户态和 vendor 4.1.15 驱动对象交叉编译通过，`.ko` 与实板仍明确待验证。

### 2 分钟展开

先讲为什么从私有字符例程升级到 IIO，再画 timer→threaded handler→buffer→epoll；随后解释内核/用户策略分层和 C++14 工具链取舍；最后诚实说明源码级与 HIL 的差距。

### 典型追问

- **为什么不用字符设备？** IIO 已有传感器 channel/scale/trigger/buffer ABI。
- **为什么 timer 里不读 SPI？** hrtimer 是原子上下文，SPI 可能睡眠。
- **为什么用户态不写死 device0？** probe 顺序不稳定，按 name 枚举。
- **为什么算法不放内核？** 策略变化快、易失败，用户态更可测且隔离更好。
- **你真的上板了吗？** 没有；当前证据是 host 与 source/cross，HIL 计划写在验证矩阵。

## 五项目横向对比

| 主题 | P1 | P2 | P3 | P4 | P5 |
| --- | --- | --- | --- | --- | --- |
| 事件入口 | 传感器任务 | USART ISR/周期任务 | UDP/PCAP | ROS/串口/RTSP | hrtimer/IIO fd |
| 并发边界 | FreeRTOS queue/mutex | ISR→queue→单控制任务 | thread+bounded queue | atomic/mutex snapshot | 单线程 epoll + threaded handler |
| 过载策略 | 丢旧保新 | overwrite 最新值 | DropOldest/Block | 3 帧点云队列 | drain + pending |
| 生命周期 | RTOS 创建断言/hooks | 传感器超时/FSM | shutdown+join | stop+join | RAII/devm |
| 主要验证 | 融合/CRC host | FSM/tick host | queue host | backoff host | host+ARM cross+driver object |
| 未验证 | 外设/WiFi | 实车参数 | ROS/设备吞吐 | 完整联调 | `.ko`/HIL |

## 面试时的真实性规则

1. “我重构了”后必须能指出文件、函数和验证。
2. “我选择了”后必须说明被放弃方案和代价。
3. “测试通过”后必须说明测试级别。
4. “没有做过”之后可解释迁移方案，但不能改口成“项目里就是这样”。
5. 任何精度、吞吐、延迟、长稳数字都要有实验记录。
