# 05｜Linux 内核、设备模型与 IIO 驱动

本章对应原题 190～216。它以 P5 为主证据，完整串起：

```text
设备树节点
  → SPI core 创建 spi_device
  → of_match 匹配 spi_driver
  → probe + regmap + WHO_AM_I
  → IIO channel / trigger / buffer
  → sysfs + /dev/iio:deviceX
  → conditiond epoll
  → 用户态状态机
```

## 1. 内核职责与上下文

### B01｜原题 190：Linux 内核五大子系统

常见回答是进程调度、内存管理、虚拟文件系统、网络和设备驱动。更重要的是说明边界：P5 的驱动通过 SPI/IIO/VFS 暴露设备机制，用户态服务通过调度与虚拟内存运行；项目没有修改内核核心子系统。

### B02｜原题 191、192：内核构建与启动

内核构建通常包含选择架构与交叉工具链、配置、编译 Image/zImage、DTB 和模块，再与 bootloader/rootfs 配套部署。启动链可概括为 bootloader 准备 CPU/内存和启动参数，内核解压、初始化体系结构/内存/调度/驱动，挂载根文件系统并启动 PID 1。

P5 只验证驱动对象与 vendor 4.1.15 API/ARM 编译器兼容；当前 vendor `.config` 未启用 IIO buffer/trigger，因此不能声称完成了完整可加载 `.ko`。

### A01｜原题 193、194：上下文、内核态与用户态

- **一句话结论**：上下文决定当前可以睡眠、可以访问什么、失败如何返回；用户态受隔离保护，通过系统调用进入内核态。
- **从零解释**：同一段“读传感器”逻辑放在进程、线程化中断或 hrtimer 回调中，允许的操作完全不同。
- **底层机制**：原子上下文不能调度睡眠；进程上下文可以阻塞。内核地址错误可能影响整个系统，用户进程错误通常被隔离。
- **项目场景**：P5 hrtimer 只触发 IIO poll，SPI bulk read 放在线程化 handler；业务算法留用户态。
- **代码证据**：P5 `kernel/icm20608_iio.c::icm20608_timer_callback/trigger_handler`。证据：`CODE`、`CROSS`。
- **工程取舍**：内核只实现稳定机制，策略在用户态迭代，降低故障影响和测试成本。
- **常见误区**：把“内核态更快”当成把算法放内核的理由；忽略可睡眠上下文。
- **30 秒回答**：项目里最关键的上下文边界是 hrtimer 不可睡眠，所以只发 trigger；SPI 读取在线程化 handler，倾斜/冲击判断在用户态。
- **2 分钟展开**：比较进程上下文、中断上下文、softirq/workqueue/threaded IRQ。

### B03｜原题 195、196：用户空间如何进入内核

用户空间与内核通信方式包括系统调用、设备文件 read/write/ioctl/mmap、sysfs/procfs/netlink 等。`open()` 经 libc 包装进入系统调用，VFS 根据路径查找 dentry/inode，得到 file，并通过 file_operations 到具体驱动。

P5 使用 sysfs 配置 IIO 属性、设备节点读取连续 scan；没有自定义 ioctl。IIO core 屏蔽了大量字符设备框架细节，因此面试时应说明“走标准 IIO ABI”，而不是冒充自己实现全部 VFS 路径。

## 2. 中断上下半部与内核同步

### A02｜原题 197、198：为什么分上下半部，如何实现

