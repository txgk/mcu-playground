#include "main.h"
#include <stdlib.h>
#include <string.h>

static char byte_str[1000];
static size_t byte_len;

bool
i2c_packet_parse(struct i2c_packet *dest, const char *src, size_t src_len)
{
	byte_len = 0;
	uint8_t value = 0;
	dest->data = NULL;
	dest->data_len = 0;
	dest->timestamp_ms = 0;
	for (const char *i = src; (size_t)(i - src) < src_len; ++i) {
		if (ISDIGIT(*i)) {
			byte_str[byte_len++] = *i;
		} else if ((*i == '>') || (*i == 'x')) {
			byte_str[byte_len] = '\0';
			sscanf(byte_str, "%" SCNu8, &value);
			dest->data = xrealloc(dest->data, sizeof(uint8_t) * (dest->data_len + 1));
			dest->data[dest->data_len] = value;
			dest->data_len += 1;
			byte_len = 0;
		} else if (*i == '=') {
			byte_str[byte_len] = '\0';
			sscanf(byte_str, "%" SCNu32, &dest->timestamp_ms);
			byte_len = 0;
		} else {
			goto bad_packet;
		}
	}
	if ((dest->data_len == 0) || (dest->timestamp_ms == 0)) {
		goto bad_packet;
	}
	return true;
bad_packet:
	free(dest->data);
	return false;
}

bool
i2c_packets_are_equal(const struct i2c_packet *packet1, const struct i2c_packet *packet2)
{
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
	sprintf(str, "%d%s", packet->data[0] >> 1, packet->data[0] & 1 ? "R" : "W");
	for (size_t i = 1; i < packet->data_len; ++i) {
		sprintf(piece, " %3" PRIu8, packet->data[i]);
		strcat(str, piece);
	}
	return str;
}
