
#include "esp_log.h"
#include "esp_check.h"
#include "bsp_keyboard.h"

esp_err_t bsp_keyboard_init(void)
{
    esp_err_t ret = ESP_OK;
    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_audio_init());
    bsp_74hc165d_init();
    return ret;
}