- **一句话结论**：中断快路径只完成必须立即处理的工作，耗时或可睡眠操作延后；常见机制有 threaded IRQ、workqueue、softirq/tasklet 等，具体选择取决于上下文需求。
- **从零解释**：硬中断长期占用会提高全系统中断延迟，且不能执行可能睡眠的 SPI/I2C。
- **底层机制**：P5 无 DRDY 引脚，以 hrtimer 产生 trigger；IIO pollfunc 在线程化上下文执行 regmap_bulk_read。
- **项目场景**：timer callback 等价承担通知快路径，trigger handler 承担慢路径。
- **代码证据**：P5 `icm20608_timer_callback`、`icm20608_trigger_handler`。证据：`CODE`、`CROSS`。
- **工程取舍**：有真实 DRDY 时可改硬件 IRQ；当前板级经历没有可用中断线，因此软件 trigger 更诚实。
- **常见误区**：把 Linux softirq 与 Cortex-M SVC 混淆；在 hrtimer 中直接调用 SPI。
- **30 秒回答**：因为 SPI 可能睡眠，不能放在 hrtimer 原子上下文。我的 timer 回调只调用 `iio_trigger_poll`，真正 bulk read 在线程化 poll handler 完成。
- **2 分钟展开**：比较 workqueue 与 threaded IRQ，并说明为什么 tasklet 不适合可睡眠 I/O。

### A03｜原题 199～201：内核同步、自旋锁与信号量

- **一句话结论**：自旋锁不可睡眠，适合极短原子上下文；mutex/semaphore 可能睡眠，只能在可睡眠上下文；中断与进程共享数据还需选择 irqsave 等变体。
- **从零解释**：持自旋锁睡眠会让其他 CPU/上下文一直自旋并破坏调度；中断上下文自身不能睡眠。
- **底层机制**：禁抢占/中断与自旋锁的组合取决于共享者可能在哪些上下文出现。
- **项目场景**：P5 避免在 timer 与线程 handler 之间无锁修改周期：buffer 运行时修改频率返回 `-EBUSY`，状态由 IIO buffer 生命周期串行化。
- **代码证据**：P5 `icm20608_write_raw` 与 buffer 状态检查。证据：`CODE`、`CROSS`。
- **工程取舍**：能通过状态机/框架生命周期消除共享，就不急于增加自旋锁；若未来动态改频率需明确锁和上下文。
- **常见误区**：说“信号量不能用于中断”却不说明因为会睡眠；把用户态 semaphore 与内核 semaphore 混为一谈。
- **30 秒回答**：自旋锁用于不可睡眠的短临界区，mutex/semaphore 会睡眠，不能在硬中断。项目通过禁止 buffer 运行时改采样率，避免 timer period 并发修改。
- **2 分钟展开**：说明 spin_lock_irqsave、mutex、completion、atomic_t 和 RCU 的适用边界。

### B04｜原题 202、203、205：内核内存与 MMU

内核常见分配包括 `kmalloc`（物理连续的小对象）、`vmalloc`（虚拟连续）、页分配器和 slab；GFP flags 表达是否可睡眠等上下文约束。P5 大部分资源使用 `devm_*`，底层仍依赖内核分配器，但项目没有实现内存管理器。

MMU 提供虚拟到物理映射、权限和隔离。驱动中的设备地址、DMA 地址与用户虚拟地址不能混用。

## 3. 模块、设备模型与设备树

### A04｜原题 207：模块加载、卸载与生命周期

- **一句话结论**：模块加载执行注册入口，卸载执行注销出口；真正设计重点是引用关系、并发使用者和逆序释放，而不是只背 module_init/module_exit。
- **从零解释**：注册 driver 后，匹配设备会调用 probe；注销时会触发 remove。模块仍被使用时不能安全卸载。
- **底层机制**：P5 用 `module_spi_driver` 生成注册/注销样板，probe/remove 处理设备实例。
- **项目场景**：驱动注册 IIO device 与 trigger；remove 先注销对外接口，再让 devm 回收资源。
- **代码证据**：P5 `kernel/icm20608_iio.c`。证据：`CODE`、`CROSS`，加载卸载为 `TODO-HIL`。
- **工程取舍**：devm 减少 probe 失败分支，但对外可见对象的注销顺序仍要明确。
- **常见误区**：认为 devm 意味 remove 什么都不用做；把对象编译成功写成 insmod 成功。
- **30 秒回答**：项目用 module_spi_driver 注册，设备匹配后 probe，卸载时 remove。当前只做 vendor 4.1.15 对象编译，没有声称完成 insmod/rmmod。
- **2 分钟展开**：说明注册驱动与创建设备的区别、引用计数和卸载竞态。

