# 术语表：用准确语言回答

## 1. C/C++ 与运行时

| 中文 | English / 缩写 | 面试中的准确含义 |
| --- | --- | --- |
| 声明 | declaration | 告诉编译器名字及类型，不一定分配实体存储 |
| 定义 | definition | 提供实体或函数实现；同一程序通常受 ODR 约束 |
| 链接属性 | linkage | 名称在翻译单元之间是否指向同一实体 |
| 名字改编 | name mangling | C++ 编译器把重载等类型信息编码进符号名 |
| 存储期 | storage duration | 对象存储存在多久：自动、静态、线程、动态 |
| 生命周期 | lifetime | 对象从开始存在到结束存在的语义区间，不完全等同存储期 |
| 未定义行为 | undefined behavior, UB | 标准不规定结果；不能依赖某次“运行正常” |
| 对齐 | alignment | 对象地址需满足类型/平台的倍数约束 |
| 浅拷贝 | shallow copy | 只复制成员值，指针成员仍指向同一资源 |
| 深拷贝 | deep copy | 为所拥有资源创建独立副本 |
| 资源获取即初始化 | RAII | 用对象生命周期管理内存、fd、锁、线程等资源 |
| 右值引用 | rvalue reference | `T&&`；支持移动语义和完美转发，但本身不是“必然移动” |
| 移动语义 | move semantics | 转移资源所有权，减少昂贵复制；源对象仍须可析构 |
| 完美转发 | perfect forwarding | 保留实参值类别转交给目标函数，常用 `std::forward` |
| 原子操作 | atomic operation | 对其他线程表现为不可分割，并受指定内存序约束 |
| 先行发生 | happens-before | C++ 内存模型中保证一个操作效果对另一个可见的偏序关系 |
| 数据竞争 | data race | 不同步的并发冲突访问且至少一次写；在 C++ 中属于 UB |
| 虚函数表 | vtable | 常见 ABI 实现机制，不是 C++ 标准强制的具体布局 |

## 2. ARM、实时与 FreeRTOS

| 中文 | English / 缩写 | 面试中的准确含义 |
| --- | --- | --- |
| 中断服务程序 | interrupt service routine, ISR | 中断入口执行的受限代码，应短小并遵守上下文 API 规则 |
| 中断向量表 | interrupt vector table | 异常/中断号到入口地址的映射结构 |
| 嵌套向量中断控制器 | NVIC | Cortex-M 中断使能、优先级、挂起和嵌套控制单元 |
| 临界区 | critical section | 访问共享不变量时不可被指定并发源打断的代码区 |
| 任务 | task | FreeRTOS 的可调度执行单元，概念上类似线程但环境不同 |
| 抢占 | preemption | 调度器暂停当前执行单元，运行更高优先级/更合适的单元 |
| 时间片 | time slicing | 同优先级就绪任务按 Tick 等规则轮换 |
| 优先级反转 | priority inversion | 高优先级等待低优先级持有资源，且低优先级受中优先级干扰 |
| 优先级继承 | priority inheritance | 临时提高锁持有者优先级以缩短反转阻塞，不能解决所有死锁 |
| 信号量 | semaphore | 表示事件或可用数量的同步原语，不一定有持有者所有权 |
| 互斥量 | mutex | 有所有权的互斥原语；通常用于保护共享不变量 |
| 队列 | queue | 在任务/中断之间按既定规则复制和排队消息 |
| 最新值邮箱 | latest-value mailbox | 只关心最新状态的长度 1 覆盖式队列模式 |
| 最坏执行时间 | WCET | 在定义硬件和输入约束下任务执行时间的上界估计 |
| 截止期 | deadline | 工作必须完成的时间约束；实时性关注是否可预测地满足 |
| 看门狗 | watchdog | 在软件失去健康进展时触发复位/告警的独立监控机制 |

## 3. Linux 进程、线程与内存

