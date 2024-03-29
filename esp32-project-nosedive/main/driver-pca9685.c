#include "nosedive.h"
#include "driver-pca9685.h"

#define PCA9685_SUCCESS_MESSAGE "PCA9685: УСПЕХ\n"
#define PCA9685_FAILURE_MESSAGE "PCA9685: ОШИБКА\n"

struct pca9685_channel_setup {
	uint8_t on_l;
	uint8_t on_h;
	uint8_t off_l;
	uint8_t off_h;
};

static const struct pca9685_channel_setup channels[] = {
	[PCA9685_CH0]  = {0x06, 0x07, 0x08, 0x09},
	[PCA9685_CH1]  = {0x0A, 0x0B, 0x0C, 0x0D},
	[PCA9685_CH2]  = {0x0E, 0x0F, 0x10, 0x11},
	[PCA9685_CH3]  = {0x12, 0x13, 0x14, 0x15},
	[PCA9685_CH4]  = {0x16, 0x17, 0x18, 0x19},
	[PCA9685_CH5]  = {0x1A, 0x1B, 0x1C, 0x1D},
	[PCA9685_CH6]  = {0x1E, 0x1F, 0x20, 0x21},
	[PCA9685_CH7]  = {0x22, 0x23, 0x24, 0x25},
	[PCA9685_CH8]  = {0x26, 0x27, 0x28, 0x29},
	[PCA9685_CH9]  = {0x2A, 0x2B, 0x2C, 0x2D},
	[PCA9685_CH10] = {0x2E, 0x2F, 0x30, 0x31},
	[PCA9685_CH11] = {0x32, 0x33, 0x34, 0x35},
	[PCA9685_CH12] = {0x36, 0x37, 0x38, 0x39},
	[PCA9685_CH13] = {0x3A, 0x3B, 0x3C, 0x3D},
	[PCA9685_CH14] = {0x3E, 0x3F, 0x40, 0x41},
	[PCA9685_CH15] = {0x42, 0x43, 0x44, 0x45},
	[PCA9685_ALL]  = {0xFA, 0xFB, 0xFC, 0xFD},
};

void
pca9685_initialize(void)
{
	uint8_t data[] = {
		0b00000000, // MODE1 register address
		0b00100001, // Enable auto-increment and disable sleep
	};
	i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(2000));
}

int
pca9685_read_mode1(void)
{
	return i2c_read_one_byte_from_register(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, 0, 2000);
}

int
pca9685_read_subadr1(void)
{
	return i2c_read_one_byte_from_register(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, 2, 2000);
}

static bool
pca9685_channel_setup(pca9685_ch_t ch, int duty_percent, int shift_percent)
{
	if (ch > PCA9685_ALL) {
		return false;
	}
	int moment_on = 4095 * shift_percent / 100;
	int moment_off = moment_on + (4095 * duty_percent / 100);
	uint8_t data[] = {
		channels[ch].on_l,
		moment_on         & 0xFF,
		(moment_on >> 8)  & 0x0F,
		moment_off        & 0xFF,
		(moment_off >> 8) & 0x0F,
	};
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(2000)) == ESP_OK) {
		return true;
	}
	return false;
}

bool
pca9685_channel_full_toggle(pca9685_ch_t ch, bool on)
{
	if (ch > PCA9685_ALL) {
		return false;
	}
	uint8_t data[] = {
		channels[ch].on_l,
		on ? 0xFF : 0x00,
		on ? 0xFF : 0x00,
		on ? 0x00 : 0xFF,
		on ? 0x00 : 0xFF,
	};
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data, sizeof(data), MS_TO_TICKS(2000)) == ESP_OK) {
		return true;
	}
	return false;
}

bool
pca9685_change_frequency(long frequency_hz)
{
	// Changing PWM frequency requires entering sleep mode.
	uint8_t data1[] = {0b00000000, 0b00110001};
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data1, sizeof(data1), MS_TO_TICKS(2000)) != ESP_OK) {
		return false;
	}

	// According to DS, 24 Hz is 0xFF and 1526 Hz is 0x03.
	if (frequency_hz < 24) {
		frequency_hz = 24;
	} else if (frequency_hz > 1525) {
		frequency_hz = 1525;
	}
	uint8_t data2[] = {
		0xFE,
		25000000 / 4096 / frequency_hz - 1 // Based on expression in DS
	};
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data2, sizeof(data2), MS_TO_TICKS(2000)) != ESP_OK) {
		return false;
	}

	// Leave sleep mode.
	data1[1] = 0b00100001;
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data1, sizeof(data1), MS_TO_TICKS(2000)) != ESP_OK) {
		return false;
	}

	TASK_DELAY_MS(1); // Give oscillator time to stabilize.

	// Restart PWM channels (without this they will keep sleeping).
	data1[1] = 0b10100001;
	if (i2c_master_write_to_device(PCA9685_I2C_PORT, PCA9685_I2C_ADDRESS, data1, sizeof(data1), MS_TO_TICKS(2000)) != ESP_OK) {
		return false;
	}

	return true;
}

void
pca9685_http_handler_pcaset(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	long ch = 0, duty = 100;
	uint8_t field = 0;
	char buf[10];
	uint8_t buf_len = 0;
	for (const char *i = value; ; ++i) {
		if (*i == ',' || *i == '\0') {
			buf[buf_len] = '\0';
			buf_len = 0;
			field += 1;
			if (field == 1) {
				if (strcmp(buf, "all") == 0) {
					ch = PCA9685_ALL;
				} else {
					ch = strtol(buf, NULL, 10);
				}
			} else if (field == 2) {
				duty = strtol(buf, NULL, 10);
			}
			if (*i == '\0') {
				break;
			}
		} else if (buf_len < 9) {
			buf[buf_len++] = *i;
		}
	}
	if (field > 0 && pca9685_channel_setup(PCA9685_CH0 + ch, duty, 0) == true) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_SUCCESS_MESSAGE);
	} else {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_FAILURE_MESSAGE);
	}
}

static void
pca9685_http_handler_pcamax_pcaoff(const char *value, char *answer_buf_ptr, int *answer_len_ptr, bool on)
{
	int channel = strcmp(value, "all") == 0 ? PCA9685_ALL : PCA9685_CH0 + strtol(value, NULL, 10);
	if (pca9685_channel_full_toggle(channel, on) == true) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_SUCCESS_MESSAGE);
	} else {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_FAILURE_MESSAGE);
	}
}

void
pca9685_http_handler_pcamax(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	pca9685_http_handler_pcamax_pcaoff(value, answer_buf_ptr, answer_len_ptr, true);
}

void
pca9685_http_handler_pcaoff(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	pca9685_http_handler_pcamax_pcaoff(value, answer_buf_ptr, answer_len_ptr, false);
}

void
pca9685_http_handler_pcafreq(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	long freq = strtol(value, NULL, 10);
	if (pca9685_change_frequency(freq) == true) {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_SUCCESS_MESSAGE);
	} else {
		*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, PCA9685_FAILURE_MESSAGE);
	}
}
