#pragma once
namespace esphome { template<typename... T> void test_log(T...) {} }
#define ESP_LOGD(...) ::esphome::test_log(__VA_ARGS__)
#define ESP_LOGV(...) ::esphome::test_log(__VA_ARGS__)
#define ESP_LOGVV(...) ::esphome::test_log(__VA_ARGS__)
#define ESP_LOGW(...) ::esphome::test_log(__VA_ARGS__)
#define ESP_LOGE(...) ::esphome::test_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) ::esphome::test_log(__VA_ARGS__)
#define LOG_PIN(...) ::esphome::test_log(__VA_ARGS__)

#define ESP_LOGI(...) do {} while (0)
