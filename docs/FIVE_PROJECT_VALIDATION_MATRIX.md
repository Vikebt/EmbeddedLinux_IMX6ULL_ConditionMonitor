# 五项目验证矩阵与复现入口

本页用于区分“代码存在”“宿主测试通过”“目标工具链通过”和“真实硬件通过”。后续任何简历或面试表述都以此为上限。

| 项目 | 已验证 | 复现入口 | 尚未验证 | 可安全表述 |
| --- | --- | --- | --- | --- |
| 手持体温检测仪 | 融合策略、标准 CRC 向量、队列过载策略源码检查 | `tests/host` | Keil 全量构建、温度/RFID/Flash/WiFi 实物链路 | “完成主机策略测试与工程结构校验” |
| 智能送药小车 | FSM、命令输出、Tick 回绕 | `tests/host` | 电机、OpenMV、HX711 实车标定与长稳 | “状态机主机测试通过，参数待实车标定” |
| 三维 LiDAR 感知 | 有界队列 DropOldest/Block、shutdown/唤醒 | `src/lslidar_ls_driver/tests/host` | ROS/PCL 全量构建、LS1550/Jetson 吞吐与丢包 | “并发队列语义已测试，设备性能未验证” |
| 多源光电数据融合 | RTSP 退避序列、源码一致性检查 | `src/mainwindow/tests/host` | ROS/Qt/OpenCV/PCL/串口/RTSP 联调 | “生命周期与退避策略已验证，集成待目标环境” |
| I.MX6ULL 状态监测器 | Linux 4/4 Host、Windows 2/2 Host、ARM 用户态交叉编译、vendor 4.1.15 驱动对象编译 | `tests`、`scripts/validate_driver_source.sh` | 目标 defconfig 后 `.ko` 联编、加载、真实 IIO 采样与 HIL | “Host 与 Source 级通过，HIL 未执行” |

Windows 上使用的 MinGW 7.3 无法可靠处理构建规则中的中文绝对路径。五个仓库的 Windows 测试均通过临时 ASCII 盘符映射复跑；直接调用编译器的最小实验也证明失败发生在 MinGW Makefiles 的路径解析层，而不是测试源码。Linux/WSL 构建不需要该映射。

## 统一完成定义

每个项目只有同时满足以下项，才可在简历上写“已验证”：

1. 对应层级的构建命令返回 0。
2. 测试输出包含预期断言，不以“能启动”代替结果检查。
3. 失败路径可解释：超时、队列满、断流、EOF、设备缺失或 probe 失败。
4. README、代码和面试表述一致，不存在虚构传感器数量、发布者或部署结果。
5. HIL 项只有实板日志、配置、版本与复现实验齐全时才改为 PASS。

## 第五项目的上板闭环

1. 给 vendor 4.1.15 应用 `deploy/imx6ull_condition_monitor_defconfig.fragment`，重新构建内核、DTB 和模块。
2. 合入 `deploy/imx6ull-alientek-icm20608.dtsi`，核对实际片选、引脚和最高频率。
3. 启动后检查 `dmesg`、IIO `name`、raw/scale/sampling_frequency。
4. 先运行 `conditiond --simulate`，再运行真实 IIO buffer 模式。
5. 依次测试静置、单点毛刺、持续倾斜、连续冲击、停流/拔除、SIGTERM 和 8 小时长稳。
6. 记录内核 commit、DTB、rootfs、工具链、采样率、丢帧/CPU/内存，再更新本矩阵。
