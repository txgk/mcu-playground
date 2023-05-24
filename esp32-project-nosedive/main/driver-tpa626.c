#include "nosedive.h"

static int
tpa626_read_two_bytes_from_register(uint8_t cmd)
{
	uint8_t data[2] = {0};
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		i2c_master_write_read_device(
			I2C_NUM_0,
			TPA626_I2C_ADDRESS,
			&cmd,
			sizeof(cmd),
			data,
			sizeof(data),
			2000 / portTICK_PERIOD_MS
		);
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	return ((int)data[0] << 8) + data[1];
}

int
tpa626_read_configuration(void)
{
	return tpa626_read_two_bytes_from_register(0);
}

int
tpa626_read_shunt_voltage(void)
{
	return tpa626_read_two_bytes_from_register(1);
}

int
tpa626_read_bus_voltage(void)
{
	return tpa626_read_two_bytes_from_register(2);
}
