#include <WiFi.h>
#include "../../wifi-credentials.h"

#define CONTROL_PORT 882
#define STREAM_PORT 80
#define CONTROL_HEADER "HTTP/1.1 200 OK\n\n"
#define STREAM_HEADER "HTTP/1.1 200 OK\nContent-type: text/event-stream\nAccess-Control-Allow-Origin: *\n"
#define CMD_MAX_LEN 50

#if 1
#define DEBUG(...) Serial.print(__VA_ARGS__)
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#define DEBUGINIT(...) Serial.begin(__VA_ARGS__)
#else
#define DEBUG
#define DEBUGLN
#define DEBUGINIT
#endif

WiFiServer server_control(CONTROL_PORT);
WiFiServer server_stream(STREAM_PORT);
WiFiClient commander;
WiFiClient listener;
bool listener_available = false;

void setup() {
	DEBUGINIT(9600);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		DEBUGLN("Not connected to Wi-Fi yet!");
		delay(1000);
	}
	DEBUG("Connected as: ");
	DEBUGLN(WiFi.localIP());
	server_control.begin();
	server_stream.begin();
}

uint8_t c;
char cmd[CMD_MAX_LEN + 1];
size_t cmd_len;
bool prev_token_is_get = false;
unsigned long delay1 = 500, nextread1 = 0;
void loop() {

	commander = server_control.available();
	if (commander.connected()) {
		for (cmd_len = 0; commander.available() > 0;) {
			c = commander.read();
			if (c == ' ') {
				if (prev_token_is_get == true) {
					cmd[cmd_len] = '\0';
					DEBUGLN(cmd);
					prev_token_is_get = false;
				} else if ((cmd_len == 3)
					&& (cmd[0] == 'G')
					&& (cmd[1] == 'E')
					&& (cmd[2] == 'T'))
				{
					prev_token_is_get = true;
				}
				cmd_len = 0;
			} else if (cmd_len < CMD_MAX_LEN) {
				cmd[cmd_len++] = c;
			}
		}
		commander.print(CONTROL_HEADER);
	}

	if (listener_available == false) {
		DEBUGLN("Don't have a listener yet...");
		listener = server_stream.available();
		if (listener.connected()) {
			DEBUGLN("I found myself a new listener!");
			listener_available = true;
			listener.setTimeout(5);
			listener.print(STREAM_HEADER);
		} else {
			delay(200);
		}
	} else if (listener.connected()) {
		if (millis() > nextread1) {
			listener.printf("data:%d,%d\n\n", millis() % 5, millis() % 150);
			delay(1);
			/* listener.printf("data:1,%d,%d\n\n", (millis() / 1000), ((millis() % 150))); */
			nextread1 = millis() + delay1;
		}
	} else {
		listener_available = false;
		listener.stop();
	}

	delay(5);

}
