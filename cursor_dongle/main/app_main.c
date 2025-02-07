#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "led_strip.h"
#include "esp_mac.h"
#include "espnow.h"
#include "iot_button.h"
#include <wifi_provisioning/manager.h>
#include "app_tusb_hid.h"
#include "app_udp_server.h"
#include "settings.h"

static const char *TAG = "app";

#define SSID     "ssid"
#define PASSWORD "password"

#define LED_STRIP_GPIO     GPIO_NUM_48
static led_strip_handle_t g_strip_handle = NULL;

#define WIFI_PROV_KEY_GPIO GPIO_NUM_0
button_handle_t button_handle;

typedef enum
{
    APP_WIFI_PROV_INIT,
    APP_WIFI_PROV_START,
    APP_WIFI_PROV_SUCCESS,
    APP_WIFI_PROV_MAX
} app_wifi_prov_status_t;
static app_wifi_prov_status_t s_wifi_prov_status = APP_WIFI_PROV_INIT;

static uint32_t wifi_disconnected_color = 0;

void app_led_set_color(uint32_t color)
{
    uint8_t red = (color >> 16) & 0xff;
    uint8_t green = (color >> 8) & 0xff;
    uint8_t blue = color & 0xff;
    led_strip_set_pixel(g_strip_handle, 0, red, green, blue);
    led_strip_refresh(g_strip_handle);
}

static void app_led_init(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_strip_handle));
    /* Set all LED off to clear all pixels */
    led_strip_clear(g_strip_handle);
}

/// @brief WiFi连接事件处理函数
/// @param arg 
/// @param event_base 
/// @param event_id 
/// @param event_data 
static void app_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        s_wifi_prov_status = APP_WIFI_PROV_INIT;
        app_led_set_color(wifi_disconnected_color);
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        s_wifi_prov_status = APP_WIFI_PROV_SUCCESS;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi connected");
        app_led_set_color((wifi_disconnected_color * 10));
    }
}

static esp_err_t espnow_recv_data_cb(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    uint8_t *dataBuff = (uint8_t *)data;
    printf("Receive data from " MACSTR ", len: %d, data: ", MAC2STR(src_addr), size);
    for (int i = 0; i < size; i++)
    {
        printf("%02x ", dataBuff[i]);
    }
    printf("\r\n");
    app_tusb_hid_send_key(dataBuff, size);

    return ESP_OK;
}

static void app_wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    app_wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, app_wifi_event_handler, NULL);
}

static void app_wifi_connect(void)
{
    wifi_config_t wifi_config = {0};
    esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Get wifi config failed, %d", ret);
        return;
    }

    if (strlen((const char *)wifi_config.sta.ssid) == 0)
    {
        ESP_LOGW(TAG, "WiFi not configured");
        memcpy(wifi_config.sta.ssid, (char *)SSID, strlen((char *)SSID));
        memcpy(wifi_config.sta.password, (char *)PASSWORD, strlen((char *)PASSWORD));
    }

    ESP_LOGI(TAG, "WiFi SSID: %s", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "WiFi Password: %s", wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    settings_read_parameter_from_nvs();
    sys_param_t *sys_param = settings_get_parameter();

    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = WIFI_PROV_KEY_GPIO,
            .active_level = 0,
        },
    };
    button_handle = iot_button_create(&button_config);
    
    app_led_init();
    int led_blink_count = 0;
    while (1)
    {
        uint8_t hid_mode_key_state = gpio_get_level(WIFI_PROV_KEY_GPIO);
        if (!hid_mode_key_state)
        {
            sys_param->mode_hid = (sys_param->mode_hid + 1) % MODE_HID_MAX;
            settings_write_parameter_to_nvs();
            break;
        }
        app_led_set_color(0x050505);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        app_led_set_color(0x000000);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_blink_count++;
        if (led_blink_count >= 20)
            break;
    }

    app_wifi_init();

    if (sys_param->mode_hid == MODE_HID_ESPNOW)
    {
        ESP_LOGI(TAG, "++++++++++HID mode: ESPNOW");
        wifi_disconnected_color = 0x000300;
        espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
        espnow_init(&espnow_config);
        espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, espnow_recv_data_cb);
    }
    else
    {
        ESP_LOGI(TAG, "++++++++++HID mode: UDP");
        wifi_disconnected_color = 0x000003;
        app_udp_server_start();
    }
    app_led_set_color(wifi_disconnected_color);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    app_tusb_hid_init();
    app_wifi_connect();
}
