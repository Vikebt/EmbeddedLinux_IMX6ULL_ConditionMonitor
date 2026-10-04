# 五项目验证矩阵与复现入口

本页用于区分“代码存在”“宿主测试通过”“目标工具链通过”和“真实硬件通过”。后续任何简历或面试表述都以此为上限。

| 项目 | 已验证 | 复现入口 | 尚未验证 | 可安全表述 |
| --- | --- | --- | --- | --- |
| 手持体温检测仪 | 宿主 2/2、ARMCC 5.06u6 完整构建 0 错误 0 警告、map 显示 `LR_IROM1` 上限 `0xFC00` | `tests/host`、`Project/MDK-ARM/Obj/Listings/sud.map` | 温度/RFID/Flash/WiFi 实物链路与掉电恢复 | “主机测试与 Keil 构建通过，板端链路未验证” |
| 智能送药小车 | 宿主 1/1、ARMCC 5.06u6 完整构建 0 错误 0 警告；map 的 Flash/RAM 上限分别为 `0x10000` / `0x5000` | `tests/host`、`Listing/Fire_FreeRTOS.map` | 电机、OpenMV、HX711 实车标定与长稳 | “状态机测试与 Keil 构建通过，参数待实车标定” |
| 三维 LiDAR 感知 | 有界队列 DropOldest/Block、shutdown/唤醒；GitHub Linux Debug/Release 宿主 CI 各 1/1；ROS Noetic/Focal 完整 catkin 构建与 18 项 gtest 通过 | `src/lslidar_ls_driver/tests/host`、GitHub Actions `Host regressions` 与 `ROS Noetic integration` | ROS 节点运行、LS1550/Jetson 吞吐与丢包 | “并发队列和 ROS 算法测试通过，工程可构建；设备性能未验证” |
| 多源光电数据融合 | GitHub Linux Debug/Release 各 3/3（重连、导航 UDP 截断、YAML 事务回滚）；Windows MSVC Release 2/2；ROS Noetic/Focal 四包完整 catkin 构建通过 | `src/mainwindow/tests/host`、GitHub Actions `Host regressions` 与 `ROS Noetic integration` | ROS 节点运行、真实导航源/串口/RTSP 联调与退出时限 | “重连、配置事务及 UDP 报文边界测试通过，ROS 工程可构建；设备联调未验证” |
| I.MX6ULL 状态监测器 | Linux Debug/Release 各 6/6 Host（含模拟服务进程级退出）、Windows 2/2 Host、ARM 用户态交叉编译、隔离 vendor 4.1.15 内核联编与 ARM `.ko` 无未解析符号 | `tests`、`scripts/validate_driver_source.sh`、`deploy/KERNEL_INTEGRATION.md` | 目标板加载、真实 IIO 采样与 HIL | “Host、ARM 交叉编译及隔离内核联编通过；板端未验证” |

