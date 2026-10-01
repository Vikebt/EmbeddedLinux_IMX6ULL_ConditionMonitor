# 面试证据索引

本文件只把已经落到代码、测试或明确验证步骤的知识点列为项目证据。硬件未验证项会保留为待办，不使用“已部署”“已量产”等表述。

| 面试问题 | 代码证据 | 可回答的工程结论 |
| --- | --- | --- |
| 设备树和驱动怎样匹配？ | `deploy/*.dtsi`、`icm20608_of_match` | SPI 控制器枚举子节点，OF compatible 匹配到 `spi_driver`，随后进入 probe |
| probe 失败怎样处理？ | `icm20608_probe` / `icm20608_hw_init` | 校验 WHO_AM_I，任何初始化错误立即返回；devm 避免半初始化路径手写多次释放 |
| 为什么用 regmap？ | `icm20608_regmap_config` | 集中表达 8 位寄存器、8 位值和 SPI 读标志，减少散落的总线协议细节 |
| 为什么用 IIO 而不是私有字符设备？ | `iio_chan_spec` / `iio_info` | 复用标准 raw/scale/offset/sampling_frequency ABI，用户态不依赖自定义 ioctl |
| 状态机怎样避免抖动？ | `Monitor::update` 与单元测试 | 冲击、倾斜使用独立连续样本窗口；单点越界不会直接报警 |
| 为什么算法不写进内核？ | `libcondition` | 内核只提供稳定机制，策略在用户态可测试、可迭代，故障也不会危及内核 |
| 如何处理脏数据？ | `finite`、时间戳检查、`checkTimeout` | NaN、时间倒退、数据超时进入显式故障态，而不是继续沿用旧值 |
| sysfs 是什么？为什么不用固定设备号？ | `findDevice` | sysfs 是内核对象/属性的文本视图；枚举 `name` 可适应 probe 顺序变化 |
| 多个 sysfs 写入如何处理失败？ | `configureBuffer` | 先停 buffer、配置、最后启用；异常路径回到 disabled，形成最小可恢复事务 |
| 为什么 hrtimer 里不能读 SPI？ | `icm20608_timer_callback` / `icm20608_trigger_handler` | hrtimer 在原子上下文，SPI 可能睡眠；回调只触发 IIO，真正读取在线程化 poll handler |
| 中断上半部/下半部思想怎样落地？ | 软件 trigger → threaded handler | 快路径只记录事件并调度，慢路径完成总线 I/O 和 buffer push |
| 如何防止内核信息泄漏？ | `st->scan` 与 `memset` | IIO 时间戳要求 8 字节对齐，14 字节传感器数据后有 padding；push 前清零整个 scan buffer |

