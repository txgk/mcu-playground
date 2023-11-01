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

void
set_modbus_frame(struct modbus_frame *frame, int count, ...)
{
	free_modbus_frame(frame);
	frame->length = count + 2;
	frame->bytes = malloc(sizeof(uint8_t) * frame->length);
	if (frame->bytes == NULL) {
		frame->length = 0;
		return;
	}

	va_list list;
	va_start(list, count);
	for (int i = 0; i < count; ++i) {
		frame->bytes[i] = (uint8_t)va_arg(list, int);
	}
	uint16_t crc = crc16_modbus(frame->bytes, count);
	frame->bytes[count] = crc & 0xFF;
	frame->bytes[count + 1] = crc >> 8;
	va_end(list);
}

void
free_modbus_frame(struct modbus_frame *frame)
{
	if (frame != NULL) {
		if (frame->bytes != NULL) {
			free(frame->bytes);
			frame->bytes = NULL;
		}
		frame->length = 0;
	}
}
