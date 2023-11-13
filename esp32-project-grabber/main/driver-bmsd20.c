#include "main.h"
#include "common-modbus.h"

#define ANSWER_SIZE_LIMIT 100
#define SEND_MODBUS_FRAME(...)                                                                 \
	do {                                                                                       \
		answer_len = 0;                                                                        \
		answer_len = uart_send_modbus(NT60_UART_PORT, answer, ANSWER_SIZE_LIMIT, __VA_ARGS__); \
		if (answer_len < 0) answer_len = 0;                                                    \
		TASK_DELAY_MS(500); /* This delay matters! NT60 consumes cmds slowly! */               \
	} while (0)

static uint8_t answer[ANSWER_SIZE_LIMIT];
static int answer_len;
static char answer_str[1000];

void
bmsd20_move_forward(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x50, 0x0E, 0x00, 0x01);
	SEND_MODBUS_FRAME(6, address, 0x05, 0x20, 0x00, 0x00, 0x01);
}

void
bmsd20_move_backward(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x50, 0x0E, 0x00, 0x02);
	SEND_MODBUS_FRAME(6, address, 0x05, 0x20, 0x00, 0x00, 0x01);
}

void
bmsd20_move_stop(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x05, 0x20, 0x01, 0x00, 0x01);
}