### B05｜原题 208～211：字符/块/网络设备和 cdev

字符设备按字节/请求访问，块设备面向可随机访问块并进入块层，网络设备通过 net_device 与网络栈交互。传统字符设备涉及 dev_t、cdev、class/device 和 file_operations。

P5 没有自建 cdev，而是注册 IIO device，由 IIO core 提供 sysfs 和 `/dev/iio:deviceX`。因此这些题必须标为 `RELATED`：理解框架差异，但不能把 IIO core 的工作说成自己手写了 cdev。

### A05｜原题 212：总线-设备-驱动模型

- **一句话结论**：字符设备描述用户 ABI；总线设备驱动模型描述设备发现、匹配、probe/remove 和电源/生命周期，两者可在一个驱动中同时出现。
- **从零解释**：SPI core 管理 spi_device 与 spi_driver 匹配；IIO core 管理传感器 ABI。
- **底层机制**：设备树创建 SPI device，`of_match_table` 匹配 compatible，core 调 probe。
- **项目场景**：P5 是 SPI driver + IIO device，不是简单的“字符驱动”。
- **代码证据**：P5 `icm20608_of_match`、`icm20608_driver`、`icm20608_probe`。证据：`CODE`、`CROSS`。
- **工程取舍**：复用标准子系统能获得一致 ABI 和工具生态，减少私有 ioctl。
- **常见误区**：说设备树直接“加载模块”；它主要描述硬件并参与匹配。
- **30 秒回答**：SPI 总线模型解决设备和驱动如何匹配，IIO 解决传感器怎样暴露给用户；这两个维度不是二选一。
- **2 分钟展开**：画 bus/device/driver 与 class/subsystem 的关系。

### A06｜设备树到 probe 的完整路径

- **一句话结论**：DTS 描述板级连接，内核解析后创建设备，compatible 与驱动匹配表相交才调用 probe。
- **从零解释**：节点包含控制器父子关系、片选、最大频率等资源，不应放业务阈值。
- **底层机制**：ECSPI3 控制器先就绪，子节点生成 spi_device；SPI core 设置模式/频率并匹配 driver。
- **项目场景**：P5 提供 ALIENTEK 板级 dtsi fragment，compatible 为 `invensense,icm20608`。
- **代码证据**：P5 `deploy/*.dtsi`、`kernel/icm20608_iio.c::of_match`。证据：`CODE`、`CROSS`，真实 DTB 为 `TODO-HIL`。
- **工程取舍**：板级引脚/片选留在 DTS，芯片通用逻辑留在 driver。
- **常见误区**：compatible 随意写成文件名；驱动代码硬编码板级 GPIO。
- **30 秒回答**：设备树描述 ICM20608 挂在 ECSPI3 的片选和频率，SPI core 创建 device，compatible 匹配后进入 probe。
- **2 分钟展开**：说明 controller probe 顺序、deferred probe 和资源获取失败。

### A07｜probe 为什么先读 WHO_AM_I

- **一句话结论**：总线通信成功且芯片身份正确是建立后续 ABI 的前提；失败必须让 probe 返回错误，不能创建一个假的可用设备。
- **从零解释**：设备节点存在只说明描述存在，不证明焊接、片选、模式和供电正确。
- **底层机制**：regmap read 返回通信状态和值，驱动检查 0xAF，再执行复位和配置。
- **项目场景**：P5 `icm20608_hw_init` 失败直接终止 probe。
- **代码证据**：P5 `kernel/icm20608_iio.c`。证据：`CODE`、`CROSS`；真实返回值为 `TODO-HIL`。
- **工程取舍**：fail-fast 避免用户态看到节点后读取垃圾；日志包含期望/实际值便于定位。
- **常见误区**：忽略返回码只比较值；失败后仍注册设备。
- **30 秒回答**：probe 先通过 regmap 读取 WHO_AM_I=0xAF，任何通信或身份错误都返回，不让 IIO 节点处于半初始化状态。
- **2 分钟展开**：列出供电、片选、CPOL/CPHA、频率、复位时序的排查顺序。

