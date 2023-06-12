#include "nosedive.h"
#include "esp_wifi.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"

#define SYSTEM_INFO_BUF_SIZE 500
static char system_info_buf[SYSTEM_INFO_BUF_SIZE];
static struct data_piece system_info;

static inline const char *
get_esp_model_string(esp_chip_model_t id)
{
	switch (id) {
		case CHIP_ESP32:   return "ESP32";
		case CHIP_ESP32S2: return "ESP32S2";
		case CHIP_ESP32S3: return "CHIP_ESP32S3";
		case CHIP_ESP32C3: return "CHIP_ESP32C3";
		case CHIP_ESP32C2: return "CHIP_ESP32C2";
		case CHIP_ESP32H2: return "CHIP_ESP32H2";
	}
	return "UNKNOWN";
}

void
create_system_info_string(void)
{
	esp_chip_info_t chip_info;
	uint8_t mac[6];
	esp_chip_info(&chip_info);
#ifdef NOSEDIVE_USE_WIFI_STATION
	esp_wifi_get_mac(WIFI_IF_STA, mac);
#else
	esp_wifi_get_mac(WIFI_IF_AP, mac);
#endif
	system_info.len = snprintf(
		system_info_buf,
		SYSTEM_INFO_BUF_SIZE,
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
	system_info.ptr = system_info_buf;
}

const struct data_piece *
get_system_info_string(const char *dummy)
{
	return system_info.len > 0 && system_info.len < SYSTEM_INFO_BUF_SIZE ? &system_info : NULL;
}
