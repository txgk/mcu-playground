#include "main.h"

struct modbus_frame {
	uint8_t *bytes;
	size_t length;
};

void set_modbus_frame(struct modbus_frame *frame, int count, ...);
void free_modbus_frame(struct modbus_frame *frame);
