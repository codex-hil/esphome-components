#include "zerotier.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
namespace esphome::zerotier {
static const char *const TAG = "zerotier";
void ZeroTierComponent::setup() {
  this->status_set_warning("Waiting for Ethernet");
  if (this->connected_sensor_) this->connected_sensor_->publish_state(false);
}
void ZeroTierComponent::loop() {
  if (this->stopping_ || millis() - this->last_poll_ < 1000) return;
  this->last_poll_ = millis();
  if (!this->started_) {
    if (!this->ethernet_->is_connected() || this->ethernet_->get_esp_netif() == nullptr) return;
    esp_err_t err = zerotier_native_start(this->network_id_, this->ethernet_->get_esp_netif());
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Cannot start: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
    this->started_ = true;
  }
  zerotier_native_status_t s{};
  zerotier_native_get_status(&s);
  if (s.error != ESP_OK) {
    ESP_LOGE(TAG, "Stopped: %s; identity retained", esp_err_to_name(s.error));
    if (this->connected_sensor_) this->connected_sensor_->publish_state(false);
    this->mark_failed();
    return;
  }
  if (s.node_id != this->last_node_) {
    this->last_node_ = s.node_id;
    char node[11]; snprintf(node, sizeof(node), "%010llx", (unsigned long long) s.node_id);
    ESP_LOGI(TAG, "Node ID: %s", node);
    if (this->node_sensor_) this->node_sensor_->publish_state(node);
  }
  if (s.ipv4 != this->last_ip_) {
    this->last_ip_ = s.ipv4;
    char addr[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &s.ipv4, addr, sizeof(addr));
    ESP_LOGI(TAG, "Overlay address: %s", addr);
    if (this->address_sensor_) this->address_sensor_->publish_state(s.ipv4 ? addr : "");
  }
  bool ready = s.active && this->ethernet_->is_connected();
  if (ready) this->status_clear_warning();
  else this->status_set_warning("Waiting for ZeroTier configuration/link");
  if (ready != this->ready_) {
    this->ready_ = ready;
    if (this->connected_sensor_) this->connected_sensor_->publish_state(ready);
  }
}
void ZeroTierComponent::on_shutdown() { this->stopping_ = true; zerotier_native_stop(); }
bool ZeroTierComponent::teardown() {
  this->on_shutdown();
  zerotier_native_status_t s{}; zerotier_native_get_status(&s);
  return !s.running;
}
void ZeroTierComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Native ZeroTier: network %016llx", (unsigned long long) this->network_id_);
  ESP_LOGCONFIG(TAG, "  Ethernet / system lwIP / IPv4 / NVS; SAR ADC reserved for entropy");
}
}
