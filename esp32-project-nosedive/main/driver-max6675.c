#include "nosedive.h"

static spi_device_handle_t max6675;

static volatile int64_t max6675_measurement_time = 0;
static volatile int16_t max6675_temperature      = 0;

static bool
max6675_initialize(void)
{
	spi_device_interface_config_t max6675_cfg = {
		.mode = 0,
		.spics_io_num = MAX6675_CS_PIN,
		.clock_speed_hz = 2000000,
		.queue_size = 3,
	};
	if (spi_bus_add_device(MAX6675_SPI_HOST, &max6675_cfg, &max6675) == ESP_OK) {
		return true;
	}
	return false;
}

static int16_t
max6675_read_temperature(void)
{
	uint16_t data = 0;
	spi_transaction_t trans = {
		.tx_buffer = NULL,
		.rx_buffer = &data,
		.length = 16,
		.rxlength = 16,
	};
	spi_device_acquire_bus(max6675, portMAX_DELAY);
	spi_device_transmit(max6675, &trans);
	spi_device_release_bus(max6675);
	int16_t res = (int16_t) SPI_SWAP_DATA_RX(data, 16);
	if (res & (1 << 2)) {
		return 0; // Not connected.
	}
	res >>= 3; // Temperature value starts from fourth bit.
	return res / 4; // It reads temperature in Celsius multiplied by 4
}

void IRAM_ATTR
max6675_task(void *arg)
{
	struct task_descriptor *task = arg;
	if (max6675_initialize() == true) {
		while (true) {
			if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
				max6675_measurement_time = esp_timer_get_time();
				max6675_temperature = max6675_read_temperature();
				xSemaphoreGive(task->mutex);
			}
			TASK_DELAY_MS(task->performer_period_ms);
		}
	}
	vTaskDelete(NULL);
}

int
max6675_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%d\n",
			task->prefix,
			max6675_measurement_time / 1000,
			max6675_temperature
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
