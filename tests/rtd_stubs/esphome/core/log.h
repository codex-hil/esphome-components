#pragma once
namespace esphome { template<typename... T> void test_log(T... ){} }
#define LOG_SENSOR(...) esphome::test_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) esphome::test_log(__VA_ARGS__)
