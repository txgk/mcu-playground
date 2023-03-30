#ifndef MAIN_H
#define MAIN_H
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

enum {
	OVERVIEW_MENU,
	SAMPLES_MENU,
	MENUS_COUNT,
};

struct i2c_packet {
	int8_t channel_index;
	uint8_t *data;
	size_t data_len;
	uint32_t timestamp_ms;
};

struct i2c_packet_sample {
	size_t packet_index;
	size_t packets_count;
	char *packet_string;
	double period_ms;
	double frequency_hz;
};

struct i2c_node {
	int address;
	size_t packets_count;
	size_t reads_count;
	size_t writes_count;
	struct i2c_packet_sample *samples;
	size_t samples_len;
	size_t samples_lim;
	size_t min_packet_size;
	size_t max_packet_size;
	double avg_packet_size;
	uint32_t first_timestamp_ms;
	double period_ms;
	double frequency_hz;
};

struct i2c_channel {
	char *name; // Terminated with NUL character.
	size_t name_len;
	struct i2c_node *nodes;
	size_t nodes_len;
};

// See "device.c" file for implementation.
void set_log_stream(FILE *stream);
const char *overview_menu_entry_writer(size_t index);
const char *samples_menu_entry_writer(size_t index);
size_t overview_menu_entries_counter(void);
size_t samples_menu_entries_counter(void);
void enter_overview_menu(void);

// See "i2c-packet.c" file for implementation.
bool i2c_packet_parse(struct i2c_packet *dest, const char *src, size_t src_len);
bool i2c_packets_are_equal(const struct i2c_packet *packet1, const struct i2c_packet *packet2);
char *i2c_packet_convert_to_string(const struct i2c_packet *packet);
const char *get_i2c_channel_name(size_t channel_index);

// See "interface.c" file for implementation.
bool curses_init(void);
void curses_free(void);
bool resize_counter_action(void);

// See "interface-list.c" file for implementation.
bool adjust_list_menu(void);
void free_list_menu(void);
void initialize_settings_of_list_menus(void);
void redraw_list_menu_unprotected(void);
const size_t *enter_list_menu(int8_t menu_index);
void reset_list_menu(void);
void leave_list_menu(void);
bool handle_list_menu_navigation(int c);

// See "interface-status.c" file for implementation.
bool status_recreate_unprotected(void);
void status_write(const char *format, ...);
void status_delete(void);

// See "xalloc.c" file for implementation.
void complain_about_insufficient_memory_and_exit(void);
void *xmalloc(size_t size);
void *xrealloc(void *src, size_t size);

extern struct i2c_channel *channels;
extern size_t channels_len;
extern size_t list_menu_height;
extern size_t list_menu_width;
extern pthread_mutex_t interface_lock;
#endif // MAIN_H
