#include "nosedive.h"
#include "esp_wifi.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"

// Undocumented secret function...
uint8_t temprature_sens_read();

static inline const char *
get_esp_model_string(esp_chip_model_t id)
{
	switch (id) {
		case CHIP_ESP32:   return "ESP32";
		case CHIP_ESP32S2: return "ESP32S2";
		case CHIP_ESP32S3: return "ESP32S3";
		case CHIP_ESP32C3: return "ESP32C3";
		case CHIP_ESP32C2: return "ESP32C2";
		case CHIP_ESP32C6: return "ESP32C6";
		case CHIP_ESP32H2: return "ESP32H2";
		default:           return "UNKNOWN";
	}
	return "UNKNOWN";
}

void
get_system_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	esp_chip_info_t chip_info;
	uint8_t mac[6];
	esp_chip_info(&chip_info);
#ifdef NOSEDIVE_USE_WIFI_STATION
	esp_wifi_get_mac(WIFI_IF_STA, mac);
#else
	esp_wifi_get_mac(WIFI_IF_AP, mac);
#endif
	*answer_len_ptr = snprintf(
		answer_buf_ptr,
		HTTP_TUNER_ANSWER_SIZE_LIMIT,
		"\n"
		"Model: %s\n"
		"Revision: %d\n"
		"Cores: %d\n"
		"Features mask: %ld\n"
		"Firmware codeword: %s (%s %s)\n"
		"ESP-IDF version: %s\n"
		"MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
		get_esp_model_string(chip_info.model),
		chip_info.revision,
		chip_info.cores,
		chip_info.features,
		FIRMWARE_CODEWORD,
		__DATE__,
		__TIME__,
		esp_get_idf_version(),
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
	);
}

void
get_temperature_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	const int temp = ((int)temprature_sens_read() - 32) * 5 / 9;
	*answer_len_ptr = snprintf(
		answer_buf_ptr,
		HTTP_TUNER_ANSWER_SIZE_LIMIT,
		"%d\n",
		temp
	);
}