| 中文 | English / 缩写 | 面试中的准确含义 |
| --- | --- | --- |
| 进程 | process | 资源隔离与地址空间容器，也是 Linux 调度实体组织的一部分 |
| 线程 | thread | 与同进程线程共享地址空间/多数资源，拥有独立栈与调度状态 |
| 系统调用 | system call | 用户程序请求内核服务的受控入口，不等于普通函数调用 |
| 上下文切换 | context switch | 保存一个执行上下文并恢复另一个；系统调用不一定引发进程切换 |
| 虚拟内存 | virtual memory | 地址转换、隔离、映射、共享和按需调页的整体机制 |
| 页表 | page table | 虚拟页到物理页及权限等属性的映射结构 |
| 缺页 | page fault | 地址访问无法由当前页表直接完成而触发的异常，可正常也可致命 |
| 写时复制 | copy-on-write, COW | 初始共享只读页，写入时再复制，用于 fork 等场景 |
| 内存映射 | mmap | 把文件或匿名内存映射到虚拟地址空间 |
| 互斥锁 | mutex | 用户态线程保护复合共享状态的常用同步原语 |
| 条件变量 | condition variable | 等待“受 mutex 保护的谓词发生变化”的协调机制 |
| 虚假唤醒 | spurious wakeup | 条件变量等待可能在谓词未满足时返回，因此必须循环检查 |
| 死锁 | deadlock | 一组执行单元循环等待彼此持有且不可获得的资源 |
| 活锁 | livelock | 执行单元持续变化和响应但没有实质进展 |
| 饥饿 | starvation | 某执行单元长期得不到运行或资源机会 |
| 文件描述符 | file descriptor, fd | 进程 fd 表中的小整数索引，不是磁盘文件本身 |
| inode | index node | Linux 文件系统中保存对象元数据并关联数据的核心结构 |
| 资源限制 | rlimit | 进程可用 fd、栈等资源的软/硬限制 |

## 4. I/O 与网络

| 中文 | English / 缩写 | 面试中的准确含义 |
| --- | --- | --- |
| 阻塞 I/O | blocking I/O | 操作暂不能完成时调用线程可睡眠等待 |
| 非阻塞 I/O | non-blocking I/O | 操作暂不能完成时立即返回，如 `EAGAIN` |
| I/O 多路复用 | I/O multiplexing | 一个线程等待多个 fd 的就绪事件，如 select/poll/epoll |
| 水平触发 | level-triggered, LT | 条件持续满足时可反复报告就绪 |
| 边沿触发 | edge-triggered, ET | 主要在就绪状态变化时报告，通常需读写至 `EAGAIN` |
| 反压 | backpressure | 下游容量不足时向上游传播限速、阻塞、拒绝或丢弃策略 |
| 高水位 | high-water mark | 队列/缓冲达到阈值后启动反压或降级的界线 |
| 字节序 | endianness | 多字节值在内存/协议中字节排列顺序；网络字节序通常为大端 |
| 套接字 | socket | 由 fd 引用的通信端点，具有协议、地址和状态 |
| 监听队列 | listen backlog | TCP 监听端对待处理连接的内核队列相关上限，不是业务消息队列 |
| 半连接 | half-open connection | 语境可能指握手未完成或一端失效未被另一端察觉，回答时需澄清 |
| MTU | maximum transmission unit | 链路层一次可承载的最大网络层包大小 |
| 粘包 | stream coalescing | 俗称 TCP 接收时多个应用消息连在字节流中，根因是无消息边界 |
| 半包 | partial frame | 一次读取只得到应用帧的一部分，需累积后解析 |
| 重传超时 | retransmission timeout, RTO | TCP 根据往返时间估计用于触发重传的计时参数 |
| 指数退避 | exponential backoff | 失败后按倍数增长重试间隔，并设置上限；常配合 jitter |
| 抖动 | jitter | 时间间隔变化；退避中的随机扰动可避免多个节点同步重试 |
| PCAP | packet capture | 保存抓包数据及时间戳等元信息的常见文件格式 |

## 5. Linux 驱动、设备树与 IIO

