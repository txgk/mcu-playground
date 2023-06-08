#include "nosedive.h"
#include "driver-tpa626.h"

bool
tpa626_initialize(void)
{
	uint8_t data[] = {
		TPA626_REGISTER_CALIBRATION,
		(CALIBRATIOON_REGISTER_VALUE >> 8) & 0xFF,
		CALIBRATIOON_REGISTER_VALUE & 0xFF,
	};
	if (i2c_master_write_to_device(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(2000)) == ESP_OK) {
		return true;
	}
	return false;
}

double
tpa626_read_shunt_voltage(void)
{
	return 0.0000025 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_SHUNT_VOLTAGE, 2000);
}

double
tpa626_read_bus_voltage(void)
{
	return 0.00125 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_BUS_VOLTAGE, 2000);
}

double
tpa626_read_current(void)
{
	return CURRENT_MEASUREMENT_RESOLUTION * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_CURRENT, 2000);
}

double
tpa626_read_power(void)
{
	return CURRENT_MEASUREMENT_RESOLUTION * 25 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_POWER, 2000);
}
