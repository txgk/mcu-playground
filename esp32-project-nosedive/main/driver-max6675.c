#include "nosedive.h"

static spi_device_handle_t max6675;

bool
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

int16_t
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
