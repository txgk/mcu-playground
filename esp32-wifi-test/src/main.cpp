#include <Arduino.h>
#include <WiFi.h>
#include "../../wifi-credentials.h"

void
setup(void) {
	Serial.begin(9600);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print("Connecting to ");
		Serial.print(WIFI_SSID);
		Serial.print(" with password ");
		Serial.println(WIFI_PASSWORD);
		delay(500);
	}
	Serial.println("Connected!");
}

void
loop(void)
{
	Serial.println("I'm alive!");
	delay(1000);
}
