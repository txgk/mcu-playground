#include "main.h"
#include <stdio.h>
#include <stdlib.h>

static void
parse_byte_string_into_i2c_packet(struct i2c_packet *dest, const char *src)
{
	uint8_t value = 0;
	sscanf(src, "%" SCNu8, &value);
	dest->data = realloc(dest->data, sizeof(uint8_t) * (dest->data_len + 1));
	dest->data[dest->data_len] = value;
	dest->data_len += 1;
}

struct i2c_packet *
i2c_packet_parse(const char *src, size_t src_len)
{
	char byte_str[100]; size_t byte_len = 0;
	struct i2c_packet *packet = xmalloc(sizeof(struct i2c_packet));
	packet->is_read = src[0] == 'r' ? true : false;
	packet->data = NULL;
	packet->data_len = 0;
	clock_gettime(CLOCK_REALTIME, &packet->gettime);
	for (const char *i = src + 1; (size_t)(i - src) < src_len; ++i) {
		if (ISDIGIT(*i)) {
			byte_str[byte_len++] = *i;
		} else if (*i == ':') {
			byte_str[byte_len] = '\0';
			parse_byte_string_into_i2c_packet(packet, byte_str);
			byte_len = 0;
		} else if (*i == '$') {
			break;
		}
	}
	if (byte_len != 0) {
		byte_str[byte_len] = '\0';
		parse_byte_string_into_i2c_packet(packet, byte_str);
	}
	return packet;
}

void
i2c_packet_free(struct i2c_packet *packet)
{
	if (packet != NULL) {
		free(packet->data);
		free(packet);
	}
}
