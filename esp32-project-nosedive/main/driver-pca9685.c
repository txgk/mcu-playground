#include "nosedive.h"

static int
pca9685_read_one_byte_from_register(uint8_t cmd)
{
	uint8_t data = 0;
	i2c_master_write_read_device(
		I2C_NUM_0,
		PCA9685_I2C_ADDRESS,
		&cmd,
		sizeof(cmd),
		&data,
		sizeof(data),
		2000 / portTICK_PERIOD_MS
	);
	return (int)data;
}

int
pca9685_read_subadr1(void)
{
	return pca9685_read_one_byte_from_register(2);
}

int
pca9685_read_subadr2(void)
{
	return pca9685_read_one_byte_from_register(3);
}

int
pca9685_read_subadr3(void)
{
	return pca9685_read_one_byte_from_register(4);
}
