# 面试证据索引

本文件只把已经落到代码、测试或明确验证步骤的知识点列为项目证据。硬件未验证项会保留为待办，不使用“已部署”“已量产”等表述。

| 面试问题 | 代码证据 | 可回答的工程结论 |
| --- | --- | --- |
| 设备树和驱动怎样匹配？ | `deploy/*.dtsi`、`icm20608_of_match` | SPI 控制器枚举子节点，OF compatible 匹配到 `spi_driver`，随后进入 probe |
| probe 失败怎样处理？ | `icm20608_probe` / `icm20608_hw_init` | 校验 WHO_AM_I，任何初始化错误立即返回；devm 避免半初始化路径手写多次释放 |
| 为什么用 regmap？ | `icm20608_regmap_config` | 集中表达 8 位寄存器、8 位值和 SPI 读标志，减少散落的总线协议细节 |
| 为什么用 IIO 而不是私有字符设备？ | `iio_chan_spec` / `iio_info` | 复用标准 raw/scale/offset/sampling_frequency ABI，用户态不依赖自定义 ioctl |

