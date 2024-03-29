#include "main.h"

static volatile bool amt_driver_is_enabled = false;
static SemaphoreHandle_t amt_driver_lock = NULL;
static volatile unsigned int current_control = 3;
static volatile unsigned long int current_throttle = 0;

static const unsigned char crc_array[] = {
	0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
	0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
	0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
	0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
	0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
	0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
	0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d,
	0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
	0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
	0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
	0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58,
	0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
	0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
	0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
	0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
	0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
	0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f,
	0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
	0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
	0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
	0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
	0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
	0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1,
	0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
	0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
	0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
	0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
	0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
	0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a,
	0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
	0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
	0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35,
};

static unsigned char
calc_crc8(unsigned char *buf, unsigned char buf_len)
{
	unsigned char crc8 = 0;
	for (int i = 0; i < buf_len; ++i) {
		crc8 = crc_array[crc8^buf[i]];
	}
	return crc8;
}

// static void IRAM_ATTR
// amt_control_spammer(void *dummy)
// {
// 	uint8_t noctrl[] = {0xFF, 0x00, 0x00, 0x00};
// 	uint8_t off[] = {0xFF, 0x00, 0x00, 0x00};
// 	uint8_t ready[] = {0xFF, 0x18, 0x00, 0x9A};
// 	uint8_t start[] = {0xFF, 0x1C, 0x00, 0xA1};
// 	while (true) {
// 		if (xSemaphoreTake(amt_driver_lock, portMAX_DELAY) == pdTRUE) {
// 			switch (current_control) {
// 				case 1: uart_write_bytes(AMT_UART_PORT, off, 4); break;
// 				case 2: uart_write_bytes(AMT_UART_PORT, ready, 4); break;
// 				case 3: uart_write_bytes(AMT_UART_PORT, start, 4); break;
// 				default: uart_write_bytes(AMT_UART_PORT, noctrl, 4); break;
// 			}
// 			xSemaphoreGive(amt_driver_lock);
// 		}
// 		TASK_DELAY_MS(50);
// 	}
// 	vTaskDelete(NULL);
// }

