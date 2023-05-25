#include "nosedive.h"

static int
lis3dh_read_one_byte_from_register(uint8_t cmd)
{
	uint8_t data = 0;
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		i2c_master_write_read_device(
			I2C_NUM_0,
			LIS3DH_I2C_ADDRESS,
			&cmd,
			sizeof(cmd),
			&data,
			sizeof(data),
			2000 / portTICK_PERIOD_MS
		);
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	return (int)data;
}

int
lis3dh_read_device_id(void)
{
	return lis3dh_read_one_byte_from_register(15);
}
