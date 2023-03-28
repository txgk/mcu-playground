#define SDA_PIN                          6
#define SCL_PIN                          7
#define SERIAL_SPEED                     115200
#define SDA                              digitalReadDirect(SDA_PIN)
#define SCL                              digitalReadDirect(SCL_PIN)
#define SCL_OR_SDA                       ((digitalReadDirect(SCL_PIN) << 1) | digitalReadDirect(SDA_PIN))
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA == 0b10)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA == 0b11)
#define IS_ACKNOWLEDGED                  '>'
#define NOT_ACKNOWLEDGED                 'x'

#define I2C_BUFFER_SIZE                  10000

#define WAIT_SDA_RISE                    while (SDA == 0)
#define WAIT_SDA_DROP                    while (SDA != 0)
#define WAIT_SCL_RISE                    while (SCL == 0)
#define WAIT_SCL_DROP                    while (SCL != 0)
#define WAIT_FOR_SDA_AND_SCL_RISE        while (SCL_OR_SDA != 0b11)
#define WAIT_FOR_SDA_OR_SCL_DROP         while (SCL_OR_SDA == 0b11)
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    while (SCL_OR_SDA == 0b10)

volatile uint8_t b;
char str[I2C_BUFFER_SIZE];
volatile size_t str_len = 0;

inline uint8_t
digitalReadDirect(int pin)
{
	return !!(g_APinDescription[pin].pPort->PIO_PDSR & g_APinDescription[pin].ulPin);
}

void
print_data(void)
{
	Serial.write(str, str_len);
	Serial.flush();
}

void
setup(void)
{
	pinMode(SDA_PIN, INPUT);
	pinMode(SCL_PIN, INPUT);
	Serial.begin(SERIAL_SPEED);
}

void
loop(void)
{
i2c_sync:
	WAIT_FOR_SDA_AND_SCL_RISE;
i2c_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!SDA_IS_LOW_AND_SCL_IS_HIGH) goto i2c_sync;
i2c_start:
	str_len = sprintf(str, "\n%lu=", millis());
	WAIT_SCL_DROP;
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (SDA) {
		WAIT_SCL_DROP;
		str[str_len++] = NOT_ACKNOWLEDGED;
		print_data();
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		str[str_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
i2c_next:
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (SDA) {
		str[str_len++] = NOT_ACKNOWLEDGED;
	} else {
		str[str_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) b = SDA;
	if (b) {
		WAIT_FOR_SDA_OR_SCL_DROP;
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) { // Repeated start signal.
			goto i2c_start;
		}
	} else {
		WAIT_FOR_SDA_RISE_OR_SCL_DROP;
		if (SDA_AND_SCL_ARE_HIGH) { // Stop signal.
			print_data();
			goto i2c_sync;
		}
	}

	b <<= 1;
	WAIT_SCL_DROP;
	goto i2c_next;
}
