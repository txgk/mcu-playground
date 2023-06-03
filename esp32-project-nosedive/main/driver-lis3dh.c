#include "nosedive.h"

static int
lis3dh_read_one_byte_from_register(uint8_t cmd)
{
	uint8_t data = 0;
	i2c_master_write_read_device(
		LIS3DH_I2C_PORT,
		LIS3DH_I2C_ADDRESS,
		&cmd,
		sizeof(cmd),
		&data,
		sizeof(data),
		2000 / portTICK_PERIOD_MS
	);
	return (int)data;
}

int
lis3dh_read_device_id(void)
{
	return lis3dh_read_one_byte_from_register(15);
}
