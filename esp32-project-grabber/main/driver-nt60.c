#include "main.h"
#include "common-modbus.h"

#define ANSWER_SIZE_LIMIT 100
#define SEND_MODBUS_FRAME(A, ...)                                                    \
	do {                                                                             \
		answer_len = 0;                                                              \
		if (xSemaphoreTake(nt60_driver_lock, portMAX_DELAY) == pdTRUE) {             \
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
			xSemaphoreGive(nt60_driver_lock);                                        \
			TASK_DELAY_MS(500); /* This delay matters! NT60 consumes cmds slowly! */ \
		}                                                                            \
	} while (0)

static SemaphoreHandle_t nt60_driver_lock = NULL;
static uint8_t answer[ANSWER_SIZE_LIMIT];
static size_t answer_len;
static char answer_str[1000];
static char send_display[100], send_tmp[20];

static inline void
nt60_create_answer_string(void)
{
	if (answer_len > 0) {
		strcpy(answer_str, "Driver answered:");
		for (size_t i = 0; i < answer_len; ++i) {
			sprintf(send_tmp, " %02X", answer[i]);
			strcat(answer_str, send_tmp);
		}
	} else {
		strcpy(answer_str, "Driver didn't answer!");
	}
}

bool
nt60_driver_setup(void)
{
	nt60_driver_lock = xSemaphoreCreateMutex();
	return nt60_driver_lock == NULL ? false : true;
}

void
nt60_servo_setup(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const uint8_t address = strtol(value, NULL, 10);
	long default_position = 0;
	long default_speed = 600;
	switch (address) {
		case 1: default_position = 0; default_speed = 600; break;
		case 2: default_position = 0; default_speed = 600; break;
		case 3: default_position = 0; default_speed = 600; break;
	}
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x5B, 0x00, 0x01); // Reset to defaults
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x11, 0x00, 0x00); // Enable internal pulse
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x14, 0x00, 0x00); // Enable communication control
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x15, 0x00, 0x00); // Enable two phase mode
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x16, 0x00, 0x01); // Enable closed loop
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x17, 0x00, 0x00); // Set normal direction
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3C, 0x00, 0x03); // Enable quadrature encoder (B)
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3D, 0x00, 0x02); // Enable quadrature encoder (A)
	// Enable inputs and outputs
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3C, 0x00, 0x23);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3D, 0x00, 0x22);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3E, 0x00, 0x27);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3F, 0x00, 0x28);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x40, 0x00, 0x2C);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x41, 0x00, 0x2B);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x42, 0x00, 0x11);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x43, 0x00, 0x12);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x66, 0x00, 0x15);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x67, 0x00, 0x14);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x54, 0x00, 0x01); // Set movement mode to absolute
	// Set position
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x49, ((default_position >>  8) & 0xFF), ((default_position >>  0) & 0xFF));
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x4A, 0x00,                              ((default_position >> 16) & 0xFF));
	// Set speed
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x48, ((default_speed    >>  8) & 0xFF), ((default_speed    >>  0) & 0xFF));
	// TODO TODO TODO
	// Determine extreme positions by scanning current on different rotate positions.
	// TODO TODO TODO
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "Servo setup command sent.\n");
	free_modbus_frame(&frame);
}

void
nt60_rotate_absolute(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) {
		return;
	}
	const uint8_t address = strtol(value, NULL, 10);
	const long position = strtol(comma_pos + 1, NULL, 10);
	struct modbus_frame frame = {0};
	// Set position
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x49, ((position >>  8) & 0xFF), ((position >>  0) & 0xFF));
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x4A, 0x00,                      ((position >> 16) & 0xFF));
	// Commit rotation
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x01);
	free_modbus_frame(&frame);
}

void
nt60_get_speed(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, 0x00, 0x48, 0x00, 0x01);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_speed = ((int)(answer[11]) << 8) | answer[12];
		*answer_len_ptr = snprintf(
			answer_buf_ptr,
			HTTP_TUNER_ANSWER_SIZE_LIMIT,
			"%s (speed is %d)\n",
			answer_str,
			read_speed
		);
	} else {
		*answer_len_ptr = snprintf(
			answer_buf_ptr,
			HTTP_TUNER_ANSWER_SIZE_LIMIT,
			"%s (can't read speed)\n",
			answer_str
		);
	}
	free_modbus_frame(&frame);
}
