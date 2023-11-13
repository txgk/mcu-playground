#include "main.h"
#include "common-modbus.h"

#define DEFAULT_ABS_POS_SHIFT_PER_PULSE 65536
// #define NT60_CURRENT_REGISTER           0x2D
// #define NT60_CURRENT_REGISTER           0xA5
#define NT60_CURRENT_REGISTER           0x19

#define ANSWER_SIZE_LIMIT 100
#define SEND_MODBUS_FRAME(...)                                                                     \
	do {                                                                                           \
		answer_len = 0;                                                                            \
		if (xSemaphoreTake(nt60_driver_lock, portMAX_DELAY) == pdTRUE) {                           \
			answer_len = uart_send_modbus(NT60_UART_PORT, answer, ANSWER_SIZE_LIMIT, __VA_ARGS__); \
			if (answer_len < 0) answer_len = 0;                                                    \
			xSemaphoreGive(nt60_driver_lock);                                                      \
			TASK_DELAY_MS(500); /* This delay matters! NT60 consumes cmds slowly! */               \
		}                                                                                          \
	} while (0)

// Use this macro when you want to access devices with different addresses at the same time!
#define SEND_MODBUS_FRAME_FAST(...)                                                                \
	do {                                                                                           \
		answer_len = 0;                                                                            \
		if (xSemaphoreTake(nt60_driver_lock, portMAX_DELAY) == pdTRUE) {                           \
			answer_len = uart_send_modbus(NT60_UART_PORT, answer, ANSWER_SIZE_LIMIT, __VA_ARGS__); \
			if (answer_len < 0) answer_len = 0;                                                    \
			xSemaphoreGive(nt60_driver_lock);                                                      \
		}                                                                                          \
	} while (0)

static SemaphoreHandle_t nt60_driver_lock = NULL;
static uint8_t answer[ANSWER_SIZE_LIMIT];
static int answer_len;
static char answer_str[1000];

static inline void
nt60_create_answer_string(void)
{
	char send_tmp[20];
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
	SEND_MODBUS_FRAME(6, address, 0x03, 0x00, 0x08, 0x00, 0x02);
	if (answer_len < 15) return 0;
	const long read_value = (((long)answer[11]) << 24) | (((long)answer[12]) << 16) | (((long)answer[13]) << 8) | answer[14];
	return read_value;
}

static inline void
nt60_set_current_absolute_position(uint8_t address, long pos)
{
	SEND_MODBUS_FRAME(11, address, 0x10, 0x00, 0x49, 0x00, 0x02, 0x04, ((pos >> 8) & 0xFF), ((pos >> 0) & 0xFF), ((pos >> 24) & 0xFF), ((pos >> 16) & 0xFF));
}

void
nt60_servo_setup(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	const long accel = 1;
	const long decel = 1;
	const long speed = 50;
	const long sgmnt = 4000;

	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x5B, 0x00, 0x01); // Reset to defaults

	// TODO TODO TODO
	// Check if delay of 500 ms is needed only here, not in every setup command.
	// TODO TODO TODO

	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x11, 0x00, 0x00); // Enable internal pulse

	SEND_MODBUS_FRAME(17, address, 0x10, 0x00, 0x14, 0x00, 0x05, 0x0A,
		0x00, 0x00,                       // 0x14 - Enable communication control
		0x00, 0x00,                       // 0x15 - Enable two phase mode
		0x00, 0x01,                       // 0x16 - Enable closed loop
		0x00, 0x00,                       // 0x17 - Set normal direction
		(sgmnt >> 8) & 0xFF, sgmnt & 0xFF // 0x18 - Set motor segmentation
	);

	SEND_MODBUS_FRAME(13, address, 0x10, 0x00, 0x46, 0x00, 0x03, 0x06,
		(accel >> 8) & 0xFF, accel & 0xFF, // 0x46 - Set acceleration
		(decel >> 8) & 0xFF, decel & 0xFF, // 0x47 - Set deceleration
		(speed >> 8) & 0xFF, speed & 0xFF  // 0x48 - Set speed
	);

	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x54, 0x00, 0x01); // Set movement mode to absolute

	// Enable inputs and outputs
	SEND_MODBUS_FRAME(23, address, 0x10, 0x00, 0x3C, 0x00, 0x08, 0x10,
		0x00, 0x23, // 0x3C - Enable quadrature encoder (B)
		0x00, 0x22, // 0x3D - Enable quadrature encoder (A)
		0x00, 0x27, // 0x3E
		0x00, 0x28, // 0x3F
		0x00, 0x2C, // 0x40
		0x00, 0x2B, // 0x41
		0x00, 0x11, // 0x42
		0x00, 0x12  // 0x43
	);
	SEND_MODBUS_FRAME(11, address, 0x10, 0x00, 0x66, 0x00, 0x02, 0x04,
		0x00, 0x15, // 0x66
		0x00, 0x14  // 0x67
	);

	http_tuner_response("Servo setup completed.\n");
}

