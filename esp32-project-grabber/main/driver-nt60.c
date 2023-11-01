#include "main.h"
#include "common-modbus.h"

#define SEND_MODBUS_FRAME(A, ...) do { set_modbus_frame(&A, __VA_ARGS__); uart_write_bytes(UART0_PORT, A.bytes, A.length); TASK_DELAY_MS(5); } while (0)

void
nt60_servo_setup(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x5B, 0x00, 0x01); // Reset to defaults
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x11, 0x00, 0x00); // Enable internal pulse
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x14, 0x00, 0x00); // Enable communication control
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x15, 0x00, 0x00); // Enable two phase mode
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x16, 0x00, 0x01); // Enable closed loop
	//.......................
	//.........TODO..........
	//.......................
	free_modbus_frame(&frame);
}
