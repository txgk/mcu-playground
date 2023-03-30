#include "main.h"
#include <stdlib.h>
#include <string.h>

static char byte_str[1000];
static size_t byte_len;

static inline int8_t
i2c_get_channel_index(const char *channel_name, size_t channel_name_len)
{
	for (size_t i = 0; i < channels_len; ++i) {
		if ((channel_name_len == channels[i].name_len)
			&& (memcmp(channel_name, channels[i].name, channel_name_len) == 0))
		{
			return i;
		}
	}
	channels = xrealloc(channels, sizeof(struct i2c_channel) * (channels_len + 1));
	channels[channels_len].name = xmalloc(sizeof(char) * (channel_name_len + 1));
	memcpy(channels[channels_len].name, channel_name, channel_name_len);
	channels[channels_len].name[channel_name_len] = '\0';
	channels[channels_len].name_len = channel_name_len;
	channels[channels_len].nodes = NULL;
	channels[channels_len].nodes_len = 0;
	channels_len += 1;
	return channels_len - 1;
}

bool
i2c_packet_parse(struct i2c_packet *dest, const char *src, size_t src_len)
{
	const char *i;
	byte_len = 0;
	uint8_t value = 0;
	dest->channel_index = -1;
	dest->data = NULL;
	dest->data_len = 0;
	dest->timestamp_ms = 0;
	for (i = src; (size_t)(i - src) < src_len; ++i) {
		if (*i == '@') {
			if ((byte_len > 2) && (memcmp(byte_str, "I2C", 3) == 0)) {
				dest->channel_index = i2c_get_channel_index(byte_str, byte_len);
			}
			break;
		} else {
			byte_str[byte_len++] = *i;
		}
	}
	if (dest->channel_index < 0) goto bad_packet;
	byte_len = 0;
	for (i = i + 1; (size_t)(i - src) < src_len; ++i) {
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
	char *str = xmalloc(sizeof(char) * (100 + packet->data_len * 4));
	str[0] = '\0';
	if (packet->data_len == 0) return str;
	int str_len = sprintf(str, "%d%s", packet->data[0] >> 1, packet->data[0] & 1 ? "R" : "W");
	for (size_t i = 1; i < packet->data_len; ++i) {
		str_len += sprintf(str + str_len, " %3" PRIu8, packet->data[i]);
	}
	return str;
}

const char *
get_i2c_channel_name(size_t channel_index)
{
	return channel_index < channels_len ? channels[channel_index].name : "None";
}