void
nt60_seek_extremes(const char *value)
{
#define NT60_SEEK_DELTA 8

	const uint8_t address = strtol(value, NULL, 10);

	// Save previous current value
	SEND_MODBUS_FRAME(6, address, 0x03, 0x00, NT60_CURRENT_REGISTER, 0x00, 0x01);
	if (answer_len < 13) return;
	long old_current = ((long)(answer[11]) << 8) | answer[12];

	// Save previous speed value
	SEND_MODBUS_FRAME(6, address, 0x03, 0x00, 0x48, 0x00, 0x01);
	if (answer_len < 13) return;
	long old_speed = ((long)(answer[11]) << 8) | answer[12];

	// Set current to 50 and make sure it is set correctly
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, NT60_CURRENT_REGISTER, 0x00, 0x32);
	SEND_MODBUS_FRAME(6, address, 0x03, 0x00, NT60_CURRENT_REGISTER, 0x00, 0x01);
	if (answer_len < 13) return;
	const long current = ((int)(answer[11]) << 8) | answer[12];
	if (current != 50) return;

	// Set speed to 16
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x48, 0x00, 0x10);

	// Do the seeking for extremes
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x54, 0x00, 0x01); // Set movement mode to absolute
	long right_extreme, left_extreme;
	long center = nt60_get_current_absolute_position(address);
	long new_pos = center + 10000 * DEFAULT_ABS_POS_SHIFT_PER_PULSE;
	nt60_set_current_absolute_position(address, new_pos / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation
	while (true) {
		long pos1 = nt60_get_current_absolute_position(address);
		// They have delay between them anyway, so don't add more waiting.
		long pos2 = nt60_get_current_absolute_position(address);
		if ((labs(pos1 - pos2) <= DEFAULT_ABS_POS_SHIFT_PER_PULSE * NT60_SEEK_DELTA) || (pos2 >= new_pos)) {
			SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x06); // Stop rotation
			break;
		}
	}
	new_pos = center - 10000 * DEFAULT_ABS_POS_SHIFT_PER_PULSE;
	nt60_set_current_absolute_position(address, new_pos / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation
	while (true) {
		long pos1 = nt60_get_current_absolute_position(address);
		// They have delay between them anyway, so don't add more waiting.
		long pos2 = nt60_get_current_absolute_position(address);
		if ((labs(pos1 - pos2) <= DEFAULT_ABS_POS_SHIFT_PER_PULSE * NT60_SEEK_DELTA) || (pos2 <= new_pos)) {
			SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x06); // Stop rotation
			break;
		}
	}

	// Return to the initial position
	nt60_set_current_absolute_position(address, center / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x01); // Commit rotation

	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, NT60_CURRENT_REGISTER, ((old_current >> 8) & 0xFF), ((old_current >> 0) & 0xFF));
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x48, ((old_speed   >> 8) & 0xFF), ((old_speed   >> 0) & 0xFF));

	http_tuner_response("Extremes seeking completed.\n");
}

