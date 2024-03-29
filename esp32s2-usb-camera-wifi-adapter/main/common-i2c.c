#include "candela.h"

uint8_t
i2c_read_one_byte_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms)
{
	uint8_t data = 0;
	i2c_master_write_read_device(
		port,
		addr,
		&reg,
		sizeof(reg),
		&data,
		sizeof(data),
		MS_TO_TICKS(timeout_ms)
	);
	return data;
}

uint16_t
i2c_read_two_bytes_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms)
{
	uint8_t data[2] = {0};
	i2c_master_write_read_device(
		port,
		addr,
		&reg,
		sizeof(reg),
		data,
		sizeof(data),
		MS_TO_TICKS(timeout_ms)
	);
	return ((uint16_t)data[0] << 8) + data[1];
}
