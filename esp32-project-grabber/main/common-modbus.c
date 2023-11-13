#include "common-modbus.h"
#include <stdarg.h>

static uint16_t
crc16_modbus(const uint8_t *data, size_t data_len)
{
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < data_len; ++i) {
		crc ^= (uint16_t)data[i];
		for (uint8_t j = 0; j < 8; ++j) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xA001;
			} else {
				crc = (crc >> 1);
			}
		}
	}
	return crc;
}

int
uart_send_modbus(uart_port_t port, uint8_t *answer, size_t answer_size, size_t bytes_count, ...)
{
#define MODBUS_FRAME_MAX_SIZE 100
	if (bytes_count > MODBUS_FRAME_MAX_SIZE) return -1;
	uint8_t buf[MODBUS_FRAME_MAX_SIZE + 3];
	struct modbus_frame frame = {.length = bytes_count + 2, .bytes = buf};

	va_list list;
	va_start(list, bytes_count);
	for (int i = 0; i < bytes_count; ++i) {
		frame.bytes[i] = (uint8_t)va_arg(list, int);
	}
	uint16_t crc = crc16_modbus(frame.bytes, bytes_count);
	frame.bytes[bytes_count] = crc & 0xFF;
	frame.bytes[bytes_count + 1] = crc >> 8;
	va_end(list);
	// int64_t tm[4] = {0};
	// tm[0] = esp_timer_get_time();
	uart_write_bytes(port, frame.bytes, frame.length);
	// tm[1] = esp_timer_get_time();
	int len = 0;
	uint8_t c;
	while (true) {
		if (uart_read_bytes(port, &c, 1, MS_TO_TICKS(20)) == 1) {
			if (len < answer_size) {
				// if (len == 0) tm[2] = esp_timer_get_time();
				answer[len++] = c;
			}
		} else {
			break;
		}
	}
	// tm[3] = esp_timer_get_time();
	// char out[300];
	// int out_len = snprintf(out, 300, "otpravka=%" PRId64 ", ojidanie=%" PRId64 ", otvechanie=%" PRId64 "\n", tm[1] - tm[0], tm[2] - tm[1], tm[3] - tm[2]);
	// stream_write(out, out_len);
	return len;
}
