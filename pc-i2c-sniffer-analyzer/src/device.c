#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <math.h>

#define INITIAL_PACKET_SIZE 5000

struct i2c_packet_sample {
	size_t packet_index;
	size_t packets_count;
	char *packet_string;
	double period_ms;
	double frequency_hz;
};

struct node_data {
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

static FILE *log_stream = NULL;
static pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;

static struct i2c_packet *packets = NULL;
static size_t packets_len = 0;
static size_t packets_lim = 0;
static size_t packets_sel = 0;

static struct node_data *nodes = NULL;
static size_t nodes_len = 0;
static size_t nodes_sel = 0;

static pthread_t device_handler_thread;
static volatile bool device_handler_must_finish = false;
static char entry_content_buffer[5000];
static const struct timespec input_delay = {0, 10000000};
static const struct timespec wait_data_delay = {0, 10000000};

static size_t traverse_speed = 1;

void
set_log_stream(FILE *stream)
{
	log_stream = stream;
}

static inline void
display_history_position(void)
{
	pthread_mutex_lock(&data_lock);
	status_write("Considered packets: %07zu/%07zu  Traverse speed: %zu",
		packets_sel == 0 ? packets_len : packets_sel,
		packets_len,
		traverse_speed
	);
	pthread_mutex_unlock(&data_lock);
}

static inline void
make_sure_there_is_enough_room_for_another_packet(void)
{
	if (packets_len >= packets_lim) {
		packets_lim = 100000 + packets_lim * 2;
		packets = xrealloc(packets, sizeof(struct i2c_packet) * packets_lim);
	}
}

static inline size_t
make_sure_node_for_packet_exists(const struct i2c_packet *packet)
{
	const uint8_t address = packet->data[0] >> 1;
	for (size_t i = 0; i < nodes_len; ++i) {
		if (address == nodes[i].address) {
			return i;
		}
	}
	nodes = xrealloc(nodes, sizeof(struct node_data) * (nodes_len + 1));
	nodes[nodes_len].address = address;
	nodes[nodes_len].packets_count = 0;
	nodes[nodes_len].reads_count = 0;
	nodes[nodes_len].writes_count = 0;
	nodes[nodes_len].samples = NULL;
	nodes[nodes_len].samples_len = 0;
	nodes[nodes_len].samples_lim = 0;
	nodes[nodes_len].min_packet_size = 999999;
	nodes[nodes_len].max_packet_size = 0;
	nodes[nodes_len].avg_packet_size = 0;
	nodes[nodes_len].first_timestamp_ms = packet->timestamp_ms;
	nodes[nodes_len].period_ms = HUGE_VAL;
	nodes[nodes_len].frequency_hz = 0;
	nodes_len += 1;
	return nodes_len - 1;
}

static inline void
take_packet_into_account(size_t packet_index)
{
	size_t node_index = make_sure_node_for_packet_exists(packets + packet_index);
	struct node_data *node = nodes + node_index;
	const struct i2c_packet *packet = &packets[packet_index];

	if ((packet->data_len > 0) && (packet->data[0] & 1)) {
		node->reads_count += 1;
	} else {
		node->writes_count += 1;
	}
	if (packet->data_len < node->min_packet_size) {
		node->min_packet_size = packet->data_len;
	}
	if (packet->data_len > node->max_packet_size) {
		node->max_packet_size = packet->data_len;
	}
	node->avg_packet_size = node->avg_packet_size * node->packets_count + packet->data_len;
	node->packets_count += 1;
	node->avg_packet_size /= node->packets_count;
	node->period_ms = (double)(packet->timestamp_ms - node->first_timestamp_ms) / node->packets_count;
	node->frequency_hz = 1000.0 / node->period_ms;

	for (size_t i = 0; i < node->samples_len; ++i) {
		if (i2c_packets_are_equal(packet, &packets[node->samples[i].packet_index])) {
			struct i2c_packet_sample *sample = node->samples + i;
			sample->packets_count += 1;
			sample->period_ms = (double)(packet->timestamp_ms - packets[sample->packet_index].timestamp_ms) / sample->packets_count;
			sample->frequency_hz = 1000.0 / sample->period_ms;
			return;
		}
	}

	if (node->samples_len >= node->samples_lim) {
		node->samples_lim = 100 + node->samples_len * 2;
		node->samples = xrealloc(node->samples, sizeof(struct i2c_packet_sample) * node->samples_lim);
	}
	node->samples[node->samples_len].packet_index = packet_index;
	node->samples[node->samples_len].packets_count = 1;
	node->samples[node->samples_len].packet_string = i2c_packet_convert_to_string(packet);
	node->samples[node->samples_len].period_ms = HUGE_VAL;
	node->samples[node->samples_len].frequency_hz = 0;
	node->samples_len += 1;
}

static void
try_to_update_menu(void)
{
	static struct timespec current_time;
	static struct timespec last_get_time = {0, 0};
	clock_gettime(CLOCK_REALTIME, &current_time);
	if ((current_time.tv_sec - last_get_time.tv_sec > 0)
		|| (current_time.tv_nsec - last_get_time.tv_nsec > 200000000))
	{
		reset_list_menu();
		display_history_position();
		last_get_time.tv_sec = current_time.tv_sec;
		last_get_time.tv_nsec = current_time.tv_nsec;
	}
}

static void *
device_handler(void *dummy)
{
	(void)dummy;
	char c;
	char *packet = xmalloc(sizeof(char) * INITIAL_PACKET_SIZE);
	size_t packet_len = 0;
	size_t packet_lim = INITIAL_PACKET_SIZE;
	while (device_handler_must_finish == false) {
		c = fgetc(log_stream);
		if (c == '\n') {
			if (packet_len < 4) {
				packet_len = 0;
				continue;
			}
			pthread_mutex_lock(&data_lock);
			make_sure_there_is_enough_room_for_another_packet();
			if (i2c_packet_parse(&packets[packets_len], packet, packet_len) == false) {
				packet_len = 0;
				pthread_mutex_unlock(&data_lock);
				continue;
			}
			packets_len += 1;
			pthread_mutex_unlock(&data_lock);
			if (packets_sel == 0) {
				pthread_mutex_lock(&data_lock);
				take_packet_into_account(packets_len - 1);
				pthread_mutex_unlock(&data_lock);
				try_to_update_menu();
			}
			packet_len = 0;
		} else if (c == EOF) {
			fseek(log_stream, 0, SEEK_CUR);
			nanosleep(&wait_data_delay, NULL);
			try_to_update_menu();
		} else {
			if (packet_len >= packet_lim) {
				packet_lim = packet_lim * 2 + 67;
				packet = xrealloc(packet, sizeof(char) * packet_lim);
			}
			packet[packet_len++] = c;
		}
	}
	free(packet);
	return NULL;
}

static inline void
destroy_nodes(void)
{
	for (size_t i = 0; i < nodes_len; ++i) {
		for (size_t j = 0; j < nodes[i].samples_len; ++j) {
			free(nodes[i].samples[j].packet_string);
		}
		free(nodes[i].samples);
	}
	free(nodes);
	nodes = NULL;
	nodes_len = 0;
	nodes_sel = 0;
}

const char *
overview_menu_entry_writer(size_t index)
{
	if ((index == 0) || (index == 2)) {
		return "+------+--------+--------+--------+--------+--------+--------+-----------+-------+";
	}
	if (index == 1) {
		return "| Addr |  Reads | Writes | MinLen | MaxLen | AvgLen | Unique |     T, ms | f, Hz |";
	}
	index -= 3;
	pthread_mutex_lock(&data_lock);
	if (index < nodes_len) {
		sprintf(entry_content_buffer,
			"| %4d | %6zu | %6zu | %6zu | %6zu | %6.0lf | %6zu | %9.1lf | %5.1lf |",
			nodes[index].address,
			nodes[index].reads_count,
			nodes[index].writes_count,
			nodes[index].min_packet_size,
			nodes[index].max_packet_size,
			nodes[index].avg_packet_size,
			nodes[index].samples_len,
			nodes[index].period_ms,
			nodes[index].frequency_hz
		);
		pthread_mutex_unlock(&data_lock);
		return entry_content_buffer;
	}
	pthread_mutex_unlock(&data_lock);
	return "";
}

const char *
samples_menu_entry_writer(size_t index)
{
	if ((index == 0) || (index == 2)) {
		return "+----------+-----------+-------+---------+";
	}
	if (index == 1) {
		return "| Quantity |     T, ms | f, Hz | Content |";
	}
	index -= 3;
	pthread_mutex_lock(&data_lock);
	if (index < nodes[nodes_sel].samples_len) {
		sprintf(entry_content_buffer,
			"| %8zu | %9.1lf | %5.1lf | %s |",
			nodes[nodes_sel].samples[index].packets_count,
			nodes[nodes_sel].samples[index].period_ms,
			nodes[nodes_sel].samples[index].frequency_hz,
			nodes[nodes_sel].samples[index].packet_string
		);
		pthread_mutex_unlock(&data_lock);
		return entry_content_buffer;
	}
	pthread_mutex_unlock(&data_lock);
	return "";
}

size_t
overview_menu_entries_counter(void)
{
	pthread_mutex_lock(&data_lock);
	const size_t entries_count = nodes_len + 3;
	pthread_mutex_unlock(&data_lock);
	return entries_count;
}

size_t
samples_menu_entries_counter(void)
{
	pthread_mutex_lock(&data_lock);
	const size_t entries_count = nodes[nodes_sel].samples_len + 3;
	pthread_mutex_unlock(&data_lock);
	return entries_count;
}

static void
discard_one_packet(void)
{
	pthread_mutex_lock(&data_lock);
	pthread_mutex_lock(&interface_lock);
	if (packets_sel == 0) {
		packets_sel = packets_len;
	} else {
		destroy_nodes();
		packets_sel = packets_sel > traverse_speed ? packets_sel - traverse_speed : 1;
		for (size_t i = 0; i < packets_sel; ++i) {
			take_packet_into_account(i);
		}
	}
	pthread_mutex_unlock(&data_lock);
	pthread_mutex_unlock(&interface_lock);
	reset_list_menu();
	display_history_position();
}

static void
discard_all_packets(void)
{
	if (packets_len == 0) return;
	pthread_mutex_lock(&data_lock);
	pthread_mutex_lock(&interface_lock);
	destroy_nodes();
	packets_sel = 1;
	take_packet_into_account(0);
	pthread_mutex_unlock(&data_lock);
	pthread_mutex_unlock(&interface_lock);
	reset_list_menu();
	display_history_position();
}

static void
apply_one_packet(void)
{
	if (packets_sel == 0) return;
	pthread_mutex_lock(&data_lock);
	pthread_mutex_lock(&interface_lock);
	for (size_t i = 0; (i < traverse_speed) && (packets_sel < packets_len); ++i) {
		take_packet_into_account(packets_sel);
		packets_sel += 1;
	}
	pthread_mutex_unlock(&data_lock);
	pthread_mutex_unlock(&interface_lock);
	reset_list_menu();
	display_history_position();
}

static void
apply_all_packets(void)
{
	if (packets_sel == 0) return;
	pthread_mutex_lock(&data_lock);
	pthread_mutex_lock(&interface_lock);
	packets_sel = 0;
	destroy_nodes();
	for (size_t i = 0; i < packets_len; ++i) {
		take_packet_into_account(i);
	}
	pthread_mutex_unlock(&data_lock);
	pthread_mutex_unlock(&interface_lock);
	reset_list_menu();
	display_history_position();
}

static void
enter_samples_menu(size_t node_index)
{
	nodes_sel = node_index;
	const size_t *view_sel = enter_list_menu(SAMPLES_MENU);
	while (true) {
		pthread_mutex_lock(&interface_lock);
		int c = getch();
		pthread_mutex_unlock(&interface_lock);
		if (handle_list_menu_navigation(c) == true) {
			// Rest a little.
		} else if ((c == '\n') || (c == KEY_RIGHT)) {
			if (*view_sel > 2) {
				pthread_mutex_lock(&interface_lock);
				clear();
				refresh();
				mvprintw(0, 0, "%s", nodes[nodes_sel].samples[*view_sel - 3].packet_string);
				while (getch() == ERR) {
					nanosleep(&input_delay, NULL);
				}
				redraw_list_menu_unprotected();
				pthread_mutex_unlock(&interface_lock);
			}
		} else if ((c == 'q') || (c == KEY_LEFT)) {
			break;
		} else if (c == KEY_RESIZE) {
			resize_counter_action();
		}
		nanosleep(&input_delay, NULL);
	}
	leave_list_menu();
}

void
enter_overview_menu(void)
{
	const size_t *view_sel = enter_list_menu(OVERVIEW_MENU);
	pthread_create(&device_handler_thread, NULL, &device_handler, NULL);

	while (true) {
		pthread_mutex_lock(&interface_lock);
		int c = getch();
		pthread_mutex_unlock(&interface_lock);
		if (handle_list_menu_navigation(c) == true) {
			// Rest a little.
		} else if ((c == '\n') || (c == KEY_RIGHT)) {
			if (*view_sel > 2) {
				enter_samples_menu(*view_sel - 3);
			}
		} else if (c == ',') {
			discard_one_packet();
		} else if (c == '<') {
			discard_all_packets();
		} else if (c == '.') {
			apply_one_packet();
		} else if (c == '>') {
			apply_all_packets();
		} else if (ISDIGIT(c)) {
			switch (c) {
				case '1': traverse_speed = 1; break;
				case '2': traverse_speed = 5; break;
				case '3': traverse_speed = 10; break;
				case '4': traverse_speed = 50; break;
				case '5': traverse_speed = 100; break;
				case '6': traverse_speed = 500; break;
				case '7': traverse_speed = 1000; break;
				case '8': traverse_speed = 5000; break;
				case '9': traverse_speed = 10000; break;
				case '0': traverse_speed = 50000; break;
			}
			display_history_position();
		} else if (c == 'q') {
			break;
		} else if (c == KEY_RESIZE) {
			resize_counter_action();
		}
		nanosleep(&input_delay, NULL);
	}

	device_handler_must_finish = true;
	pthread_join(device_handler_thread, NULL);
	destroy_nodes();
	for (size_t i = 0; i < packets_len; ++i) {
		free(packets[i].data);
	}
	free(packets);
}