static void IRAM_ATTR
amt_driver(void *dummy)
{
	char out[1000];
	int out_len;
	uint8_t c;
	uint8_t packet[10];
	size_t packet_len = 0;
	uint8_t packet_type = 0;
	bool in_packet = false;
	bool skip_byte = false;
	size_t packet_index = 0;
	while (true) {
		bool got_byte = false;
		if (xSemaphoreTake(amt_driver_lock, portMAX_DELAY) == pdTRUE) {
			if (uart_read_bytes(AMT_UART_PORT, &c, 1, MS_TO_TICKS(100)) == 1) {
				got_byte = true;
			}
			xSemaphoreGive(amt_driver_lock);
		}
		if (got_byte == false) {
			continue;
		}
		// out_len = snprintf(out, 1000, "0x%02X, ", c);
		// if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
		if (skip_byte == true) {
			skip_byte = false;
			continue;
		}
		if (in_packet == false) {
			if ((c & 0xF0) == 0xF0) {
				packet_type = c & 0x0F;
				if (packet_type > 0 && packet_type < 9) {
					in_packet = true;
					packet[0] = c;
					packet_len = 1;
				}
			}
		} else {
			packet[packet_len++] = c;
			if (packet_len == 7) {
				in_packet = false;
				packet_index += 1;
				if ((packet_index % 4) != 0) continue;
				if (packet[6] == calc_crc8(packet, 6)) {
					// stream_write("valid\n", 6);
					int64_t packet_birth = esp_timer_get_time() / 1000;
					unsigned long rpm = ((((unsigned long)packet[2]) << 8) + packet[1]) * 10;
					if (packet_type == 1) {
						unsigned int error_code = ( (((unsigned int)(packet[3])) & 0xE0) >> 5 ) | ( (((unsigned int)(packet[4])) & 0x03) << 3 );
						unsigned int engine_state = packet[3] & 0x1F;
						unsigned int switch_state = (packet[4] & 0x60) >> 5;
						unsigned int engine_temp = (( ((unsigned int)(packet[4] & 0x1C)) << 6 ) | ((unsigned int)(packet[5] & 0xFF)) ) - 50;
						out_len = snprintf(out, 1000, "UART_AMT_1@%lld=%lu,%u,%u,%u,%u\n",
							packet_birth,
							rpm,
							engine_state,
							error_code,
							engine_temp,
							switch_state
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 2) {
						unsigned int radio_voltage = packet[3];
						unsigned int power_voltage = packet[4];
						unsigned int pump_voltage  = packet[5];
						out_len = snprintf(out, 1000, "UART_AMT_2@%lld=%lu,%u,%u,%u\n",
							packet_birth,
							rpm,
							radio_voltage,
							power_voltage,
							pump_voltage
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 3) {
						unsigned int throttle = packet[3] > 100 ? 100 : packet[3];
						unsigned long pressure = (((unsigned long)packet[4]) | (((unsigned long)packet[5]) << 8)) * 2;
						out_len = snprintf(out, 1000, "UART_AMT_3@%lld=%lu,%u,%lu\n",
							packet_birth,
							rpm,
							throttle, // percent
							pressure // Pa
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 4) {
						unsigned int current = ((((unsigned int)packet[3]) << 0) | (((unsigned int)packet[4]) << 8)) & 0x1FF;
						unsigned int thrust = ((((unsigned int)packet[5]) << 0) | (((unsigned int)(packet[4] & 0xFE)) << 7)) & 0x7FFF;
						out_len = snprintf(out, 1000, "UART_AMT_4@%lld=%lu,%u,%u\n",
							packet_birth,
							rpm,
							current, // 0.1A
							thrust // 0.1Kg
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 5) {
						unsigned int pump_ignite_voltage = ((unsigned int)packet[3]) * 2; // 0.1v ~ 5.0v
						unsigned int curve_increase = packet[4];
						unsigned int curve_decrease = packet[5];
						out_len = snprintf(out, 1000, "UART_AMT_5@%lld=%lu,%u,%u,%u\n",
							packet_birth,
							rpm,
							pump_ignite_voltage,
							curve_increase,
							curve_decrease
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 6) {
						unsigned long int max_rpm = ((unsigned long)packet[3]) * 1000;
						unsigned int pump_max_voltage = packet[4];
						unsigned int version = (packet[5] >> 2) & 0x3F;
						unsigned int update_rate = packet[5] & 0x03;
						out_len = snprintf(out, 1000, "UART_AMT_6@%lld=%lu,%lu,%u,%u,%u\n",
							packet_birth,
							rpm,
							max_rpm,
							pump_max_voltage,
							version,
							update_rate
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 7) {
						unsigned int flow_rate = ((((unsigned int)packet[3]) << 0) | (((unsigned int)packet[4]) << 8)) & 0x3FF;
						unsigned int flow_total = ((((unsigned int)packet[4]) >> 2) | (((unsigned short)packet[5]) << 6)) & 0x3FFF;
						out_len = snprintf(out, 1000, "UART_AMT_7@%lld=%lu,%u,%u\n",
							packet_birth,
							rpm,
							flow_rate,
							flow_total
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					} else if (packet_type == 8) {
						unsigned long idle_rpm = ((unsigned long)packet[3]) * 1000;
						out_len = snprintf(out, 1000, "UART_AMT_8@%lld=%lu,%lu\n",
							packet_birth,
							rpm,
							idle_rpm
						);
						if (out_len > 0 && out_len < 1000) stream_write(out, out_len);
					}
				} else {
					// stream_write("invalid\n", 8);
					skip_byte = true;
				}
			}
		}
	}
	vTaskDelete(NULL);
}

bool
driver_amt_init(void)
{
	amt_driver_lock = xSemaphoreCreateMutex();
	if (amt_driver_lock == NULL) {
		return false;
	}
	uart_config_t amt_uart_cfg = {0};
	amt_uart_cfg.baud_rate = AMT_UART_SPEED;
	amt_uart_cfg.data_bits = UART_DATA_8_BITS;
	amt_uart_cfg.parity    = UART_PARITY_DISABLE;
	amt_uart_cfg.stop_bits = UART_STOP_BITS_1;
	amt_uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	// amt_uart_cfg.source_clk = UART_SCLK_DEFAULT;
#if CONFIG_UART_ISR_IN_IRAM
	uart_driver_install(AMT_UART_PORT, 1024, 0, 0, NULL, ESP_INTR_FLAG_IRAM);
#else
	uart_driver_install(AMT_UART_PORT, 1024, 0, 0, NULL, 0);
#endif
	uart_param_config(AMT_UART_PORT, &amt_uart_cfg);
	uart_set_pin(AMT_UART_PORT, AMT_UART_TX_PIN, AMT_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	xTaskCreatePinnedToCore(&amt_driver, "amt_driver", 4096, NULL, 1, NULL, 1);
	// xTaskCreatePinnedToCore(&amt_control_spammer, "amt_control_spammer", 4096, NULL, 1, NULL, 1);
	amt_driver_is_enabled = true;
	return true;
}

void
driver_amt_engine_set_throttle(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	if (amt_driver_is_enabled == false) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "AMT driver isn't enabled!\n");
		return;
	}
	const long new_throttle = strtol(value, NULL, 10);
	if (new_throttle < 0 || new_throttle > 1000) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "Throttle must be in range [0; 1000]!\n");
		return;
	}
	current_throttle = new_throttle;
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "Throttle set successfully!\n");
}

static inline void
driver_amt_engine_control_toggle(const char *value, char *answer_buf_ptr, int *answer_len_ptr, uint8_t mode)
{
	if (amt_driver_is_enabled == false) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "AMT driver isn't enabled!\n");
		return;
	}
	uint8_t cmd[4];
	cmd[0]  = 0xFF;
	cmd[1]  = 1 << 4;
	cmd[1] |= (mode & 0x03) << 2;
	cmd[1] |= (unsigned char)((current_throttle & 0x300)>>8);
	cmd[2]  = (unsigned char)((current_throttle & 0x0FF)>>0);
	cmd[3]  = calc_crc8(&cmd[1], 2);
	if (xSemaphoreTake(amt_driver_lock, portMAX_DELAY) == pdTRUE) {
		uart_write_bytes(AMT_UART_PORT, cmd, 4);
		xSemaphoreGive(amt_driver_lock);
	}
	*answer_len_ptr = snprintf(
		answer_buf_ptr,
		HTTP_TUNER_ANSWER_SIZE_LIMIT,
		"Sent: %02X %02X %02X %02X\n",
		cmd[0],
		cmd[1],
		cmd[2],
		cmd[3]
	);
}

void
driver_amt_engine_control_drop(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_engine_control_toggle(value, answer_buf_ptr, answer_len_ptr, 0);
}

void
driver_amt_engine_control_off(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_engine_control_toggle(value, answer_buf_ptr, answer_len_ptr, 1);
}

void
driver_amt_engine_control_ready(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_engine_control_toggle(value, answer_buf_ptr, answer_len_ptr, 2);
}

void
driver_amt_engine_control_start(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_engine_control_toggle(value, answer_buf_ptr, answer_len_ptr, 3);
}

static inline void
driver_amt_send_short_cmd(const char *value, char *answer_buf_ptr, int *answer_len_ptr, unsigned int id)
{
	if (amt_driver_is_enabled == false) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "AMT driver isn't enabled!\n");
		return;
	}
	unsigned char cmd[8];
	cmd[0]  = 0xFF;   // first header
	cmd[1]  = 1 << 4; // command id
	cmd[1] |= 3 << 2; // enable control
	cmd[1] |= (unsigned char)((current_throttle & 0x300)>>8);
	cmd[2]  = (unsigned char)((current_throttle & 0x0FF)>>0);
	cmd[3]  = calc_crc8(&cmd[1], 2);
	cmd[4]  = 0xFF;   // second header
	cmd[5]  = 2 << 4; // command id
	cmd[6]  = id;     // command param
	cmd[7]  = calc_crc8(&cmd[5], 2);
	if (xSemaphoreTake(amt_driver_lock, portMAX_DELAY) == pdTRUE) {
		uart_write_bytes(AMT_UART_PORT, cmd, 8);
		xSemaphoreGive(amt_driver_lock);
	}
	*answer_len_ptr = snprintf(
		answer_buf_ptr,
		HTTP_TUNER_ANSWER_SIZE_LIMIT,
		"Sent: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		cmd[0],
		cmd[1],
		cmd[2],
		cmd[3],
		cmd[4],
		cmd[5],
		cmd[6],
		cmd[7]
	);
}

