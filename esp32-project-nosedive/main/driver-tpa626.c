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

int
tpa626_read_two_bytes_from_register(uint8_t cmd)
{
	uint8_t data[2] = {0};
	i2c_master_write_read_device(
		TPA626_I2C_PORT,
		TPA626_I2C_ADDRESS,
		&cmd,
		sizeof(cmd),
		data,
		sizeof(data),
		MS_TO_TICKS(2000)
	);
	return ((int)data[0] << 8) + data[1];
}
