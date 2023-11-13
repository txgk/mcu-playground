#include "main.h"

struct modbus_frame {
	uint8_t *bytes;
	size_t length;
};

int uart_send_modbus(uart_port_t port, uint8_t *answer, size_t answer_size, size_t bytes_count, ...);
