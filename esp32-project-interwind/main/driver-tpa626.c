#include "interwind.h"
#include "driver-tpa626.h"

static volatile int64_t tpa626_measurement_time = 0;
static volatile double tpa626_shunt_voltage     = 0;
static volatile double tpa626_bus_voltage       = 0;
static volatile double tpa626_current           = 0;
static volatile double tpa626_power             = 0;

static inline bool
tpa626_initialize(void)
{
	uint8_t data[] = {
		TPA626_REGISTER_CALIBRATION,
		(CALIBRATIOON_REGISTER_VALUE >> 8) & 0xFF,
		CALIBRATIOON_REGISTER_VALUE & 0xFF,
	};
	if (i2c_master_write_to_device(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(5000)) == ESP_OK) {
		return true;
	}
	return false;
}

static inline double
tpa626_read_shunt_voltage(void)
{
	return 0.0000025 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_SHUNT_VOLTAGE, 2000);
}

static inline double
tpa626_read_bus_voltage(void)
{
	return 0.00125 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_BUS_VOLTAGE, 2000);
}

static inline double
tpa626_read_current(void)
{
	return CURRENT_MEASUREMENT_RESOLUTION * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_CURRENT, 2000);
}

static inline double
tpa626_read_power(void)
{
	return CURRENT_MEASUREMENT_RESOLUTION * 25 * i2c_read_two_bytes_from_register(TPA626_I2C_PORT, TPA626_I2C_ADDRESS, TPA626_REGISTER_POWER, 2000);
}

void IRAM_ATTR
tpa626_task(void *arg)
{
	struct task_descriptor *task = arg;
	if (tpa626_initialize() == true) {
		while (true) {
			if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
				tpa626_measurement_time = esp_timer_get_time();
				tpa626_shunt_voltage = tpa626_read_shunt_voltage();
				tpa626_bus_voltage = tpa626_read_bus_voltage();
				tpa626_current = tpa626_read_current();
				tpa626_power = tpa626_read_power();
				xSemaphoreGive(task->mutex);
			}
			TASK_DELAY_MS(task->performer_period_ms);
		}
	}
	vTaskDelete(NULL);
}

int
tpa626_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%.2lf,%.2lf,%.4lf,%.2lf\n",
			task->prefix,
			tpa626_measurement_time / 1000,
			tpa626_shunt_voltage,
			tpa626_bus_voltage,
			tpa626_current,
			tpa626_power
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
