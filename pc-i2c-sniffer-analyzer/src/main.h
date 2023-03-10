#ifndef MAIN_H
#define MAIN_H
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))

struct i2c_packet {
	bool is_read;
	uint8_t *data;
	size_t data_len;
	struct timespec gettime;
};

// See "device.c" file for implementation.
bool start_serial_device_analysis(const char *device_path, long baud_rate);
void print_analysis_results(size_t selected_channel);
void stop_serial_device_analysis(void);

// See "i2c-packet.c" file for implementation.
struct i2c_packet *i2c_packet_parse(const char *src, size_t src_len);
void i2c_packet_free(struct i2c_packet *packet);

// See "interface.c" file for implementation.
void enter_interface(void);

// See "xmalloc.c" file for implementation.
void *xmalloc(size_t size);
#endif // MAIN_H
