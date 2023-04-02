#include "main.h"

struct uart_packet {
	int8_t channel_index;
	str data;
	uint32_t timestamp_ms;
};

struct uart_channel {
	str name;
};

static struct uart_packet *uart_packets = NULL;
static size_t uart_packets_len = 0;
static size_t uart_packets_lim = 0;

static struct uart_channel *uart_channels = NULL;
static size_t uart_channels_len = 0;

static void
make_sure_there_is_enough_room_for_another_uart_packet(void)
{
	if (uart_packets_len >= uart_packets_lim) {
		uart_packets_lim = 1000 + uart_packets_len * 2;
		uart_packets = xrealloc(uart_packets, sizeof(struct uart_packet) * uart_packets_lim);
	}
}

static inline int8_t
uart_get_channel_index(const char *channel_name, size_t channel_name_len)
{
	for (size_t i = 0; i < uart_channels_len; ++i) {
		if ((channel_name_len == uart_channels[i].name->len)
			&& (memcmp(channel_name, uart_channels[i].name->ptr, channel_name_len) == 0))
		{
			return i;
		}
	}
	uart_channels = xrealloc(uart_channels, sizeof(struct uart_channel) * (uart_channels_len + 1));
	uart_channels[uart_channels_len].name = str_gen(channel_name, channel_name_len);
	uart_channels_len += 1;
	return uart_channels_len - 1;
}

void
uart_packet_parse(const str src)
{
	size_t name_end, time_end;
	make_sure_there_is_enough_room_for_another_uart_packet();
	struct uart_packet *packet = uart_packets + uart_packets_len;
	packet->channel_index = -1;
	for (name_end = 0; name_end < src->len; ++name_end) {
		if (src->ptr[name_end] == '@') {
			packet->channel_index = uart_get_channel_index(src->ptr, name_end);
			break;
		}
	}
	if (packet->channel_index < 0) return;
	packet->timestamp_ms = 0;
	for (time_end = name_end + 1; time_end < src->len; ++time_end) {
		if (src->ptr[time_end] == '=') {
			if (sscanf(src->ptr + name_end + 1, "%" SCNu32 "=", &packet->timestamp_ms) != 1) {
				return;
			}
			break;
		}
	}
	if (packet->timestamp_ms == 0) return;
	packet->data = str_gen(src->ptr + time_end + 1, src->len - time_end - 1);
	uart_packets_len += 1;
}

str
get_uart_packets_string_to_some_point(uint32_t timestamp_ms)
{
	size_t got = 0;
	str list = str_big(5000);
	for (size_t i = uart_packets_len; (i > 0) && (got < list_menu_height); --i) {
		if (uart_packets[i - 1].timestamp_ms <= timestamp_ms) {
			str_cat(list, uart_packets[i - 1].data->ptr, uart_packets[i - 1].data->len);
			str_add(list, '\n');
			got += 1;
		}
	}
	return list;
}
