#include "main.h"

static volatile bool websocket_streamer_enabled = true;
static volatile bool tcp_streamer_enabled = false;

void
stream_write(const char *buf, size_t len)
{
	if (websocket_streamer_enabled == true) write_websocket_message(buf, len);
	if (tcp_streamer_enabled == true) write_tcp_message(buf, len);
}

void
stream_panic(const char *buf, size_t len)
{
	while (true) {
		stream_write(buf, len);
		TASK_DELAY_MS(2000);
	}
}

void
streamer_websocket_enable(const char *value)
{
	websocket_streamer_enabled = true;
	http_tuner_response("WebSocket streamer enabled!\n");
}

void
streamer_websocket_disable(const char *value)
{
	websocket_streamer_enabled = false;
	http_tuner_response("WebSocket streamer disabled!\n");
}

void
streamer_tcp_enable(const char *value)
{
	tcp_streamer_enabled = true;
	http_tuner_response("TCP streamer enabled!\n");
}

void
streamer_tcp_disable(const char *value)
{
	tcp_streamer_enabled = false;
	http_tuner_response("TCP streamer disabled!\n");
}