### A08｜devm 与 probe 失败回滚

- **一句话结论**：devm 把资源释放绑定到 device 生命周期，简化多出口回滚；但注册顺序、外部可见状态和非 devm 资源仍需设计。
- **从零解释**：probe 第五步失败时，前四步资源必须释放，手写 goto 容易遗漏。
- **底层机制**：device-managed action 在 probe 失败或 device detach 时逆序执行。
- **项目场景**：P5 用 devm 分配 IIO device、regmap 和 trigger，错误码逐层返回。
- **代码证据**：P5 `icm20608_probe`。证据：`CODE`、`CROSS`。
- **工程取舍**：devm 提高清晰度；需要早于 device detach 释放或复杂共享的资源可能仍手工管理。
- **常见误区**：把所有指针都 devm 后忽略用户并发；重复手工 free devm 资源。
- **30 秒回答**：probe 每步检查返回码，devm 负责已取得资源的逆序回收，因此中途失败不会留下半初始化内存；IIO 对外注册仍保持清晰顺序。
- **2 分钟展开**：对比 goto unwind，并说明为什么 remove 仍要处理对外状态。

## 4. SPI、regmap 与 IIO

### A09｜为什么使用 regmap

- **一句话结论**：regmap 把寄存器宽度、值宽度、读标志和访问 API 统一起来，隔离总线协议细节。
- **从零解释**：SPI 芯片常在寄存器地址高位编码读写方向，散落手工拼字节会重复且易错。
- **底层机制**：配置 `reg_bits/val_bits/read_flag_mask`，调用 regmap_read/write/bulk_read。
- **项目场景**：P5 采用 8 位寄存器、8 位值和 SPI 读标志，连续读取 14 字节原始数据。
- **代码证据**：P5 `icm20608_regmap_config`、trigger handler。证据：`CODE`、`CROSS`。
- **工程取舍**：regmap 有抽象开销但换来统一错误处理和可维护性；高频路径使用 bulk_read 减少事务。
- **常见误区**：把 regmap 说成一种总线；它是寄存器映射抽象，可后端接 SPI/I2C/MMIO。
- **30 秒回答**：我用 regmap 集中描述 8 位寄存器和值以及 SPI 读标志，直读和 buffer 都不再重复拼总线帧。
- **2 分钟展开**：说明 volatile/precious register、cache 选项在本项目为何未扩展。

### A10｜为什么选择 IIO 而不是私有 ioctl

- **一句话结论**：IMU 属于标准工业 I/O 传感器，IIO 已提供 channel、raw/scale/offset、trigger 和 buffer ABI，复用比自定义字符协议更可维护。
- **从零解释**：raw 是寄存器读数，scale/offset 把它转换为物理量；buffer 负责连续采样。
- **底层机制**：`iio_chan_spec` 描述通道，`iio_info` 处理属性，IIO core 创建 sysfs 和设备节点。
- **项目场景**：P5 暴露 accel、anglvel、temp 和 timestamp。
- **代码证据**：P5 `kernel/icm20608_iio.c` channel 表与 read_raw。证据：`CODE`、`CROSS`。
- **工程取舍**：标准 ABI 降低用户态耦合；特殊高性能协议仍可能需要其他接口，但必须有证据。
- **常见误区**：认为用了 IIO 就完全不用理解字符设备；IIO core 仍通过内核文件接口提供数据。
- **30 秒回答**：我没有重复写私有 ioctl，而是用 IIO 表达 raw/scale/采样率和 buffer，使用户态按标准 sysfs 与设备节点工作。
- **2 分钟展开**：比较 hwmon、input、IIO 和自定义 cdev 的选择依据。

