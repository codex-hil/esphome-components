// Diagnostic only: no ADC/DAC initialization and no traffic at boot.
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const int module_pins[] = {CONFIG_HEAD_MOD0, CONFIG_HEAD_MOD1,
                                CONFIG_HEAD_MOD2, CONFIG_HEAD_MOD3};
static const int chip_pins[] = {CONFIG_HEAD_CHIP0, CONFIG_HEAD_CHIP1};
static spi_device_handle_t device;
static bool routed;
static unsigned module_addr, chip_addr;
static uint32_t attempts, errors;

static bool valid_wiring(void) {
#ifndef CONFIG_HEAD_WIRING_VERIFIED
    return false;
#else
    const int pins[] = {CONFIG_HEAD_SCK, CONFIG_HEAD_MOSI, CONFIG_HEAD_MISO,
                       CONFIG_HEAD_CS, CONFIG_HEAD_MOD0, CONFIG_HEAD_MOD1,
                       CONFIG_HEAD_MOD2, CONFIG_HEAD_MOD3, CONFIG_HEAD_CHIP0,
                       CONFIG_HEAD_CHIP1};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (!GPIO_IS_VALID_GPIO(pins[i])) return false;
        if (i != 2 && !GPIO_IS_VALID_OUTPUT_GPIO(pins[i])) return false;
        for (size_t j = 0; j < i; ++j) if (pins[i] == pins[j]) return false;
    }
    return true;
#endif
}

static esp_err_t start_bus(void) {
    if (device) return ESP_OK;
    if (!valid_wiring()) return ESP_ERR_INVALID_STATE;
    // Establish inactive CS before changing address pins.
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_HEAD_CS, 1));
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_HEAD_CS, GPIO_MODE_OUTPUT));
    for (size_t i = 0; i < 4; ++i) {
        ESP_ERROR_CHECK(gpio_set_level(module_pins[i], 0));
        ESP_ERROR_CHECK(gpio_set_direction(module_pins[i], GPIO_MODE_OUTPUT));
    }
    for (size_t i = 0; i < 2; ++i) {
        ESP_ERROR_CHECK(gpio_set_level(chip_pins[i], 0));
        ESP_ERROR_CHECK(gpio_set_direction(chip_pins[i], GPIO_MODE_OUTPUT));
    }
    spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_HEAD_MOSI, .miso_io_num = CONFIG_HEAD_MISO,
        .sclk_io_num = CONFIG_HEAD_SCK, .quadwp_io_num = -1,
        .quadhd_io_num = -1, .max_transfer_sz = 9,
    };
    // No DMA: 2–9-byte frames use bounded stack buffers without DMA alignment copies.
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) return err;
    spi_device_interface_config_t cfg = {
        .clock_speed_hz = CONFIG_HEAD_SPI_HZ, .mode = CONFIG_HEAD_SPI_MODE,
        .spics_io_num = CONFIG_HEAD_CS, .queue_size = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &cfg, &device);
    if (err != ESP_OK) spi_bus_free(SPI2_HOST);
    return err;
}

static esp_err_t route(unsigned module, unsigned chip) {
    if (module > 15 || chip > 3) return ESP_ERR_INVALID_ARG;
    esp_err_t err = start_bus();
    if (err != ESP_OK) return err;
    // Called only from the single task after the previous transfer completed.
    for (size_t i = 0; i < 4; ++i)
        ESP_ERROR_CHECK(gpio_set_level(module_pins[i], (module >> i) & 1));
    for (size_t i = 0; i < 2; ++i)
        ESP_ERROR_CHECK(gpio_set_level(chip_pins[i], (chip >> i) & 1));
    esp_rom_delay_us(10); // External GPIO settling before CS; never between bytes.
    module_addr = module;
    chip_addr = chip;
    routed = true;
    return ESP_OK;
}

static void burst(const uint8_t *tx, size_t length) {
    if (!routed) { puts("ERR route required"); return; }
    if (length < 2 || length > 9) { puts("ERR frame must be 2..9 bytes"); return; }
    uint8_t rx[9] = {0};
    spi_transaction_t t = {
        .length = length * 8, .rxlength = length * 8,
        .tx_buffer = tx, .rx_buffer = rx,
    };
    ++attempts;
    // Exactly ONE IDF hardware transaction: header is tx[0], no command phase.
    esp_err_t err = spi_device_polling_transmit(device, &t);
    if (err != ESP_OK) ++errors;
    printf("FRAME n=%" PRIu32 " module=%u chip=%u mode=%d hz=%d err=%s tx=",
           attempts, module_addr, chip_addr, CONFIG_HEAD_SPI_MODE,
           CONFIG_HEAD_SPI_HZ, esp_err_to_name(err));
    for (size_t i = 0; i < length; ++i) printf("%02X", tx[i]);
    printf(" rx=");
    if (err == ESP_OK) for (size_t i = 0; i < length; ++i) printf("%02X", rx[i]);
    else printf("UNAVAILABLE");
    printf(" errors=%" PRIu32 "\n", errors);
    // RX[0] is logged for diagnostics and must be discarded by device decoding.
}

static bool number(const char *s, unsigned base, unsigned max, unsigned *out) {
    if (!s || !*s || *s == '-') return false;
    errno = 0;
    char *end;
    unsigned long v = strtoul(s, &end, base);
    if (errno || *end || v > max) return false;
    *out = v;
    return true;
}

static void command(char *line) {
    char *save;
    char *cmd = strtok_r(line, " \t", &save);
    if (!cmd) return;
    if (!strcmp(cmd, "route")) {
        char *a = strtok_r(NULL, " \t", &save);
        char *b = strtok_r(NULL, " \t", &save);
        unsigned module, chip;
        if (!number(a, 0, 15, &module) || !number(b, 0, 3, &chip) ||
            strtok_r(NULL, " \t", &save)) { puts("ERR route <module 0..15> <chip 0..3>"); return; }
        printf("ROUTE err=%s\n", esp_err_to_name(route(module, chip)));
    } else if (!strcmp(cmd, "burst")) {
        uint8_t tx[9];
        size_t length = 0;
        char *token;
        while ((token = strtok_r(NULL, " \t", &save))) {
            unsigned value;
            if (length == sizeof(tx) || strlen(token) != 2 ||
                !isxdigit((unsigned char) token[0]) || !isxdigit((unsigned char) token[1]) ||
                !number(token, 16, 255, &value)) { puts("ERR use 2..9 hex bytes"); return; }
            tx[length++] = value;
        }
        burst(tx, length);
    } else if (!strcmp(cmd, "stats")) {
        printf("STATS attempts=%" PRIu32 " errors=%" PRIu32 "\n", attempts, errors);
    } else {
        puts("Commands: route <module> <chip>; burst <HH HH ...>; stats");
    }
}

void app_main(void) {
    puts("MODULIQ raw probe: no boot SPI/GPIO init. CPLD programming is not supported.");
    puts("Commands: route <module> <chip>; burst <HH HH ...>; stats");
    char line[96];
    size_t pos = 0;
    bool overflow = false;
    for (;;) {
        int c = getchar();
        if (c == EOF) { clearerr(stdin); vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (c == '\r' || c == '\n') {
            if (overflow) puts("ERR line too long");
            else { line[pos] = 0; command(line); }
            pos = 0; overflow = false;
        } else if (pos + 1 < sizeof(line) && !overflow) line[pos++] = (char) c;
        else overflow = true;
    }
}
