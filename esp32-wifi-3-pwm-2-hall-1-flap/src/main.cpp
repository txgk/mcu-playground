#include <WiFi.h>

#define WIFI_SSID "ssid"
#define WIFI_SECRET "secret"
#define DATA_LIM 42000
#define DEFAULT_SAMPLING_RESOLUTION 1000
#define DEFAULT_SAMPLING_PERIOD 100 // us
#define DEFAULT_THRESHOLD 2047
#define PORT 6503
#define FLAP_PIN 27
#define HALL_PIN 34
#define FLOW_PIN 35
#define PWM1_PIN 4
#define PWM2_PIN 5
#define PWM3_PIN 6
#define TIMER1_ID 0
#define TIMER2_ID 1

#define FLAP_PIN_DEFAULT_VALUE LOW
#define FLAP_PIN_PERISH_PERIOD 5000 // ms
#define PWM1_PIN_DEFAULT_VALUE 0    // 1 byte
#define PWM1_PIN_PERISH_PERIOD 5000 // ms
#define PWM2_PIN_DEFAULT_VALUE 0    // 1 byte
#define PWM2_PIN_PERISH_PERIOD 5000 // ms
#define PWM3_PIN_DEFAULT_VALUE 0    // 1 byte
#define PWM3_PIN_PERISH_PERIOD 5000 // ms

#if 1
#define DEBUG(...) Serial.print(__VA_ARGS__)
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#define DEBUGINIT(...) Serial.begin(__VA_ARGS__)
#else
#define DEBUG
#define DEBUGLN
#define DEBUGINIT
#endif

struct hall_data {
	bool has_stopped;
	bool want_to_reconfigure;
	uint16_t len;
	uint16_t delay;
	uint16_t current_lim;
	uint16_t desired_lim;
	uint16_t current_threshold;
	uint16_t desired_threshold;
	uint32_t start;
	uint32_t finish;
	uint64_t switchings;
};

WiFiServer slave(PORT);
WiFiClient master;

volatile uint16_t data[DATA_LIM];
volatile uint8_t transaction[2];

hw_timer_t *volatile timer1 = NULL;
hw_timer_t *volatile timer2 = NULL;

volatile struct hall_data hall1 = {0};
volatile struct hall_data hall2 = {0};

volatile uint32_t perish_flap_after = 0;
volatile uint32_t perish_pwm1_after = 0;
volatile uint32_t perish_pwm2_after = 0;
volatile uint32_t perish_pwm3_after = 0;

void
activate_hall_routine(volatile struct hall_data *hall)
{
	hall->switchings = 0;
	hall->len = 0;
	hall->has_stopped = hall->current_lim == 0 ? true : false;
}

void
try_sending_gathered_data(uint8_t sensor_id, volatile struct hall_data *hall)
{
	if ((hall->len != hall->current_lim) || (hall->current_lim == 0)) {
		DEBUG("Won't send data from channel "); DEBUG(sensor_id); DEBUGLN(" because packet is incomplete!");
		return; // Don't bother with empty or incomplete packets!
	}
	// Sensor ID
	master.write((uint8_t)0x00);
	master.write(sensor_id);
	// Transaction ID
	master.write(transaction[1]);
	master.write(transaction[0]);
	// Start (ms)
	master.write((uint8_t)((hall->start >> 24) & 0xFF));
	master.write((uint8_t)((hall->start >> 16) & 0xFF));
	master.write((uint8_t)((hall->start >>  8) & 0xFF));
	master.write((uint8_t)(hall->start         & 0xFF));
	// Finish (ms)
	master.write((uint8_t)((hall->finish >> 24) & 0xFF));
	master.write((uint8_t)((hall->finish >> 16) & 0xFF));
	master.write((uint8_t)((hall->finish >>  8) & 0xFF));
	master.write((uint8_t)(hall->finish         & 0xFF));
	// Frequency in tenths of Hertz
	static uint16_t freq = hall->start != hall->finish ? hall->switchings * 5000 / (hall->finish - hall->start) : 0;
	master.write((uint8_t)(freq >> 8));
	master.write((uint8_t)(freq & 0xFF));
	// Amount of points
	master.write((uint8_t)(hall->current_lim >> 8));
	master.write((uint8_t)(hall->current_lim & 0xFF));
	// Points themselves
	if (sensor_id == 1) {
		DEBUG("Sending "); DEBUG(sizeof(uint16_t) * hall->current_lim); DEBUGLN(" bytes from channel 1.");
		master.write((uint8_t *)(data), sizeof(uint16_t) * hall->current_lim);
	} else if (sensor_id == 2) {
		DEBUG("Sending "); DEBUG(sizeof(uint16_t) * hall->current_lim); DEBUGLN(" bytes from channel 2.");
		master.write((uint8_t *)(data + hall1.current_lim), sizeof(uint16_t) * hall->current_lim);
	}
}

