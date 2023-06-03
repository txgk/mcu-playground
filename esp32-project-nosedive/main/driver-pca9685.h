#ifndef PCA9685_H
#define PCA9685_H
typedef uint8_t pca9685_ch_t;
enum pca9685_ch {
	PCA9685_CH0 = 0,
	PCA9685_CH1,
	PCA9685_CH2,
	PCA9685_CH3,
	PCA9685_CH4,
	PCA9685_CH5,
	PCA9685_CH6,
	PCA9685_CH7,
	PCA9685_CH8,
	PCA9685_CH9,
	PCA9685_CH10,
	PCA9685_CH11,
	PCA9685_CH12,
	PCA9685_CH13,
	PCA9685_CH14,
	PCA9685_CH15,
	PCA9685_ALL,
};

void pca9685_initialize(void);

int pca9685_read_mode1(void);
int pca9685_read_subadr1(void);
int pca9685_read_subadr2(void);
int pca9685_read_subadr3(void);

void pca9685_channel_setup(pca9685_ch_t ch, int duty_percent, int shift_percent);
void pca9685_channel_full_toggle(pca9685_ch_t ch, bool on);
void pca9685_change_frequency(long frequency_hz);
#endif // PCA9685_H