| 中文 | English / 缩写 | 面试中的准确含义 |
| --- | --- | --- |
| 内核模块 | loadable kernel module, LKM | 可动态装载/卸载的内核目标；运行在内核地址空间 |
| 设备树 | device tree, DT | 描述不可枚举硬件实例、连接和资源的层次化数据 |
| 设备树源文件 | DTS | 人类可读的设备树源文本 |
| 设备树二进制 | DTB | 由 DTC 编译、由引导程序传给内核的二进制设备树 |
| compatible | compatible property | 表示设备兼容型号，参与总线设备与驱动匹配 |
| 探测 | probe | 匹配后驱动初始化一个设备实例的回调过程 |
| 移除 | remove | 设备解绑/模块退出时撤销注册与资源的回调过程 |
| 延迟探测 | deferred probe | 依赖资源暂不可用时稍后重试 probe 的机制 |
| 设备管理资源 | devres / devm | 与 device 生命周期绑定的自动释放资源机制 |
| 寄存器映射 | regmap | 统一寄存器访问、缓存、锁与调试的内核抽象 |
| 工业 I/O | Industrial I/O, IIO | 面向 ADC、IMU 等传感器/转换器的 Linux 子系统 |
| 通道 | IIO channel | 某种测量量及其格式、索引、scale 等描述 |
| 扫描元素 | scan element | IIO 缓冲中启用通道的数据布局描述 |
| 触发器 | IIO trigger | 发起一轮缓冲采样的事件源 |
| 线程化轮询 | threaded poll function | IIO 中把可能睡眠的采样工作放在线程上下文的处理方式 |
| 高精度定时器 | hrtimer | 内核高分辨率计时机制；回调上下文不可直接做可睡眠 I/O |
| 工作队列 | workqueue | 把工作安排到内核工作线程执行的延后机制 |
| 上半部/下半部 | top half / bottom half | 将紧急中断响应与可延后工作分开的传统描述 |
| 原子上下文 | atomic context | 不允许睡眠的执行上下文，不等价于“正在做 atomic 变量操作” |
| 进程上下文 | process context | 代表某任务执行、通常允许睡眠的内核上下文，仍受锁等约束 |
| sysfs | sysfs | 内核对象和设备属性的文本化视图，适合少量控制/状态 |
| ioctl | input/output control | 对 fd 发出设备/协议特定控制命令的系统调用接口 |
| 用户拷贝 | copy_to/from_user | 内核安全访问用户地址的接口，返回未复制字节数等语义需核对 |
| 主设备号/次设备号 | major/minor number | 字符/块设备号中分别标识驱动类别与具体实例的传统编号 |
| U-Boot | Universal Boot Loader | 常见嵌入式引导程序，负责早期硬件、镜像加载和启动参数 |
| 交叉编译 | cross compilation | 在宿主平台生成另一目标架构/ABI 的产物 |
| sysroot | system root | 交叉工具链编译链接时使用的目标头文件和库根目录 |

## 6. 项目和证据术语

| 标识 | 全称/含义 | 使用规则 |
| --- | --- | --- |
| P1 | Handheld Temperature | 手持体温检测仪项目 |
| P2 | Smart Medicine Cart | 智能送药小车项目 |
| P3 | 3D LiDAR Perception | 三维 LiDAR 感知项目 |
| P4 | MultiSource Optical Fusion | 多源光电数据融合项目 |
| P5 | I.MX6ULL Condition Monitor | 状态监测器与 IIO 驱动项目 |
| CODE | code evidence | 仓库有实现；不自动等于运行通过 |
| HOST | host-verified | 宿主测试或实验通过；不证明目标硬件 |
| CROSS | cross-build verified | 目标工具链生成产物；不证明板端运行 |
| HIL | hardware-in-the-loop | 在记录清楚的真实硬件环境中通过 |
| TODO-HIL | hardware validation pending | 明确待验证，面试和简历不得省略 TODO |
| RELATED | related concept | 与项目可迁移关联，但当前实现没有直接证据 |
| BASE | foundational knowledge | 基础知识，正确回答即可，不强行项目化 |

## 7. 容易说错的完整句式

| 不准确表达 | 推荐表达 |
| --- | --- |
| “volatile 防止数据竞争” | “volatile 约束访问优化；线程同步使用 atomic、mutex 或队列。” |
| “中断里不能睡眠因为优先级高” | “ISR/原子上下文没有可调度睡眠条件，且会影响中断延迟。” |
| “epoll 是异步 I/O” | “epoll 是就绪通知；实际 read/write 仍由应用执行。” |
| “一次 read 读一包” | “read 返回当前可得字节数，应用协议必须自己维护帧边界。” |
| “设备树调用 probe” | “内核根据 DT 创建设备，总线匹配 compatible 后调用驱动 probe。” |
| “hrtimer 精度高所以里面采 SPI” | “hrtimer 负责触发；SPI 可能睡眠，放在线程化处理。” |
| “编译过就是驱动验证过” | “已完成指定内核/工具链的 CROSS；加载、波形和数据仍待 HIL。” |
| “项目用了零拷贝” | “除非能指出具体机制和测量，否则只说减少了哪些复制。” |
| “无锁一定更快” | “无锁需要正确内存序和竞争模型，是否更快必须测量。” |
| “Linux 不实时” | “通用 Linux 默认不提供严格硬实时保证；配置、PREEMPT_RT 与负载会改变延迟特征。” |

准确表达的目标不是堆术语，而是让面试官能分辨：你知道机制、看过代码、做过验证，也知道结论在哪一步停止。
