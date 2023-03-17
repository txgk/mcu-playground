#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char byte_str[1000];
static size_t byte_len;

static inline void
parse_byte_string_into_i2c_packet(struct i2c_packet *dest, const char *src)
{
	uint8_t value = 0;
	sscanf(src, "%" SCNu8, &value);
	dest->data = xrealloc(dest->data, sizeof(uint8_t) * (dest->data_len + 1));
	dest->data[dest->data_len] = value;
	dest->data_len += 1;
}

struct i2c_packet *
i2c_packet_parse(const char *src, size_t src_len)
{
	byte_len = 0;
	struct i2c_packet *packet = xmalloc(sizeof(struct i2c_packet));
	packet->data = NULL;
	packet->data_len = 0;
	clock_gettime(CLOCK_REALTIME, &packet->gettime);
	for (const char *i = src; (size_t)(i - src) < src_len; ++i) {
		if (ISDIGIT(*i)) {
			byte_str[byte_len++] = *i;
		} else if ((*i == '>') || (*i == 'x')) {
			byte_str[byte_len] = '\0';
			parse_byte_string_into_i2c_packet(packet, byte_str);
			byte_len = 0;
		} else if (*i == '=') {
			byte_str[byte_len] = '\0';
			sscanf(byte_str, "%" SCNu32, &packet->timestamp_ms);
			byte_len = 0;
		} else {
			goto bad_packet;
		}
	}
	if (byte_len != 0) {
		byte_str[byte_len] = '\0';
		parse_byte_string_into_i2c_packet(packet, byte_str);
	}
	if (packet->data_len == 0) {
		goto bad_packet;
	}
	packet->is_read = packet->data[0] & 0x1 ? true : false;
	packet->data[0] >>= 1;
	return packet;
bad_packet:
	free(packet->data);
	free(packet);
	return NULL;
}

bool
i2c_packets_are_equal(const struct i2c_packet *packet1, const struct i2c_packet *packet2)
{
	if (packet1->is_read != packet2->is_read) return false;
	if (packet1->data_len != packet2->data_len) return false;
	for (size_t i = 0; i < packet1->data_len; ++i) {
		if (packet1->data[i] != packet2->data[i]) {
			return false;
		}
	}
	return true;
}

char *
i2c_packet_convert_to_string(const struct i2c_packet *packet)
{
	char *str = xmalloc(sizeof(char) * (1000 + packet->data_len * 4));
	char piece[10];
	str[0] = '\0';
	if (packet->data_len == 0) return str;
	sprintf(str, "%d%s", packet->data[0], packet->is_read ? "R" : "W");
	for (size_t i = 1; i < packet->data_len; ++i) {
		sprintf(piece, " %3" PRIu8, packet->data[i]);
		strcat(str, piece);
	}
	return str;
}

void
free_i2c_packet(struct i2c_packet *packet)
{
	if (packet != NULL) {
		free(packet->data);
		free(packet);
	}
}
