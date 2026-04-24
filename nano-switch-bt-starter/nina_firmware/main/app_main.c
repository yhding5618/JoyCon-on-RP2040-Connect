#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "nvs_flash.h"

#include "bridge_uart.h"
#include "switch_hid.h"

void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  if (switch_hid_init() != ESP_OK || bridge_uart_init() != ESP_OK) {
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  while (1) {
    switch_hid_tick();
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}
