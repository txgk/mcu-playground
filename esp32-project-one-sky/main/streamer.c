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
streamer_websocket_enable(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	websocket_streamer_enabled = true;
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "WebSocket streamer enabled!\n");
}

void
streamer_websocket_disable(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	websocket_streamer_enabled = false;
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "WebSocket streamer disabled!\n");
}

void
streamer_tcp_enable(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	tcp_streamer_enabled = true;
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "TCP streamer enabled!\n");
}

void
streamer_tcp_disable(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	tcp_streamer_enabled = false;
	*answer_len_ptr = snprintf(answer_buf_ptr, HTTP_TUNER_ANSWER_SIZE_LIMIT, "TCP streamer disabled!\n");
}
