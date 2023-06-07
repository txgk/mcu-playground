#include "nosedive.h"

void
lis3dh_initialize(void)
{
	uint8_t data[] = {
		0x20,       // CTRL_REG1 register address
		0b00100111, // Enable all axis, set hi-res mode and 10 Hz data rate
	};
	i2c_master_write_to_device(LIS3DH_I2C_PORT, LIS3DH_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(2000));
}

int
lis3dh_read_device_id(void)
{
	return i2c_read_one_byte_from_register(LIS3DH_I2C_PORT, LIS3DH_I2C_ADDRESS, 15, 2000);
}

void
lis3dh_read_acceleration(uint16_t *x, uint16_t *y, uint16_t *z)
{
	// MSB must be set to 1 to be able to read multiple bytes.
	// This makes reading work in "auto increment" mode.
	uint8_t first_accel_register_addr = 0x28 | 0b10000000;
	uint8_t data[6] = {0};
	i2c_master_write_read_device(
		LIS3DH_I2C_PORT,
		LIS3DH_I2C_ADDRESS,
		&first_accel_register_addr,
		sizeof(first_accel_register_addr),
		data,
		sizeof(data),
		MS_TO_TICKS(2000)
	);
	*x = ((uint16_t)data[1] << 8) + data[0];
	*y = ((uint16_t)data[3] << 8) + data[2];
	*z = ((uint16_t)data[5] << 8) + data[4];
}
