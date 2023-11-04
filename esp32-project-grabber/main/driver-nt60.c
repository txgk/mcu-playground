#include "main.h"
#include "common-modbus.h"

#define DEFAULT_ABS_POS_SHIFT_PER_PULSE 65536

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
			TASK_DELAY_MS(400); /* This delay matters! NT60 consumes cmds slowly! */ \
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

static inline long
nt60_get_current_absolute_position(uint8_t address)
{
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, 0x00, 0x08, 0x00, 0x02);
	if (answer_len < 15) return 0;
	const long read_value = (((long)answer[11]) << 24) | (((long)answer[12]) << 16) | (((long)answer[13]) << 8) | answer[14];
	free_modbus_frame(&frame);
	return read_value;
}

static inline void
nt60_set_current_absolute_position(uint8_t address, long pos)
{
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 11, address, 0x10, 0x00, 0x49, 0x00, 0x02, 0x04, ((pos >> 8) & 0xFF), ((pos >> 0) & 0xFF), ((pos >> 24) & 0xFF), ((pos >> 16) & 0xFF));
	free_modbus_frame(&frame);
}

void
nt60_servo_setup(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	const long speed = 50;
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x5B, 0x00, 0x01); // Reset to defaults
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x11, 0x00, 0x00); // Enable internal pulse
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x14, 0x00, 0x00); // Enable communication control
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x15, 0x00, 0x00); // Enable two phase mode
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x16, 0x00, 0x01); // Enable closed loop
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x17, 0x00, 0x00); // Set normal direction
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3C, 0x00, 0x03); // Enable quadrature encoder (B)
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x3D, 0x00, 0x02); // Enable quadrature encoder (A)
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x54, 0x00, 0x01); // Set movement mode to absolute
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x18, 0x0F, 0xA0); // Set motor segmentation to 4000
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x46, 0x00, 0x01); // Set acceleration to 1
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x47, 0x00, 0x01); // Set deceleration to 1
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
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x48, (speed >> 8) & 0xFF, speed & 0xFF); // Set speed
	free_modbus_frame(&frame);
	http_tuner_response("Servo setup completed.\n");
}

void
nt60_seek_extremes(const char *value)
{
#define NT60_SEEK_DELTA 8

	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};

	// Save previous current value
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, 0x00, 0x2D, 0x00, 0x01);
	if (answer_len < 13) { free_modbus_frame(&frame); return; }
	long old_current = ((long)(answer[11]) << 8) | answer[12];

	// Save previous speed value
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, 0x00, 0x48, 0x00, 0x01);
	if (answer_len < 13) { free_modbus_frame(&frame); return; }
	long old_speed = ((long)(answer[11]) << 8) | answer[12];

	// Set current to 50 and make sure it is set correctly
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x2D, 0x00, 0x32);
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, 0x00, 0x2D, 0x00, 0x01);
	if (answer_len < 13) { free_modbus_frame(&frame); return; }
	const long current = ((int)(answer[11]) << 8) | answer[12];
	if (current != 50) { free_modbus_frame(&frame); return; }

	// Set speed to 16
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x48, 0x00, 0x10);

	// Do the seeking for extremes
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x54, 0x00, 0x01); // Set movement mode to absolute
	long right_extreme, left_extreme;
	long center = nt60_get_current_absolute_position(address);
	long new_pos = center + 10000 * DEFAULT_ABS_POS_SHIFT_PER_PULSE;
	nt60_set_current_absolute_position(address, new_pos / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation
	while (true) {
		long pos1 = nt60_get_current_absolute_position(address);
		// They have delay between them anyway, so don't add more waiting.
		long pos2 = nt60_get_current_absolute_position(address);
		if ((pos1 == pos2) || (labs(pos1 - pos2) <= DEFAULT_ABS_POS_SHIFT_PER_PULSE * NT60_SEEK_DELTA) || (pos2 >= new_pos)) {
			SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x06); // Stop rotation
			break;
		}
	}
	new_pos = center - 10000 * DEFAULT_ABS_POS_SHIFT_PER_PULSE;
	nt60_set_current_absolute_position(address, new_pos / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation
	while (true) {
		long pos1 = nt60_get_current_absolute_position(address);
		// They have delay between them anyway, so don't add more waiting.
		long pos2 = nt60_get_current_absolute_position(address);
		if ((pos1 == pos2) || (labs(pos1 - pos2) <= DEFAULT_ABS_POS_SHIFT_PER_PULSE * NT60_SEEK_DELTA) || (pos2 <= new_pos)) {
			SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x06); // Stop rotation
			break;
		}
	}

	// Return to the initial position
	nt60_set_current_absolute_position(address, center / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation

	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x2D, ((old_current >> 8) & 0xFF), ((old_current >> 0) & 0xFF));
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x48, ((old_speed   >> 8) & 0xFF), ((old_speed   >> 0) & 0xFF));

	free_modbus_frame(&frame);
	http_tuner_response("Extremes seeking completed.\n");
}

