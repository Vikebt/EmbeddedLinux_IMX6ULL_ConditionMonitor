// SPDX-License-Identifier: GPL-2.0
/* ICM20608 SPI IIO driver, scoped to ALIENTEK Linux 4.1.15. */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <asm/unaligned.h>

#include <linux/iio/iio.h>

#define ICM20608_REG_SMPLRT_DIV      0x19
#define ICM20608_REG_CONFIG          0x1a
#define ICM20608_REG_GYRO_CONFIG     0x1b
#define ICM20608_REG_ACCEL_CONFIG    0x1c
#define ICM20608_REG_ACCEL_XOUT_H    0x3b
#define ICM20608_REG_TEMP_OUT_H      0x41
#define ICM20608_REG_GYRO_XOUT_H     0x43
#define ICM20608_REG_PWR_MGMT_1      0x6b
#define ICM20608_REG_WHO_AM_I        0x75

#define ICM20608_WHO_AM_I_VALUE      0xaf
#define ICM20608_RESET               0x80
#define ICM20608_CLK_PLL_XGYRO       0x01
#define ICM20608_BASE_RATE_HZ        1000
#define ICM20608_DEFAULT_RATE_HZ     100

struct icm20608_state {
	struct regmap *map;
	struct mutex lock;
	unsigned int sampling_hz;
};

static const struct regmap_config icm20608_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ICM20608_REG_WHO_AM_I,
	.read_flag_mask = 0x80,
	.cache_type = REGCACHE_NONE,
};

static int icm20608_set_rate(struct icm20608_state *st, unsigned int hz)
{
	unsigned int divider;

	if (hz < 10 || hz > 200 || ICM20608_BASE_RATE_HZ % hz)
		return -EINVAL;

	divider = ICM20608_BASE_RATE_HZ / hz - 1;
	if (regmap_write(st->map, ICM20608_REG_SMPLRT_DIV, divider))
		return -EIO;

	st->sampling_hz = hz;
	return 0;
}

static int icm20608_read_be16(struct icm20608_state *st, unsigned int reg,
			      int *value)
{
	u8 bytes[2];
	int ret;

	ret = regmap_bulk_read(st->map, reg, bytes, sizeof(bytes));
	if (ret)
		return ret;

	*value = (s16)get_unaligned_be16(bytes);
	return 0;
}

static int icm20608_read_raw(struct iio_dev *indio_dev,
			     const struct iio_chan_spec *chan,
			     int *val, int *val2, long mask)
{
	struct icm20608_state *st = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&st->lock);
		ret = icm20608_read_be16(st, chan->address, val);
		mutex_unlock(&st->lock);
		return ret ? ret : IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ACCEL:
			/* +/-2 g: 9.80665 / 16384 m/s^2 per LSB. */
			*val = 0;
			*val2 = 598;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_ANGL_VEL:
			/* +/-250 dps: pi / (180 * 131) rad/s per LSB. */
			*val = 0;
			*val2 = 133231;
			return IIO_VAL_INT_PLUS_NANO;
		case IIO_TEMP:
			*val = 0;
			*val2 = 3059;
			return IIO_VAL_INT_PLUS_MICRO;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		if (chan->type != IIO_TEMP)
			return -EINVAL;
		/* (raw + 8170) / 326.8 is approximately raw/326.8 + 25 C. */
		*val = 8170;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = st->sampling_hz;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static int icm20608_write_raw(struct iio_dev *indio_dev,
			      const struct iio_chan_spec *chan,
			      int val, int val2, long mask)
{
	struct icm20608_state *st = iio_priv(indio_dev);
	int ret;

	(void)chan;
	if (mask != IIO_CHAN_INFO_SAMP_FREQ || val2 != 0 || val <= 0)
		return -EINVAL;

	mutex_lock(&st->lock);
	ret = icm20608_set_rate(st, val);
	mutex_unlock(&st->lock);
	return ret;
}

