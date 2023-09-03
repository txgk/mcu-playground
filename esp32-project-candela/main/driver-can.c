#include "candela.h"

static SemaphoreHandle_t can_messenger_lock = NULL;

static void IRAM_ATTR
can_listener(void *dummy)
{
	char out[1000];
	twai_message_t message;
	while (true) {
		if (xSemaphoreTake(can_messenger_lock, portMAX_DELAY) == pdTRUE) {
			if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
				// Serial.printf("Received message %d in %s format\n", message.identifier, message.extd ? "extended" : "standard");
				if (!(message.rtr)) {
					unsigned long packet_birth = esp_timer_get_time() / 1000;
					if ((message.identifier == JETCAT_CAN_ID_BASE + 0) && (message.data_length_code == 8)) {
						unsigned long set_rpm = (unsigned long)(((unsigned int)message.data[0] << 8) + message.data[1]) * 10; // rpm
						unsigned long real_rpm = (unsigned long)(((unsigned int)message.data[2] << 8) + message.data[3]) * 10; // rpm
						float exhaust_gas_temp = (float)(((unsigned int)message.data[4] << 8) + message.data[5]) / 10.0; // celsius degree
						int engine_state = message.data[6]; // flags
						float pump_volts = (float)message.data[7] / 10.0; // volts
						int len = snprintf(out, 1000,
							"CAN_VALUES@%lu=%lu,%lu,%.1f,%d,%.1f\n",
							packet_birth,
							set_rpm,
							real_rpm,
							exhaust_gas_temp,
							engine_state,
							pump_volts
						);
						if (len > 0 && len < 1000) write_websocket_message(out, len);
					} else if ((message.identifier == JETCAT_CAN_ID_BASE + 1) && (message.data_length_code == 6)) {
						float bat_volts = (float)(message.data[0]) / 10.0; // volts
						float engine_amps = (float)(message.data[1]) / 10.0; // amps
						float generator_volts = (float)(message.data[2]) / 5.0; // volts
						float generator_amps = (float)(message.data[3]) / 10.0; // amps
						int len = snprintf(out, 1000,
							"CAN_ELECTRO@%lu=%.1f,%.1f,%.1f,%.1f,%d\n",
							packet_birth,
							bat_volts,
							engine_amps,
							generator_volts,
							generator_amps,
							message.data[4]
							// message.data[4] & 1  ? 1 : 0, // engine_connected
							// message.data[4] & 2  ? 1 : 0, // fuel_pump_connected
							// message.data[4] & 4  ? 1 : 0, // smoke_pump_connected
							// message.data[4] & 8  ? 1 : 0, // gsu_connected
							// message.data[4] & 16 ? 1 : 0, // egt_ok_flag
							// message.data[4] & 32 ? 1 : 0, // glow_plug_ok_flag
							// message.data[4] & 64 ? 1 : 0  // flow_sensor_ok_flag
						);
						if (len > 0 && len < 1000) write_websocket_message(out, len);
					} else if ((message.identifier == JETCAT_CAN_ID_BASE + 2) && (message.data_length_code == 8)) {
						unsigned long fuel_flow = (unsigned long)(((int)message.data[0] << 8) + message.data[1]); // ml/min
						unsigned long fuel_consum = (unsigned long)(((int)message.data[2] << 8) + message.data[3]) * 10; // ml
						float air_pressure = (float)(((unsigned int)message.data[4] << 8) + message.data[5]) / 50.0; // mbar
						float ecu_temp = (float)message.data[6] / 2.0 + 30; // celsius degree
						int len = snprintf(out, 1000,
							"CAN_FUEL@%lu=%lu,%lu,%.1f,%.1f\n",
							packet_birth,
							fuel_flow,
							fuel_consum,
							air_pressure,
							ecu_temp
						);
						if (len > 0 && len < 1000) write_websocket_message(out, len);
					} else {
						// for (int i = 0; i < message.data_length_code; i++) {
							// Serial.printf("Data byte %d = %d\n", i, message.data[i]);
						// }
					}
				}
			}
			xSemaphoreGive(can_messenger_lock);
		}
	}
	vTaskDelete(NULL);
}

bool
start_can_messenger(void)
{
	if (check_if_can_driver_is_started()) {
		can_messenger_lock = xSemaphoreCreateMutex();
		if (can_messenger_lock == NULL) {
			return false;
		}
		xTaskCreatePinnedToCore(&can_listener, "can_listener", 4096, NULL, 1, NULL, 1);
	}
	return true;
}

static inline void
send_jetcat_message(uint32_t id, const uint8_t *data, uint8_t size, const char *ok_msg, const char *fail_msg, char *answer_buf_ptr, int *answer_len_ptr)
{
	twai_message_t message;
	message.extd = 0;
	message.rtr = 0;
	message.ss = 0;
	message.self = 0;
	message.dlc_non_comp = 0;
	message.identifier = id;
	message.data_length_code = size;
	for (uint8_t i = 0; i < size; ++i) {
		message.data[i] = data[i];
	}
	bool success = false;
	if (xSemaphoreTake(can_messenger_lock, portMAX_DELAY) == pdTRUE) {
		if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
			success = true;
		}
		xSemaphoreGive(can_messenger_lock);
	}
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, success ? ok_msg : fail_msg);
}


void
can_jetcat_engine_on(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const uint8_t data = 1;
	send_jetcat_message(JETCAT_CAN_ID_BASE1 + 0, &data, 1, "Engine is on!\n", "Failed to enable engine!\n", answer_buf_ptr, answer_len_ptr);
}

void
can_jetcat_engine_off(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const uint8_t data = 0;
	send_jetcat_message(JETCAT_CAN_ID_BASE1 + 0, &data, 1, "Engine is off!\n", "Failed to disable engine!\n", answer_buf_ptr, answer_len_ptr);
}

void
can_jetcat_engine_thrust(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const long raw = strtol(value, NULL, 10) * 10;
	const uint8_t data[2] = {(raw >> 8) & 0xFF, raw & 0xFF};
	send_jetcat_message(JETCAT_CAN_ID_BASE1 + 1, data, 2, "Thrust is set!\n", "Failed to set thrust!\n", answer_buf_ptr, answer_len_ptr);
}