static inline void
driver_amt_send_tune_cmd(const char *value, char *answer_buf_ptr, int *answer_len_ptr, unsigned int id, unsigned int data)
{
	if (amt_driver_is_enabled == false) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "AMT driver isn't enabled!\n");
		return;
	}
	unsigned char cmd[8];
	cmd[0]  = 0xFF;    // first header
	cmd[1]  = 3 << 4;  // command id
	cmd[2]  = 0;       // unlock
	cmd[3]  = calc_crc8(&cmd[1], 2);
	cmd[4]  = 0xFF;    // second header
	cmd[5]  = id << 4; // command id
	cmd[6]  = data;    // command param
	cmd[7]  = calc_crc8(&cmd[5], 2);
	if (xSemaphoreTake(amt_driver_lock, portMAX_DELAY) == pdTRUE) {
		uart_write_bytes(AMT_UART_PORT, cmd, 8);
		xSemaphoreGive(amt_driver_lock);
	}
	*answer_len_ptr = snprintf(
		answer_buf_ptr,
		HTTP_TUNER_ANSWER_SIZE_LIMIT,
		"Sent: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		cmd[0],
		cmd[1],
		cmd[2],
		cmd[3],
		cmd[4],
		cmd[5],
		cmd[6],
		cmd[7]
	);
}

void
driver_amt_engine_exhaust_fuel(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 1);
}

void
driver_amt_engine_test_glowplug(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 2);
}

void
driver_amt_engine_test_main_valve(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 3);
}

void
driver_amt_engine_test_ignition_valve(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 4);
}

void
driver_amt_engine_test_pump(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 5);
}

void
driver_amt_engine_test_starter(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 6);
}

void
driver_amt_engine_set_update_rate_20hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 7);
}

void
driver_amt_engine_set_update_rate_50hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 8);
}

void
driver_amt_engine_set_update_rate_100hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 9);
}

void
driver_amt_engine_reset_fuel_flow(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 10);
}

void
driver_amt_engine_calibrate_thrust_zero(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	driver_amt_send_short_cmd(value, answer_buf_ptr, answer_len_ptr, 11);
}

void
driver_amt_engine_set_ignition_pump_voltage(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const double data_raw = atof(value);
	const unsigned int data = data_raw * 100 / 2;
	driver_amt_send_tune_cmd(value, answer_buf_ptr, answer_len_ptr, 4, data);
}

void
driver_amt_engine_set_acceleration_curve(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const long data = strtol(value, NULL, 10);
	driver_amt_send_tune_cmd(value, answer_buf_ptr, answer_len_ptr, 5, data);
}
