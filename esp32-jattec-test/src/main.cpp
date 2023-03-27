#include <Arduino.h>
#include <WiFi.h>
#include "wifi-credentials.h"

#define WIFI_DATA_PORT 2222
#define SERIAL_SPEED 115200

const size_t commands_count = 6;
const char *commands_list[] = {
	"1,RAC,1\r",
	"1,RSS,1\r",
	"1,RFI,1\r",
	"1,RAI,1\r",
	"1,RRC,1\r",
	"1,RI1,1\r",
};

WiFiServer server(WIFI_DATA_PORT);
WiFiClient client;

void
client_write(char byte_value)
{
	if (byte_value == '\r') byte_value = '$';
	if (client.connected()) {
		client.write(byte_value);
	} else {
		client = server.available();
		if (client.connected()) {
			client.write(byte_value);
		}
	}
}

void
setup(void)
{
	Serial.begin(SERIAL_SPEED);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	server.begin();
}

void
loop(void)
{
	for (size_t i = 0; i < commands_count; ++i) {
		Serial.write(commands_list[i]);
		delay(100);
		if (Serial.available()) {
			do {
				client_write(Serial.read());
			} while (Serial.available());
			client_write('\n');
		}
		delay(500);
	}
}
