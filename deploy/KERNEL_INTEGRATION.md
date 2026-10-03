# vendor 4.1.15 内核集成与验证

以下操作必须在**干净的内核副本**中进行；不要对已有改动的 vendor 工作树运行 `mrproper`、`reset` 或覆盖配置。模块必须与板上运行的同一内核版本、配置和工具链构建。

1. 把本仓库 `kernel/Kconfig` 复制到内核副本的 `drivers/iio/imu/condition_monitor/Kconfig`，在内核副本根目录应用 `deploy/vendor-iio-kconfig.patch`。补丁只接入 Kconfig；驱动源码仍从本仓库以外部模块方式构建。
2. 从板端对应的 defconfig 开始，合并 `deploy/imx6ull_iio_defconfig.fragment`，运行 `make ARCH=arm olddefconfig`。确认最终 `.config` 有 `CONFIG_ICM20608_IIO=m` 和 `CONFIG_IIO_TRIGGERED_BUFFER=m`。后者是隐藏选项：不能仅手写 fragment，必须由驱动 Kconfig 的 `select` 产生。
3. 使用目标交叉工具链构建内核和 `modules`，确保 `Module.symvers` 存在。以构建输出目录设置 `KERNEL_BUILD`，运行 `scripts/check_kernel_module_config.sh`；预检查必须通过。
4. 执行 `make -C "$KERNEL_SRC" O="$KERNEL_BUILD" M="$PWD/kernel" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules`。检查构建输出没有任何未解析符号警告；仅看到 `.ko` 文件或 `make` 返回 0 不够。
5. 合并并编译设备树节点，确保与真实板级 SPI 片选、电压和引脚一致。板端执行 `modinfo`、`insmod`、`dmesg`、IIO raw/scale 和 buffer 采样；保存内核 commit、配置、DTB、rootfs、工具链版本与日志。未完成这些步骤时，HIL 仍标为未验证。

`kernel/Makefile` 的 `obj-m` 路径只负责外部模块编译；它不能让内核 Kconfig 看见 `kernel/Kconfig`。本补丁解决的是这条配置依赖，不会修改或自动部署用户的原始内核树。