#define ICM20608_AXIS_CHANNEL(_type, _mod, _addr) {                 \
	.type = (_type),                                              \
	.modified = 1,                                                \
	.channel2 = (_mod),                                           \
	.address = (_addr),                                           \
	.scan_index = -1,                                             \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),                 \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),         \
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),      \
}

static const struct iio_chan_spec icm20608_channels[] = {
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_X, ICM20608_REG_ACCEL_XOUT_H),
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Y, ICM20608_REG_ACCEL_XOUT_H + 2),
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Z, ICM20608_REG_ACCEL_XOUT_H + 4),
	{
		.type = IIO_TEMP,
		.address = ICM20608_REG_TEMP_OUT_H,
		.scan_index = -1,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_OFFSET) | BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, ICM20608_REG_GYRO_XOUT_H),
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, ICM20608_REG_GYRO_XOUT_H + 2),
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, ICM20608_REG_GYRO_XOUT_H + 4),
};

static const struct iio_info icm20608_info = {
	.read_raw = icm20608_read_raw,
	.write_raw = icm20608_write_raw,
	.driver_module = THIS_MODULE,
};

static int icm20608_hw_init(struct icm20608_state *st)
{
	unsigned int who_am_i;
	int ret;

	ret = regmap_read(st->map, ICM20608_REG_WHO_AM_I, &who_am_i);
	if (ret)
		return ret;
	if (who_am_i != ICM20608_WHO_AM_I_VALUE)
		return -ENODEV;

	ret = regmap_write(st->map, ICM20608_REG_PWR_MGMT_1, ICM20608_RESET);
	if (ret)
		return ret;
	msleep(100);

	ret = regmap_write(st->map, ICM20608_REG_PWR_MGMT_1,
			   ICM20608_CLK_PLL_XGYRO);
	if (ret)
		return ret;
	ret = regmap_write(st->map, ICM20608_REG_CONFIG, 0x03);
	if (ret)
		return ret;
	ret = regmap_write(st->map, ICM20608_REG_GYRO_CONFIG, 0x00);
	if (ret)
		return ret;
	ret = regmap_write(st->map, ICM20608_REG_ACCEL_CONFIG, 0x00);
	if (ret)
		return ret;

	return icm20608_set_rate(st, ICM20608_DEFAULT_RATE_HZ);
}

static int icm20608_probe(struct spi_device *spi)
{
	struct icm20608_state *st;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	st = iio_priv(indio_dev);
	mutex_init(&st->lock);
	st->map = devm_regmap_init_spi(spi, &icm20608_regmap_config);
	if (IS_ERR(st->map))
		return PTR_ERR(st->map);

	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = "icm20608";
	indio_dev->info = &icm20608_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = icm20608_channels;
	indio_dev->num_channels = ARRAY_SIZE(icm20608_channels);

	ret = icm20608_hw_init(st);
	if (ret) {
		dev_err(&spi->dev, "sensor initialization failed: %d\n", ret);
		return ret;
	}

	ret = iio_device_register(indio_dev);
	if (ret)
		return ret;

	dev_info(&spi->dev, "ICM20608 registered as IIO device\n");
	return 0;
}

static int icm20608_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	iio_device_unregister(indio_dev);
	return 0;
}

static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "invensense,icm20608" },
	{ }
};
MODULE_DEVICE_TABLE(of, icm20608_of_match);

static const struct spi_device_id icm20608_ids[] = {
	{ "icm20608", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, icm20608_ids);

static struct spi_driver icm20608_driver = {
	.driver = {
		.name = "icm20608-iio",
		.of_match_table = icm20608_of_match,
	},
	.probe = icm20608_probe,
	.remove = icm20608_remove,
	.id_table = icm20608_ids,
};
module_spi_driver(icm20608_driver);

MODULE_AUTHOR("Embedded Linux study project");
MODULE_DESCRIPTION("ICM20608 SPI IIO driver for i.MX6ULL Linux 4.1.15");
MODULE_LICENSE("GPL v2");

