#pragma once
#include "../../../cpld_stubs/esphome/core/log.h"
#define LOG_SENSOR(...) ::esphome::test_log(__VA_ARGS__)
#define LOG_UPDATE_INTERVAL(...) ::esphome::test_log(__VA_ARGS__)
#define YESNO(x) ((x)?"YES":"NO")
