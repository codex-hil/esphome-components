#pragma once
// Laboratory-only read-only services. Explicitly opt in through the HIL YAML.
// No GPIO, instrument control, configuration writes or firmware endpoints.
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esphome/core/log.h"
#include "zerotier_native.h"

namespace zt_load_fixture {
inline bool send_all(int fd, const char *data, size_t length) {
  while (length) {
    int n = send(fd, data, length, 0);
    if (n <= 0) return false;
    data += n;
    length -= n;
  }
  return true;
}

inline void serve(void *argument) {
  bool http = argument != nullptr;
  int port = http ? 8080 : 5025;
  int listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener < 0) { vTaskDelete(nullptr); return; }
  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(listener, 4)) {
    ESP_LOGE("zt_load", "Cannot listen on %d", port);
    close(listener); vTaskDelete(nullptr); return;
  }
  ESP_LOGI("zt_load", "Read-only test service listening on %d", port);
  for (;;) {
    int fd = accept(listener, nullptr, nullptr);
    if (fd < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    timeval timeout{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char request[512]{};
    size_t used = 0;
    bool complete = false;
    // Bound both buffer use and total receive time, including slow senders.
    int64_t deadline = esp_timer_get_time() + 2000000;
    while (used < sizeof(request) - 1 && esp_timer_get_time() < deadline) {
      int n = recv(fd, request + used, sizeof(request) - 1 - used, 0);
      if (n <= 0) break;
      used += n; request[used] = 0;
      if (http ? strstr(request, "\r\n\r\n") != nullptr : strchr(request, '\n') != nullptr) {
        complete = true; break;
      }
    }
    if (complete && http) {
      bool valid = strncmp(request, "GET /status HTTP/1.", 19) == 0;
      char body[384];
      zerotier_native_status_t status{};
      zerotier_native_get_status(&status);
      int length = valid ? snprintf(body, sizeof(body),
        "{\"fixture\":\"zt-load-v1\",\"uptime_ms\":%llu,\"internal_free\":%u,"
        "\"psram_free\":%u,\"zt_stack_free\":%u,\"rx_drop\":%u,\"tx_drop\":%u}\n",
        static_cast<unsigned long long>(esp_timer_get_time() / 1000),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(status.stack_free), static_cast<unsigned>(status.rx_drop),
        static_cast<unsigned>(status.tx_drop)) : snprintf(body, sizeof(body), "{\"error\":\"not found\"}\n");
      char header[192];
      int hn = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        valid ? "200 OK" : "404 Not Found", length);
      if (length > 0 && length < static_cast<int>(sizeof(body)) && hn > 0 && hn < static_cast<int>(sizeof(header))) {
        if (send_all(fd, header, hn)) send_all(fd, body, length);
      }
    } else if (complete) {
      char *end = strchr(request, '\n');
      *end = 0;
      if (end > request && end[-1] == '\r') end[-1] = 0;
      const char *reply = strcmp(request, "*IDN?") == 0 ? "LAB,ESP32-P4-ZT,LOAD,1\n" :
          strcmp(request, "SYST:ERR?") == 0 ? "0,\"No error\"\n" : "-113,\"Undefined header\"\n";
      send_all(fd, reply, strlen(reply));
    }
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
}

inline void start() {
  if (xTaskCreate(serve, "zt_http_test", 8192, reinterpret_cast<void *>(1), 2, nullptr) != pdPASS)
    ESP_LOGE("zt_load", "HTTP task allocation failed");
  if (xTaskCreate(serve, "zt_scpi_test", 8192, nullptr, 2, nullptr) != pdPASS)
    ESP_LOGE("zt_load", "SCPI task allocation failed");
}
}  // namespace zt_load_fixture
