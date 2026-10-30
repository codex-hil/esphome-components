#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
 uint64_t node_id;
 uint32_t ipv4;
 bool running, online, active;
 esp_err_t error;
 int network_status;
 uint32_t internal_free, psram_free, stack_free;
 uint32_t wire_tx, wire_rx, rx_frames, tx_frames, rx_drop, tx_drop;
} zerotier_native_status_t;
/* Single instance. Start from the application loop after Ethernet DHCP.
 * stop is asynchronous; status.running becomes false after owner cleanup. */
esp_err_t zerotier_native_start(uint64_t network_id, esp_netif_t *ethernet);
void zerotier_native_stop(void);
void zerotier_native_get_status(zerotier_native_status_t *out);
#ifdef __cplusplus
}
#endif