void
nt60_servo_stop(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, 0x06);
	nt60_create_answer_string();
	if (answer_len > 12) {
		http_tuner_response("%s\n", answer_str);
	} else {
		http_tuner_response("Servo %d doesn't respond to stop command!\n", (int)address);
	}
	free_modbus_frame(&frame);
}

static void
nt60_rotate_absolute_or_relative(const char *value, bool is_absolute)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	uint8_t address = strtol(value, NULL, 10);
	long position = strtol(comma_pos + 1, NULL, 10);
	uint8_t rotation_cmd = 0x01;
	struct modbus_frame frame = {0};
	// Set movement mode
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x54, 0x00, is_absolute ? 0x01 : 0x00);
	// Set position
	SEND_MODBUS_FRAME(frame, 11, address, 0x10, 0x00, 0x49, 0x00, 0x02, 0x04, ((position >> 8) & 0xFF), ((position >> 0) & 0xFF), ((position >> 24) & 0xFF), ((position >> 16) & 0xFF));
	// Commit rotation
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, 0x00, 0x12, 0x00, rotation_cmd);
	nt60_create_answer_string();
	if (answer_len > 0) http_tuner_response("%s\n", answer_str);
	free_modbus_frame(&frame);
}


void
nt60_rotate_absolute(const char *value)
{
	nt60_rotate_absolute_or_relative(value, true);
}

void
nt60_rotate_relative(const char *value)
{
	nt60_rotate_absolute_or_relative(value, false);
}

void
nt60_read_short_register(const char *value)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos + 1, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, ((reg >> 8) & 0xFF), ((reg >> 0) & 0xFF), 0x00, 0x01);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_value = ((int)(answer[11]) << 8) | answer[12];
		http_tuner_response("%s (value of register %ld is %d)\n", answer_str, reg, read_value);
	} else {
		http_tuner_response("%s (can't read value of register %ld)\n", answer_str, reg);
	}
	free_modbus_frame(&frame);
}

void
nt60_read_long_register(const char *value)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos + 1, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, ((reg >> 8) & 0xFF), ((reg >> 0) & 0xFF), 0x00, 0x02);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_value = (((long)answer[11]) << 24) | (((long)answer[12]) << 16) | (((long)answer[13]) << 8) | answer[14];
		http_tuner_response("%s (value of register %ld is %d)\n", answer_str, reg, read_value);
	} else {
		http_tuner_response("%s (can't read value of register %ld)\n", answer_str, reg);
	}
	free_modbus_frame(&frame);
}

static void
nt60_write_short_register(const char *value, const char *name, unsigned long reg_addr)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long data = strtol(comma_pos + 1, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x06, ((reg_addr >> 8) & 0xFF), ((reg_addr >> 0) & 0xFF), ((data >> 8) & 0xFF), ((data >> 0) & 0xFF));
	nt60_create_answer_string();
	if (answer_len > 13) {
		http_tuner_response("%s (%s set to %d)\n", answer_str, name, (((int)answer[12]) << 8) | answer[13]);
	} else {
		http_tuner_response("%s (failed to set %s)\n", answer_str, name);
	}
	free_modbus_frame(&frame);
}

static void
nt60_get_short_register(const char *value, const char *name, unsigned long reg_addr)
{
	const uint8_t address = strtol(value, NULL, 10);
	struct modbus_frame frame = {0};
	SEND_MODBUS_FRAME(frame, 6, address, 0x03, ((reg_addr >> 8) & 0xFF), ((reg_addr >> 0) & 0xFF), 0x00, 0x01);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_value = ((int)(answer[11]) << 8) | answer[12];
		http_tuner_response("%s (%s is %d)\n", answer_str, name, read_value);
	} else {
		http_tuner_response("%s (can't read %s)\n", answer_str, name);
	}
	free_modbus_frame(&frame);
}

void
nt60_set_speed(const char *value)
{
	nt60_write_short_register(value, "speed", 0x48);
}

void
nt60_get_speed(const char *value)
{
	nt60_get_short_register(value, "speed", 0x48);
}

void
nt60_set_acceleration(const char *value)
{
	nt60_write_short_register(value, "acceleration", 0x46);
}

void
nt60_get_acceleration(const char *value)
{
	nt60_get_short_register(value, "acceleration", 0x46);
}

void
nt60_set_deceleration(const char *value)
{
	nt60_write_short_register(value, "deceleration", 0x47);
}

void
nt60_get_deceleration(const char *value)
{
	nt60_get_short_register(value, "deceleration", 0x47);
}

void
nt60_set_current(const char *value)
{
	nt60_write_short_register(value, "current", 0x2D);
}

void
nt60_get_current(const char *value)
{
	nt60_get_short_register(value, "current", 0x2D);
}

void
nt60_set_tracking_error_threshold(const char *value)
{
	nt60_write_short_register(value, "tracking error alarm threshold", 0x29);
}

void
nt60_get_tracking_error_threshold(const char *value)
{
	nt60_get_short_register(value, "tracking error alarm threshold", 0x29);
}

void
nt60_save_config_to_flash(const char *value)
{
	nt60_write_short_register(value, "register 90", 0x5A);
}