void IRAM_ATTR
on_timer1(void)
{
	if (hall1.has_stopped == false) {
		data[hall1.len] = analogRead(HALL_PIN);
		if (hall1.len == 0) {
			hall1.start = millis();
		} else if (data[hall1.len] > hall1.current_threshold) {
			if (data[hall1.len - 1] < hall1.current_threshold) {
				hall1.switchings += 1;
			}
		} else {
			if (data[hall1.len - 1] > hall1.current_threshold) {
				hall1.switchings += 1;
			}
		}
		hall1.len += 1;
		if (hall1.len == hall1.current_lim) {
			hall1.finish = millis();
			hall1.has_stopped = true;
		}
	}
}

void IRAM_ATTR
on_timer2(void)
{
	if (hall2.has_stopped == false) {
		data[hall1.current_lim + hall2.len] = analogRead(FLOW_PIN);
		if (hall2.len == 0) {
			hall2.start = millis();
		} else if (data[hall1.current_lim + hall2.len] > hall2.current_threshold) {
			if (data[hall1.current_lim + hall2.len - 1] < hall2.current_threshold) {
				hall2.switchings += 1;
			}
		} else {
			if (data[hall1.current_lim + hall2.len - 1] > hall2.current_threshold) {
				hall2.switchings += 1;
			}
		}
		hall2.len += 1;
		if (hall2.len == hall2.current_lim) {
			hall2.finish = millis();
			hall2.has_stopped = true;
		}
	}
}

inline void
write_default_values_to_output_pins(void)
{
	digitalWrite(FLAP_PIN, FLAP_PIN_DEFAULT_VALUE);
	analogWrite(PWM1_PIN, PWM1_PIN_DEFAULT_VALUE);
	analogWrite(PWM2_PIN, PWM2_PIN_DEFAULT_VALUE);
	analogWrite(PWM3_PIN, PWM3_PIN_DEFAULT_VALUE);
}

inline void
write_default_values_to_hall_structures(void)
{
	hall1.has_stopped = true;
	hall1.want_to_reconfigure = true;
	hall1.len = 0;
	hall1.delay = DEFAULT_SAMPLING_PERIOD;
	hall1.current_lim = DEFAULT_SAMPLING_RESOLUTION;
	hall1.desired_lim = DEFAULT_SAMPLING_RESOLUTION;
	hall1.current_threshold = DEFAULT_THRESHOLD;
	hall1.desired_threshold = DEFAULT_THRESHOLD;
	hall2.has_stopped = true;
	hall2.want_to_reconfigure = true;
	hall2.len = 0;
	hall2.delay = DEFAULT_SAMPLING_PERIOD;
	hall2.current_lim = DEFAULT_SAMPLING_RESOLUTION;
	hall2.desired_lim = DEFAULT_SAMPLING_RESOLUTION;
	hall2.current_threshold = DEFAULT_THRESHOLD;
	hall2.desired_threshold = DEFAULT_THRESHOLD;
}

void
setup(void)
{
	DEBUGINIT(9600);

	pinMode(HALL_PIN, INPUT);
	pinMode(FLOW_PIN, INPUT);
	pinMode(FLAP_PIN, OUTPUT);
	pinMode(PWM1_PIN, OUTPUT);
	pinMode(PWM2_PIN, OUTPUT);
	pinMode(PWM3_PIN, OUTPUT);

	write_default_values_to_output_pins();

	WiFi.begin(WIFI_SSID, WIFI_SECRET);
	while (WiFi.status() != WL_CONNECTED) {
		DEBUGLN("Not connected to Wi-Fi yet!");
		delay(1000);
	}
	slave.begin();

	write_default_values_to_hall_structures();

	timer1 = timerBegin(TIMER1_ID, 80, true);
	timerAttachInterrupt(timer1, &on_timer1, true);
	timerAlarmWrite(timer1, DEFAULT_SAMPLING_PERIOD, true);
	timerAlarmEnable(timer1);
	timer2 = timerBegin(TIMER2_ID, 80, true);
	timerAttachInterrupt(timer2, &on_timer2, true);
	timerAlarmWrite(timer2, DEFAULT_SAMPLING_PERIOD, true);
	timerAlarmEnable(timer2);
}