void
nt60_servo_stop(const char *value)
{
	const uint8_t address = strtol(value, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x06, 0x00, 0x12, 0x00, 0x06);
	nt60_create_answer_string();
	if (answer_len > 12) {
		http_tuner_response("%s\n", answer_str);
	} else {
		http_tuner_response("Servo %d doesn't respond to stop command!\n", (int)address);
	}
}

static void
nt60_rotate_absolute_or_relative(const char *value, bool is_absolute)
{
	const char *iter = value, *comma_pos;
	while (true) {
		if (*iter == '~') {
			unsigned long delay = strtoul(iter + 1, NULL, 10);
			TASK_DELAY_MS(delay);
			comma_pos = iter;
		} else {
			uint8_t address = strtol(iter, NULL, 10);
			comma_pos = strchr(iter, ',');
			if (comma_pos == NULL) return;
			long position = strtol(comma_pos + 1, NULL, 10);

			uint8_t rotation_cmd = 0x01;
			if (is_absolute == false && position < 0) {
				position = labs(position);
				rotation_cmd = 0x02; // Reverse for relative rotation action.
			}
			// Set movement mode
			SEND_MODBUS_FRAME_FAST(6, address, 0x06, 0x00, 0x54, 0x00, is_absolute ? 0x01 : 0x00);
			// Set position
			SEND_MODBUS_FRAME_FAST(11, address, 0x10, 0x00, 0x49, 0x00, 0x02, 0x04, ((position >> 8) & 0xFF), ((position >> 0) & 0xFF), ((position >> 24) & 0xFF), ((position >> 16) & 0xFF));
			// Commit rotation
			SEND_MODBUS_FRAME_FAST(6, address, 0x06, 0x00, 0x12, 0x00, rotation_cmd);
		}
		comma_pos = strchr(comma_pos + 1, ';');
		if (comma_pos == NULL) return;
		iter = comma_pos + 1;
	}
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
nt60_set_short_register(const char *value)
{
	char *comma_pos_1 = strchr(value, ',');
	if (comma_pos_1 == NULL) return;
	char *comma_pos_2 = strchr(comma_pos_1 + 1, ',');
	if (comma_pos_2 == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos_1 + 1, NULL, 10);
	const long data = strtol(comma_pos_2 + 1, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x06, (reg >> 8) & 0xFF, reg & 0xFF, (data >> 8) & 0xFF, data & 0xFF);
	nt60_create_answer_string();
	if (answer_len > 13) {
		http_tuner_response("%s (register %ld set to %ld)\n", answer_str, reg, (((int)answer[12]) << 8) | answer[13]);
	} else {
		http_tuner_response("%s (failed to set register %ld)\n", answer_str, reg);
	}
}

void
nt60_set_long_register(const char *value)
{
	char *comma_pos_1 = strchr(value, ',');
	if (comma_pos_1 == NULL) return;
	char *comma_pos_2 = strchr(comma_pos_1 + 1, ',');
	if (comma_pos_2 == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos_1 + 1, NULL, 10);
	const long data = strtol(comma_pos_2 + 1, NULL, 10);
	SEND_MODBUS_FRAME(11, address, 0x10, (reg >> 8) & 0xFF, reg & 0xFF, 0x00, 0x02, 0x04, ((data >> 8) & 0xFF), ((data >> 0) & 0xFF), ((data >> 24) & 0xFF), ((data >> 16) & 0xFF));
	nt60_create_answer_string();
	if (answer_len > 13) {
		http_tuner_response("%s (register %ld set to %ld)\n", answer_str, reg, (((int)answer[12]) << 8) | answer[13]);
	} else {
		http_tuner_response("%s (failed to set register %ld)\n", answer_str, reg);
	}
}

void
nt60_get_short_register(const char *value)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos + 1, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x03, ((reg >> 8) & 0xFF), ((reg >> 0) & 0xFF), 0x00, 0x01);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_value = ((int)(answer[11]) << 8) | answer[12];
		http_tuner_response("%s (value of register %ld is %d)\n", answer_str, reg, read_value);
	} else {
		http_tuner_response("%s (can't read value of register %ld)\n", answer_str, reg);
	}
}