### A11｜direct mode 与 buffer 的互斥

- **一句话结论**：直接读取和连续 buffer 不能无约束同时访问设备，驱动要用 IIO direct mode 保护或在运行状态拒绝冲突配置。
- **从零解释**：一次 sysfs raw read 若与定时 bulk read 同时改变/读取寄存器，可能破坏采样一致性。
- **底层机制**：claim direct mode 检查 buffer 是否活跃；运行中修改 sampling_frequency 返回 `-EBUSY`。
- **项目场景**：P5 L1 先实现 direct read，L4 加 buffer 后补互斥边界。
- **代码证据**：P5 `read_raw/write_raw`。证据：`CODE`、`CROSS`。
- **工程取舍**：拒绝运行中改频率简化并发；更复杂动态更新需停止 timer、加锁并重启。
- **常见误区**：sysfs 写成功就立即无锁改 hrtimer period。
- **30 秒回答**：buffer 开启时 direct/config 操作要受限。我选择运行中改采样率返回 EBUSY，保证 timer 周期与 scan 布局一致。
- **2 分钟展开**：说明直接模式、buffer enable 和配置事务顺序。

### A12｜IIO scan、时间戳与信息泄漏

- **一句话结论**：scan 布局由启用通道、storage bits、endianness 和对齐共同决定；padding 必须初始化，用户态按实际布局解析。
- **从零解释**：七个 16 位值共 14 字节，8 字节时间戳放在偏移 16，结构大小 24。
- **底层机制**：内核把整块 scan 推入 buffer；未初始化的 2 字节 padding 若不清零可能泄漏内核数据。
- **项目场景**：P5 available_scan_masks 只允许完整布局，scan 每次 memset 后填值和时间戳。
- **代码证据**：P5 `kernel/icm20608_iio.c`。证据：`CODE`、`CROSS`。
- **工程取舍**：固定全通道简化用户态；未来支持任意 mask 必须动态计算偏移。
- **常见误区**：把 14+8 算成 22；忽略 timestamp 对齐；用户态硬编码设备号和布局却不校验。
- **30 秒回答**：IIO 布局实际是 24 字节：14 字节通道、2 字节 padding、8 字节时间戳。我 push 前清零整个 scan，避免泄漏未初始化内核内存。
- **2 分钟展开**：手画字节偏移并关联用户态 pending parser。

### A13｜sysfs 与 `/dev/iio:deviceX`

- **一句话结论**：sysfs 适合低频配置/属性，字符设备 buffer 适合连续二进制数据；设备编号不稳定，应按 name 枚举。
- **从零解释**：sysfs 是内核对象属性的文本视图，不是普通配置数据库。
- **底层机制**：IIO core 根据注册设备和 channel 创建属性；probe 顺序决定 deviceX。
- **项目场景**：P5 `findDevice` 遍历 name，`configureBuffer` 先 disable、配置全部项、最后 enable。
- **代码证据**：P5 `libiio/src/iio_sysfs.cpp` 与 host fake-sysfs test。证据：`CODE`、`HOST`。
- **工程取舍**：配置步骤形成最小事务，失败回到 disabled；不写死 `iio:device0`。
- **常见误区**：边运行边修改 scan_elements；一次失败留下半配置 enable 状态。
- **30 秒回答**：用户态按 IIO name 找设备，不依赖编号；配置先关闭 buffer，设置频率/长度/scan elements，最后开启，异常时尝试恢复关闭。
- **2 分钟展开**：说明 sysfs 单文件写并非跨多个属性原子，所以应用自己组织事务。

## 5. 用户态算法与内核边界

### A14｜为什么算法放用户态

