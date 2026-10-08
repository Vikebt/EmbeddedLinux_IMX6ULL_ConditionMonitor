# 验证记录与边界

## 验证分级

- **Host**：x86/Windows 或 Linux 宿主机可运行的算法、回放和事件循环测试。
- **Source**：针对 vendor kernel 4.1.15 头文件/接口的源码或交叉编译验证。
- **HIL**：I.MX6ULL + ICM20608 实板验证。

任何尚未执行的 HIL 项目都不会写成通过。

## 当前结果

| 项目 | 级别 | 结果 | 证据 |
| --- | --- | --- | --- |
| `libcondition` 编译 | Host | PASS | MinGW GCC 7.3，`-Wall -Wextra -Wpedantic` |
| 状态机单元测试 | Host | PASS | 正常、毛刺确认、冲击、倾斜、超时、NaN、时间倒退 |
| CSV 回放 | Host | PASS | 5 条接受、0 条拒绝，状态 normal → impact → normal |
| Windows CMake/CTest | Host | PASS | ASCII 盘符映射下 2/2 tests passed（领域层与回放） |
| Linux CMake/CTest | Host | PASS | WSL GCC 9.4，Debug 和 Release 各 6/6 tests passed，含扫描帧拆包、伪 sysfs、epoll 模拟服务与进程级退出测试 |
| `conditiond` 进程级测试 | Host | PASS | 模拟 30 次采样的 JSON 状态为 normal → impact → normal；SIGTERM/SIGINT 后返回 0；非法参数返回 2。这不覆盖真实 IIO 设备或目标板退出耗时 |
| IIO 扫描帧拆包 | Host | PASS | 单字节与半帧输入、多帧合并、最多保留 23 字节残帧；单次事件最多读取 16 次，避免持续输入独占事件循环 |
| 传感器失联确认窗口 | Host | PASS | 超时清空冲击/倾斜累计，恢复后必须重新累计确认样本 |
| sysfs 写入失败 | Host | PASS | 伪 sysfs 指向 `/dev/full`，确认写入错误传播且 buffer 保持关闭 |
| IIO 温度比例 | Source | 已修改，待模块验证 | 按 milli-°C ABI 输出每 LSB 约 3.059976 m°C，不能仅凭源码推断板端读数正确 |
| `conditiond` ARM 交叉编译 | Source | PASS | Linaro GCC 4.9.4，`-std=c++14 -Werror`，生成 ARM EABI5 ELF |
| IIO 驱动对象交叉编译 | Source | PASS | vendor Linux 4.1.15 头文件 + Linaro ARM GCC 4.9.4，`CC [M] icm20608_iio.o` |
| 与 vendor 内核联编及符号解析 | Source | PASS（隔离副本） | 干净 vendor 4.1.15 快照接入驱动 Kconfig 后，`olddefconfig` 生成 `IIO_TRIGGERED_BUFFER=m`；完整内核 `zImage modules` 构建、导出符号预检查和外部模块构建通过，产物为 ARM EABI5 `.ko`，无 modpost 未解析符号警告 |
| 板端 `.ko` 加载与 ABI 匹配 | HIL | 未执行 | 尚未在真实板端核对 `vermagic`、运行内核配置和 `insmod/dmesg` |
| I.MX6ULL 实板加载与采样 | HIL | 未执行 | 需要开发板与 ICM20608 |

Windows 下 MinGW Makefiles 无法可靠解析仓库路径中的中文字符；验证时将仓库临时映射为 ASCII 盘符。这是构建工具路径限制，不是测试降级，编译输入仍是本仓库源码。

驱动对象验证使用 `scripts/validate_driver_source.sh`。脚本通过编译宏补齐当前 `.config` 中尚未启用的 IIO trigger 声明，只证明代码与 4.1.15 API/ARM 编译器兼容。首次隔离构建发现：单独在 fragment 中写隐藏的 `CONFIG_IIO_TRIGGERED_BUFFER` 无效，`olddefconfig` 会移除它，导致 `.ko` 有未解析符号。修正后按 `deploy/KERNEL_INTEGRATION.md` 在干净内核快照接入 Kconfig，重新构建并通过 `scripts/check_kernel_module_config.sh`，未解析符号警告消失。此结果仍不等同于目标板加载或实测采样。

Linux 用户态验证可通过 `scripts/validate_userspace.sh` 复现。脚本兼容随当前 WSL 发行版安装的旧 CTest：进入构建目录后执行测试，而不依赖较新的 `ctest --test-dir` 参数；提供 `CROSS_COMPILE` 时还会交叉构建并用 `file` 打印产物架构。