2026-10-03 的 GitHub 宿主 CI 记录：[P1](https://github.com/Vikebt/Handheld_Temperature_STM32F103C8T6/actions/runs/37106291838)、[P2](https://github.com/Vikebt/Smart_Medicine_Cart_STM32F103C8T6/actions/runs/37106328712)、[P3](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/actions/runs/37106343765)、[P4](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/actions/runs/37106357832)、[P5](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/actions/runs/37106372372)。五项作业均完成且结果为 success；它们只覆盖各自工作流列明的宿主测试，不覆盖 Keil 固件构建、ROS 全量构建或 HIL。

P3 的 [ROS Noetic 测试记录](https://github.com/Vikebt/EmbeddedLinux_3DLiDAR_Perception/actions/runs/37119796557) 同时包含完整构建与四组 gtest，汇总为 18 测试、0 失败；仍不包含 ROS 节点与雷达实机运行。

P4 的 [Linux 宿主测试记录](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/actions/runs/37121300009) 在 Debug/Release 各 3/3，通过回环 UDP 验证截断边界；[ROS Noetic/Qt 四包构建记录](https://github.com/Vikebt/EmbeddedLinux_MultiSource_OpticalDataFusion/actions/runs/37121299927) 为 success。这些结果仍不证明串口、RTSP 或 ROS 节点的实机运行行为。

Windows 上使用的 MinGW 7.3 无法可靠处理构建规则中的中文绝对路径。五个仓库的 Windows 测试均通过临时 ASCII 盘符映射复跑；直接调用编译器的最小实验也证明失败发生在 MinGW Makefiles 的路径解析层，而不是测试源码。Linux/WSL 构建不需要该映射。

## 统一完成定义

每个项目只有同时满足以下项，才可在简历上写“已验证”：

1. 对应层级的构建命令返回 0。
2. 测试输出包含预期断言，不以“能启动”代替结果检查。
3. 失败路径可解释：超时、队列满、断流、EOF、设备缺失或 probe 失败。
4. README、代码和面试表述一致，不存在虚构传感器数量、发布者或部署结果。
5. HIL 项只有实板日志、配置、版本与复现实验齐全时才改为 PASS。

## 无硬件阶段的边界与移交

当前没有可连接的 STM32、Jetson/传感器或 I.MX6ULL 实物。下列 P1–P5 上板项一律为 **TODO-HIL**；宿主测试、交叉编译、Keil 构建、ROS 构建以及模拟输入均不能替代它们。不要在简历、项目 README 或面试回答中说已经完成实机联调、性能标定或长稳测试。

上板前先记录目标硬件版本、接线/供电、固件或内核 commit、工具链、启动配置与可复现的输入。每一步保留原始串口/ROS/内核日志及测量结果，注明预期与实际；失败也要记录。没有产品指标的吞吐、时延、精度、CPU/内存等项目，应先确定验收目标，再据实测量，不补写猜测数值。合并草稿 PR 是代码评审决策，不等同于 HIL 通过。

## P1 手持体温检测仪：TODO-HIL

前提：与工程相符的 STM32F103C8T6 板和调试器、MLX90614、MFRC522、ESP8266、OLED、供电/电池采样链路；先按实际原理图核对引脚、电平与电源，不照搬未经确认的接线。

1. 烧录与冷启动：保存固件版本、map、启动日志，检查各任务和看门狗运行；观察复位后显示与无线连接的初始状态。
2. 正常链路：用可溯源的温度参考和已知 RFID 卡，分别记录温度读数、卡号、OLED 显示及 ESP8266 上传结果；核对一次采样对应的数据身份，不宣称未经标定的精度。
3. 异常链路：断开温度/RFID/WiFi 外设，观察超时、恢复及界面提示；在受控供电条件下复现 Flash 配置写入中断与损坏页启动，检查 CRC、旧配置保留或默认配置回退，并留存前后原始数据。
4. 运行观察：记录任务栈余量、复位原因、供电范围与重复测试次数；只有明确试验时长及日志后，才能写“长稳”。

## P2 智能送药小车：TODO-HIL

前提：对应 STM32 板与调试器、OpenMV、七路灰度传感器、HX711/称重机构、电机驱动及安全可控的路线；先校验电机方向、限位、地线和供电。

1. 静态联调：逐个检查 OpenMV 帧格式、灰度传感器值和 HX711 零点/砝码读数，保存原始帧及校准参数；不将宿主状态机测试当成传感器精度验证。
2. 受控路线：记录每次状态转换与电机输出，覆盖识别房号、循迹、转向、取放药品与停车；对照路线视频或日志核验结果。
3. 故障与安全：断开视觉输入、遮挡灰度传感器、制造称重异常或丢线，检查超时、停机与恢复是否符合状态机设计；先空载、低速测试，避免直接在人旁试车。
4. 时序与长稳：用实测数据核对控制周期抖动、任务栈余量、复位原因、续航与重复运行；具体合格阈值须由实车需求确定。

## P3 三维 LiDAR 感知：TODO-HIL

前提：目标 Jetson/ROS 环境、LS1550 或与实际驱动协议一致的雷达、网络连接和可保存的原始包；先确认设备型号、固件、坐标系及时间源。

1. 先用留存的数据包回放，再接实雷达；分别保存原始包计数、ROS 话题频率、`frame_id`、时间戳与点云样例，核对每帧身份传递。
2. 制造高负载与短时断流，观察有界队列的容量、丢弃计数、消费者唤醒和退出耗时；将实测吞吐、丢包、CPU/内存与温度和输入速率一起记录。
3. 用相同场景重复测试，区分雷达/网络丢包与应用队列丢弃。现有 18 项 gtest 只覆盖算法/组件，不证明 Jetson 上的实时性能。

## P4 多源光电数据融合：TODO-HIL

前提：目标 Jetson/ROS/Qt 环境及可控的 RTSP 源、导航 UDP 源、串口设备、激光雷达和写盘目标；保存设备型号、端口、波特率、网络拓扑及时间同步方式。

1. 各源独立启动：分别保存导航原始报文、串口读数、RTSP 首帧/断流日志与点云消息，核对字节序、长度、时间戳和数据单位。
2. 联合运行：按实际配置启动四个 ROS 包与界面，核对跨源时间关联、数据展示和三帧点云写盘上限；记录慢盘条件下的排队/丢弃行为，不以编译通过替代节点运行。
3. 故障注入：发送短包和超长导航 UDP 报文、断开/恢复串口与 RTSP、关闭界面或发送终止信号；记录重连、超时、线程退出及资源释放的实测耗时。
4. 保存 CPU/内存、磁盘吞吐、掉帧及连续运行日志；目标值由设备能力与产品需求确定，暂不宣称实机性能达标。

## 第五项目的上板闭环

1. 按 `deploy/KERNEL_INTEGRATION.md` 给 vendor 4.1.15 的干净副本接入驱动 Kconfig，再合并 `deploy/imx6ull_iio_defconfig.fragment`，检查隐藏的 `CONFIG_IIO_TRIGGERED_BUFFER` 后重新构建内核、DTB 和模块。
2. 合入 `deploy/imx6ull-alientek-icm20608.dtsi`，核对实际片选、引脚和最高频率。
3. 启动后检查 `dmesg`、IIO `name`、raw/scale/sampling_frequency。
4. 先运行 `conditiond --simulate`，再运行真实 IIO buffer 模式。
5. 依次测试静置、单点毛刺、持续倾斜、连续冲击、停流/拔除、SIGTERM 和 8 小时长稳。
6. 记录内核 commit、DTB、rootfs、工具链、采样率、丢帧/CPU/内存，再更新本矩阵。

## HIL 证据记录模板

每个项目单独建记录，至少包含：`项目/硬件版本/接线与供电/软件 commit/构建产物/工具链与系统版本/输入与操作步骤/预期/实际/原始日志或视频路径/测量方法与次数/结论 PASS、FAIL 或 TODO-HIL/遗留问题`。任何一项没有真实板端证据时保持 TODO-HIL；FAIL 不得改写成 PASS。恢复测试还应记录故障注入方式与恢复条件，长稳测试还应记录开始/结束时间和中途重启次数。
