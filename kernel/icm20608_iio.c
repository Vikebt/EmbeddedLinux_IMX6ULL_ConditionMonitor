// SPDX-License-Identifier: GPL-2.0
/* ICM20608 SPI IIO driver, scoped to ALIENTEK Linux 4.1.15. */

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/string.h>
#include <asm/unaligned.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

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
	struct iio_trigger *trigger;
	struct hrtimer timer;
	ktime_t period;
	u8 scan[24] __aligned(8);
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
	st->period = ktime_set(0, NSEC_PER_SEC / hz);
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
		if (iio_buffer_enabled(indio_dev))
			return -EBUSY;
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
			/* IIO temperature ABI uses milli-degrees Celsius. */
			*val = 3;
			*val2 = 59976;
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
	if (iio_buffer_enabled(indio_dev))
		return -EBUSY;

	mutex_lock(&st->lock);
	ret = icm20608_set_rate(st, val);
	mutex_unlock(&st->lock);
	return ret;
}

#define ICM20608_AXIS_CHANNEL(_type, _mod, _addr, _scan) {          \
	.type = (_type),                                              \
	.modified = 1,                                                \
	.channel2 = (_mod),                                           \
	.address = (_addr),                                           \
	.scan_index = (_scan),                                        \
	.scan_type = {                                                \
		.sign = 's', .realbits = 16, .storagebits = 16,          \
		.endianness = IIO_BE,                                    \
	},                                                             \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),                 \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),         \
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),      \
}

static const struct iio_chan_spec icm20608_channels[] = {
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_X, ICM20608_REG_ACCEL_XOUT_H, 0),
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Y, ICM20608_REG_ACCEL_XOUT_H + 2, 1),
	ICM20608_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Z, ICM20608_REG_ACCEL_XOUT_H + 4, 2),
	{
		.type = IIO_TEMP,
		.address = ICM20608_REG_TEMP_OUT_H,
		.scan_index = 3,
		.scan_type = {
			.sign = 's', .realbits = 16, .storagebits = 16,
			.endianness = IIO_BE,
		},
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_OFFSET) | BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, ICM20608_REG_GYRO_XOUT_H, 4),
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, ICM20608_REG_GYRO_XOUT_H + 2, 5),
	ICM20608_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, ICM20608_REG_GYRO_XOUT_H + 4, 6),
	IIO_CHAN_SOFT_TIMESTAMP(7),
};

static const unsigned long icm20608_scan_masks[] = {
	GENMASK(6, 0),
	0,
};

static enum hrtimer_restart icm20608_timer_callback(struct hrtimer *timer)
{
	struct icm20608_state *st = container_of(timer, struct icm20608_state,
						 timer);

	/* Atomic context: only notify IIO.  SPI runs in the threaded handler. */
	iio_trigger_poll(st->trigger);
	hrtimer_forward_now(timer, st->period);
	return HRTIMER_RESTART;
}

static int icm20608_trigger_set_state(struct iio_trigger *trigger, bool enable)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trigger);
	struct icm20608_state *st = iio_priv(indio_dev);

	if (enable)
		hrtimer_start(&st->timer, st->period, HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&st->timer);
	return 0;
}

static const struct iio_trigger_ops icm20608_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = icm20608_trigger_set_state,
};

static irqreturn_t icm20608_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *poll = private;
	struct iio_dev *indio_dev = poll->indio_dev;
	struct icm20608_state *st = iio_priv(indio_dev);
	int ret;

	(void)irq;
	mutex_lock(&st->lock);
	memset(st->scan, 0, sizeof(st->scan));
	ret = regmap_bulk_read(st->map, ICM20608_REG_ACCEL_XOUT_H,
			       st->scan, 14);
	mutex_unlock(&st->lock);
	if (!ret)
		iio_push_to_buffers_with_timestamp(indio_dev, st->scan,
					   poll->timestamp);

	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

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
	hrtimer_init(&st->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	st->timer.function = icm20608_timer_callback;
	st->map = devm_regmap_init_spi(spi, &icm20608_regmap_config);
	if (IS_ERR(st->map))
		return PTR_ERR(st->map);

	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = "icm20608";
	indio_dev->info = &icm20608_info;
	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_TRIGGERED;
	indio_dev->channels = icm20608_channels;
	indio_dev->num_channels = ARRAY_SIZE(icm20608_channels);
	indio_dev->available_scan_masks = icm20608_scan_masks;

	ret = icm20608_hw_init(st);
	if (ret) {
		dev_err(&spi->dev, "sensor initialization failed: %d\n", ret);
		return ret;
	}

	st->trigger = iio_trigger_alloc("%s-%s-hrtimer", indio_dev->name,
					dev_name(&spi->dev));
	if (!st->trigger)
		return -ENOMEM;
	st->trigger->dev.parent = &spi->dev;
	st->trigger->ops = &icm20608_trigger_ops;
	iio_trigger_set_drvdata(st->trigger, indio_dev);

	ret = iio_trigger_register(st->trigger);
	if (ret)
		goto error_free_trigger;
	indio_dev->trig = iio_trigger_get(st->trigger);

	ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
					 icm20608_trigger_handler, NULL);
	if (ret)
		goto error_unregister_trigger;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_cleanup_buffer;

	dev_info(&spi->dev, "ICM20608 registered as IIO device\n");
	return 0;

error_cleanup_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
error_unregister_trigger:
	iio_trigger_put(indio_dev->trig);
	indio_dev->trig = NULL;
	iio_trigger_unregister(st->trigger);
error_free_trigger:
	iio_trigger_free(st->trigger);
	return ret;
}

static int icm20608_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct icm20608_state *st = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	hrtimer_cancel(&st->timer);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_trigger_unregister(st->trigger);
	iio_trigger_free(st->trigger);
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

