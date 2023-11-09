#include "main.h"
#include "common-modbus.h"

#define ANSWER_SIZE_LIMIT 100
#define SEND_MODBUS_FRAME(A, ...)                                                \
	do {                                                                         \
		answer_len = 0;                                                          \
		set_modbus_frame(&A, __VA_ARGS__);                                       \
		uart_write_bytes(NT60_UART_PORT, A.bytes, A.length);                     \
		strcpy(send_display, "SENDING:");                                        \
		for (size_t i = 0; i < A.length; ++i) {                                  \
			sprintf(send_tmp, " %02X", A.bytes[i]);                              \
			strcat(send_display, send_tmp);                                      \
		}                                                                        \
		strcat(send_display, "\n");                                              \
		stream_write(send_display, strlen(send_display));                        \
		uint8_t c;                                                               \
		while (true) {                                                           \
			if (uart_read_bytes(NT60_UART_PORT, &c, 1, MS_TO_TICKS(100)) == 1) { \
				if (answer_len < ANSWER_SIZE_LIMIT) {                            \
					answer[answer_len++] = c;                                    \
				}                                                                \
			} else {                                                             \
				break;                                                           \
			}                                                                    \
		}                                                                        \
		TASK_DELAY_MS(100);                                                      \
	} while (0)

static uint8_t answer[ANSWER_SIZE_LIMIT];
static size_t answer_len;
static char answer_str[1000];
static char send_display[100], send_tmp[20];

void
bmsd20_move_forward(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x50, 0x0E, 0x00, 0x01);
	SEND_MODBUS_FRAME(frame, 6, address, 0x05, 0x20, 0x00, 0x00, 0x01);
	free_modbus_frame(&frame);
}

void
bmsd20_move_backward(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x50, 0x0E, 0x00, 0x02);
	SEND_MODBUS_FRAME(frame, 6, address, 0x05, 0x20, 0x00, 0x00, 0x01);
	free_modbus_frame(&frame);
}

void
bmsd20_move_stop(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x05, 0x20, 0x01, 0x00, 0x01);
	free_modbus_frame(&frame);
}
