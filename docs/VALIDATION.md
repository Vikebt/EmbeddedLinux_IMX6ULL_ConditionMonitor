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
| 完整 CMake/CTest | Host | PASS | ASCII 盘符映射下 3/3 tests passed（含 sysfs 夹具） |
| IIO 驱动对象交叉编译 | Source | PASS | vendor Linux 4.1.15 头文件 + Linaro ARM GCC 4.9.4，`CC [M] icm20608_iio.o` |
| 可加载 `.ko` 与目标内核联编 | Source | 未执行 | 当前 vendor `.config` 未启用 IIO buffer/trigger；需应用 defconfig fragment 后重编内核与模块 |
| I.MX6ULL 实板加载与采样 | HIL | 未执行 | 需要开发板与 ICM20608 |

Windows 下 MinGW Makefiles 无法可靠解析仓库路径中的中文字符；验证时将仓库临时映射为 ASCII 盘符。这是构建工具路径限制，不是测试降级，编译输入仍是本仓库源码。

驱动对象验证使用 `scripts/validate_driver_source.sh`。脚本通过编译宏补齐当前 `.config` 中尚未启用的 IIO trigger 声明，只证明代码与 4.1.15 API/ARM 编译器兼容；它不等同于可加载模块验证，文档刻意保留这一边界。