void
nt60_get_long_register(const char *value)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long reg = strtol(comma_pos + 1, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x03, ((reg >> 8) & 0xFF), ((reg >> 0) & 0xFF), 0x00, 0x02);
	nt60_create_answer_string();
	if (answer_len > 12) {
		long read_value = (((long)answer[11]) << 24) | (((long)answer[12]) << 16) | (((long)answer[13]) << 8) | answer[14];
		if (reg == 8) {
			http_tuner_response("%s (current absolute position is %ld, encoded %ld)\n", answer_str, read_value, read_value / DEFAULT_ABS_POS_SHIFT_PER_PULSE);
		} else {
			http_tuner_response("%s (value of register %ld is %ld)\n", answer_str, reg, read_value);
		}
	} else {
		http_tuner_response("%s (can't read value of register %ld)\n", answer_str, reg);
	}
}

static void
nt60_write_short_register(const char *value, const char *name, unsigned long reg_addr)
{
	char *comma_pos = strchr(value, ',');
	if (comma_pos == NULL) return;
	const uint8_t address = strtol(value, NULL, 10);
	const long data = strtol(comma_pos + 1, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x06, ((reg_addr >> 8) & 0xFF), ((reg_addr >> 0) & 0xFF), ((data >> 8) & 0xFF), ((data >> 0) & 0xFF));
	nt60_create_answer_string();
	if (answer_len > 13) {
		http_tuner_response("%s (%s set to %d)\n", answer_str, name, (((int)answer[12]) << 8) | answer[13]);
	} else {
		http_tuner_response("%s (failed to set %s)\n", answer_str, name);
	}
}

static void
nt60_read_short_register(const char *value, const char *name, unsigned long reg_addr)
{
	const uint8_t address = strtol(value, NULL, 10);
	SEND_MODBUS_FRAME(6, address, 0x03, ((reg_addr >> 8) & 0xFF), ((reg_addr >> 0) & 0xFF), 0x00, 0x01);
	nt60_create_answer_string();
	if (answer_len > 12) {
		int read_value = ((int)(answer[11]) << 8) | answer[12];
		http_tuner_response("%s (%s is %d)\n", answer_str, name, read_value);
	} else {
		http_tuner_response("%s (can't read %s)\n", answer_str, name);
	}
}

void
nt60_set_speed(const char *value)
{
	nt60_write_short_register(value, "speed", 0x48);
}

void
nt60_get_speed(const char *value)
{
	nt60_read_short_register(value, "speed", 0x48);
}

void
nt60_set_acceleration(const char *value)
{
	nt60_write_short_register(value, "acceleration", 0x46);
}

void
nt60_get_acceleration(const char *value)
{
	nt60_read_short_register(value, "acceleration", 0x46);
}

void
nt60_set_deceleration(const char *value)
{
	nt60_write_short_register(value, "deceleration", 0x47);
}

void
nt60_get_deceleration(const char *value)
{
	nt60_read_short_register(value, "deceleration", 0x47);
}

void
nt60_set_current(const char *value)
{
	nt60_write_short_register(value, "current", NT60_CURRENT_REGISTER);
}

void
nt60_get_current(const char *value)
{
	nt60_read_short_register(value, "current", NT60_CURRENT_REGISTER);
}

void
nt60_set_tracking_error_threshold(const char *value)
{
	nt60_write_short_register(value, "tracking error alarm threshold", 0x29);
}

void
nt60_get_tracking_error_threshold(const char *value)
{
	nt60_read_short_register(value, "tracking error alarm threshold", 0x29);
}

void
nt60_save_config_to_flash(const char *value)
{
	nt60_write_short_register(value, "register 90", 0x5A);
}
