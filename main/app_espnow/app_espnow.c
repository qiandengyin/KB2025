#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "espnow.h"
#include "app_wifi.h"
#include "app_espnow.h"

static const char *TAG = "app_espnow";

void app_espnow_send_data(uint8_t *data, size_t data_len)
{
    if (app_wifi_connected_already() != WIFI_STATUS_CONNECTED_OK)
        return;

    if (data_len > 8)
        return;

    // 按键状态改变, 发送数据
    uint8_t key_changed = 0;
    static uint8_t key_buffer_last[8] = {0};
    uint8_t key_buffer[8] = {0};
    memcpy(key_buffer, data, data_len);
    for (uint8_t i = 0; i < 8; i++)
    {
        if (key_buffer[i] != key_buffer_last[i])
            key_changed++;
    }
    if (key_changed == 0)
        return;
    memcpy(key_buffer_last, key_buffer, sizeof(key_buffer));

    espnow_frame_head_t frame_head = {
        .retransmit_count = 5,
        .broadcast = true,
    };
    app_wifi_lock(0);
    espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, key_buffer_last, sizeof(key_buffer_last), &frame_head, portMAX_DELAY);
    app_wifi_unlock();
}

void app_espnow_init(void)
{
    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);
}
