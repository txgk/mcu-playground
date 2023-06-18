#include "nosedive.h"

static volatile int64_t lis3dh_measurement_time = 0;
static uint16_t lis3dh_acceleration_x           = 0;
static uint16_t lis3dh_acceleration_y           = 0;
static uint16_t lis3dh_acceleration_z           = 0;

static bool
lis3dh_initialize(void)
{
	uint8_t data[] = {
		0x20,       // CTRL_REG1 register address
		0b00100111, // Enable all axis, set hi-res mode and 10 Hz data rate
	};
	if (i2c_master_write_to_device(LIS3DH_I2C_PORT, LIS3DH_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(5000)) == ESP_OK) {
		return true;
	}
	return false;
}

static int
lis3dh_read_device_id(void)
{
	return i2c_read_one_byte_from_register(LIS3DH_I2C_PORT, LIS3DH_I2C_ADDRESS, 15, 2000);
}

static void
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

void IRAM_ATTR
lis3dh_task(void *arg)
{
	struct task_descriptor *task = arg;
	if (lis3dh_initialize() == true) {
		while (true) {
			if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
				lis3dh_measurement_time = esp_timer_get_time();
				lis3dh_read_acceleration(&lis3dh_acceleration_x, &lis3dh_acceleration_y, &lis3dh_acceleration_z);
				xSemaphoreGive(task->mutex);
			}
			TASK_DELAY_MS(task->performer_period_ms);
		}
	}
	vTaskDelete(NULL);
}

int
lis3dh_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%d,%d,%d\n",
			task->prefix,
			lis3dh_measurement_time / 1000,
			lis3dh_acceleration_x,
			lis3dh_acceleration_y,
			lis3dh_acceleration_z
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
