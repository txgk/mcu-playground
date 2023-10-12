#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "main.h"

#define TAG "TCP_STREAMER"

static volatile bool tcp_streamer_is_connected = false;
SemaphoreHandle_t tcp_streamer_lock = NULL;
static char tcp_streamer_data_buf[TCP_STREAMER_MAX_PACKET_SIZE];
static size_t tcp_streamer_data_len = 0;

static void
tcp_streamer(void *dummy)
{
	while (true) {
		tcp_streamer_is_connected = false;
		int reuse_addr = 1;    // toggle
		int keep_alive = 1;    // toggle
		int keep_idle = 5;     // seconds
		int keep_interval = 5; // seconds
		int keep_count = 3;    // tries
		struct sockaddr_storage dest_addr;
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr_ip4->sin_port = htons(TCP_STREAMER_PORT);
		int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (listen_sock < 0) {
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			TASK_DELAY_MS(1000);
			continue;
		}
		setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
		if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
			goto here_we_go_again;
		}
		if (listen(listen_sock, 1) != 0) {
			ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
			goto here_we_go_again;
		}
		while (1) {
			struct sockaddr_storage source_addr;
			socklen_t addr_len = sizeof(source_addr);
			int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			if (sock < 0) {
				ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
				break;
			}
			tcp_streamer_is_connected = true;
			setsockopt(sock, SOL_SOCKET,  SO_KEEPALIVE,  &keep_alive,    sizeof(keep_alive));
			setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,  &keep_idle,     sizeof(keep_idle));
			setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
			setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,   &keep_count,    sizeof(keep_count));
			bool send_failed = false;
			while (send_failed != true) {
				if (tcp_streamer_data_len > 0) {
					int to_write = tcp_streamer_data_len;
					while (to_write > 0) {
						int written = send(sock, tcp_streamer_data_buf + (tcp_streamer_data_len - to_write), to_write, 0);
						if (written < 0) {
							send_failed = true;
							break;
						}
						to_write -= written;
					}
					tcp_streamer_data_len = 0;
				} else {
					TASK_DELAY_MS(50);
				}
			}
			tcp_streamer_is_connected = false;
			shutdown(sock, SHUT_RDWR);
			close(sock);
		}
here_we_go_again:
		close(listen_sock);
		TASK_DELAY_MS(1000);
	}
}

bool
start_tcp_streamer(void)
{
	tcp_streamer_lock = xSemaphoreCreateMutex();
	if (tcp_streamer_lock == NULL) {
		return false;
	}
	xTaskCreatePinnedToCore(&tcp_streamer, "tcp_streamer", 4096, NULL, 1, NULL, 1);
	return true;
}

void
write_tcp_message(const char *buf, size_t len)
{
	if (tcp_streamer_is_connected == true && buf != NULL && len > 0) {
		if (xSemaphoreTake(tcp_streamer_lock, portMAX_DELAY) == pdTRUE) {
			for (uint8_t i = 0; i < 20; ++i) {
				if (tcp_streamer_data_len == 0) {
					const size_t new_len = len > TCP_STREAMER_MAX_PACKET_SIZE ? TCP_STREAMER_MAX_PACKET_SIZE : len;
					memcpy(tcp_streamer_data_buf, buf, new_len);
					tcp_streamer_data_len = new_len;
					break;
				}
				TASK_DELAY_MS(50);
			}
			xSemaphoreGive(tcp_streamer_lock);
		}
	}
}