void
read_packet(void)
{
	master.read(); // Ignore first channel byte.
	static uint8_t channel_value = master.read(); DEBUG("channel_value = "); DEBUGLN(channel_value);
	transaction[1] = master.read(); DEBUG("transaction[1] = "); DEBUGLN(transaction[1]);
	transaction[0] = master.read(); DEBUG("transaction[0] = "); DEBUGLN(transaction[0]);
	static uint8_t tmp1 = master.read(); DEBUG("tmp1 = "); DEBUGLN(tmp1);
	static uint8_t tmp2 = master.read(); DEBUG("tmp2 = "); DEBUGLN(tmp2);
	static uint16_t threshold_value = (((uint16_t)(tmp1)) << 8) + tmp2; DEBUG("threshold_value = "); DEBUGLN(threshold_value);
	tmp1 = master.read(); DEBUG("tmp1 = "); DEBUGLN(tmp1);
	tmp2 = master.read(); DEBUG("tmp2 = "); DEBUGLN(tmp2);
	static uint16_t period_value = (((uint16_t)(tmp1)) << 8) + tmp2; DEBUG("period_value = "); DEBUGLN(period_value);
	tmp1 = master.read(); DEBUG("tmp1 = "); DEBUGLN(tmp1);
	tmp2 = master.read(); DEBUG("tmp2 = "); DEBUGLN(tmp2);
	static uint16_t points_value = (((uint16_t)(tmp1)) << 8) + tmp2; DEBUG("points_value = "); DEBUGLN(points_value);

	if (points_value > DATA_LIM) {
		points_value = DATA_LIM; DEBUGLN("Lowered down points_value to match the actual data size!");
	}

	if (channel_value == 1) {
		hall1.has_stopped = true;
		hall1.want_to_reconfigure = true;
		hall1.desired_lim = points_value;
		hall1.delay = period_value;
		hall1.desired_threshold = threshold_value;
		if (hall1.desired_lim + hall2.desired_lim > DATA_LIM) {
			hall2.desired_lim = DATA_LIM - hall1.desired_lim;
		}
	} else if (channel_value == 2) {
		hall2.has_stopped = true;
		hall2.want_to_reconfigure = true;
		hall2.desired_lim = points_value;
		hall2.delay = period_value;
		hall2.desired_threshold = threshold_value;
		if (hall1.desired_lim + hall2.desired_lim > DATA_LIM) {
			hall1.desired_lim = DATA_LIM - hall2.desired_lim;
		}
	} else if (channel_value == 3) {
		digitalWrite(FLAP_PIN, period_value == 0 ? LOW : HIGH);
		perish_flap_after = millis() + FLAP_PIN_PERISH_PERIOD;
	} else if (channel_value == 4) {
		analogWrite(PWM1_PIN, period_value);
		perish_pwm1_after = millis() + PWM1_PIN_PERISH_PERIOD;
	} else if (channel_value == 5) {
		analogWrite(PWM2_PIN, period_value);
		perish_pwm2_after = millis() + PWM2_PIN_PERISH_PERIOD;
	} else if (channel_value == 6) {
		analogWrite(PWM3_PIN, period_value);
		perish_pwm3_after = millis() + PWM3_PIN_PERISH_PERIOD;
	}
}

volatile bool master_available = false;
void
loop(void)
{
	if (master_available == false) {
		DEBUGLN("Don't have a master yet...");
		master = slave.available();
		if (master.connected()) {
			DEBUGLN("I found myself a master!");
			master_available = true;
			master.setTimeout(5);
			write_default_values_to_output_pins();
			write_default_values_to_hall_structures();
		}
	} else if (master.connected()) {
		if (master.available() > 0) {
			/* DEBUGLN("Some data is available from the master!"); */
			read_packet();
		}
		if ((hall1.want_to_reconfigure == false) && (hall2.want_to_reconfigure == false)) {
			/* DEBUGLN("No one wants to change, trying to send gathered data..."); */
			if (hall1.has_stopped == true) {
				try_sending_gathered_data(1, &hall1);
				activate_hall_routine(&hall1);
			}
			if (hall2.has_stopped == true) {
				try_sending_gathered_data(2, &hall2);
				activate_hall_routine(&hall2);
			}
		} else if ((hall1.has_stopped == true) && (hall2.has_stopped == true)) {
			hall1.want_to_reconfigure = false;
			hall2.want_to_reconfigure = false;
			try_sending_gathered_data(1, &hall1);
			try_sending_gathered_data(2, &hall2);
			timerAlarmDisable(timer1);
			timerAlarmDisable(timer2);
			hall1.current_lim = hall1.desired_lim;
			hall2.current_lim = hall2.desired_lim;
			hall1.current_threshold = hall1.desired_threshold;
			hall2.current_threshold = hall2.desired_threshold;
			timerAlarmWrite(timer1, hall1.delay, true);
			timerAlarmWrite(timer2, hall2.delay, true);
			activate_hall_routine(&hall1);
			activate_hall_routine(&hall2);
			timerAlarmEnable(timer1);
			timerAlarmEnable(timer2);
		}
	} else {
		/* DEBUGLN("It seems my master has abandoned me..."); */
		master_available = false;
		master.stop();
	}

	static uint32_t current_millis = millis();
	if (current_millis > perish_flap_after) {
		digitalWrite(FLAP_PIN, FLAP_PIN_DEFAULT_VALUE);
		perish_flap_after = current_millis + FLAP_PIN_PERISH_PERIOD / 2;
	}
	if (current_millis > perish_pwm1_after) {
		analogWrite(PWM1_PIN, PWM1_PIN_DEFAULT_VALUE);
		perish_pwm1_after = current_millis + PWM1_PIN_PERISH_PERIOD / 2;
	}
	if (current_millis > perish_pwm2_after) {
		analogWrite(PWM2_PIN, PWM2_PIN_DEFAULT_VALUE);
		perish_pwm2_after = current_millis + PWM2_PIN_PERISH_PERIOD / 2;
	}
	if (current_millis > perish_pwm3_after) {
		analogWrite(PWM3_PIN, PWM3_PIN_DEFAULT_VALUE);
		perish_pwm3_after = current_millis + PWM3_PIN_PERISH_PERIOD / 2;
	}

	delay(10);
}
