// MPL-2.0. Exclusive SAR-ADC entropy source; schema rejects ADC components.
#include "esp_random.h"
#include "bootloader_random.h"
#include <stdlib.h>
#include <stdatomic.h>
static atomic_bool ready;
void ztp_rng_init(void) {
 bootloader_random_enable();
 atomic_store(&ready,1);
}
void ztp_rng_deinit(void) {
 atomic_store(&ready,0);
 bootloader_random_disable();
}
void ztp_secure_random(void *data,unsigned int bytes) {
 if(!atomic_load(&ready))abort();
 esp_fill_random(data,bytes);
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
void ztp_crypto_yield(void) {
 if(esp_task_wdt_status(NULL)==ESP_OK)esp_task_wdt_reset();
 vTaskDelay(1);
}