- **一句话结论**：内核提供稳定机制和 ABI，变化频繁、可失败的业务策略放用户态，更易测试和更新，也缩小内核故障面。
- **从零解释**：倾斜阈值、连续样本窗口和告警优先级属于产品策略，不是硬件访问必需。
- **底层机制**：驱动输出带时间戳物理量，`libcondition` 接收纯 Sample，无系统调用依赖。
- **项目场景**：P5 状态机在 x86 主机运行相同输入得到确定输出。
- **代码证据**：P5 `libcondition`、tests、replay。证据：`CODE`、`HOST`。
- **工程取舍**：多一次用户/内核数据传递，换来隔离、可测试和发布灵活性。
- **常见误区**：为了“实时”把所有计算放内核；阈值算法异常会扩大为内核崩溃。
- **30 秒回答**：驱动只做发现、采样和标准 IIO ABI，倾斜/冲击/失联在 libcondition。这样算法可在 PC 回放测试，修改策略不需要重装内核模块。
- **2 分钟展开**：说明何种极低延迟闭环才可能考虑内核/eBPF/RT 扩展。

### A15｜原题 206、213：交叉编译及两种驱动构建方式

- **一句话结论**：交叉编译是在主机生成目标架构产物；内建与模块两种方式必须使用与目标运行内核匹配的配置、源码和工具链。
- **从零解释**：x86 编译成功不代表 ARM 可运行；同为 ARM 还要匹配 ABI、glibc、内核版本和 module vermagic。
- **底层机制**：CMake toolchain 指定 ARM g++；内核外部模块使用 `make -C KERNEL_SRC M=... ARCH=arm CROSS_COMPILE=...`。
- **项目场景**：P5 ARM 用户服务由 Linaro GCC 4.9.4 构建为 EABI5；驱动针对 ALIENTEK vendor 4.1.15 编译对象。
- **代码证据**：P5 `cmake/toolchains`、`scripts/validate_driver_source.sh`。证据：`CROSS`。
- **工程取舍**：板端主体用 C++14 兼容旧工具链；不为追求 C++17 牺牲可部署性。
- **常见误区**：file 显示 ARM 就等于能在板上运行；忽略动态加载器/rootfs 版本；对象文件等于 `.ko`。
- **30 秒回答**：我用目标 Linaro 工具链得到 ARM EABI5 用户态 ELF，并用 vendor 4.1.15 头编译驱动对象；由于当前 defconfig 未启用 IIO buffer/trigger，仍不宣称 `.ko` 已联编加载。
- **2 分钟展开**：说明 sysroot、ABI、vermagic、内建与模块的差异。

### B06｜原题 214～216：U-Boot 启动与参数传递

U-Boot 完成早期硬件准备、加载内核/DTB/rootfs 并通过 bootargs、设备树等传递参数。进入内核前关闭/整理中断、看门狗、cache、MMU 的具体要求依赖架构和内核启动协议，不能背成所有平台绝对规则。

P5 提供设备树 fragment，但没有修改 U-Boot，因此这些题为 `BASE/RELATED`。

## 6. P5 上板时的真实验证链

```text
1. 应用 defconfig fragment，确认 IIO_BUFFER/TRIGGER 已启用
2. 合入 DTS，重新生成 DTB
3. 用目标内核配置编译 .ko，核对 vermagic
4. 启动后检查 dmesg 与 WHO_AM_I
5. 查找 /sys/bus/iio/devices/*/name
6. 读取 raw/scale/sampling_frequency
7. 配置 scan_elements 与 buffer
8. 运行 conditiond，验证真实 24 字节 scan
9. 倾斜/冲击/停流/拔除/SIGTERM 故障注入
10. 记录 CPU、内存、丢样和 8 小时长稳
```

只有第 1～10 步实际执行并留存版本/日志后，才能把 `TODO-HIL` 改为 `HIL`。

## 7. 本章掌握度检查

- 能否从 DTS 节点一路讲到 `icm20608_probe`？
- 能否解释 hrtimer 为什么不能直接读 SPI？
- 能否说明 IIO 相比私有 cdev 的工程收益，而不是说“框架更高级”？
- 能否手画 24 字节 scan 布局并解释 padding 清零？
- 能否准确说出当前通过的是对象交叉编译，而不是 `.ko` 上板？
