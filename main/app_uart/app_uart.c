#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "rgb_matrix.h"
#include "bsp_keyboard.h"
#include "app_led.h"
#include "app_uart.h"

static const int RX_BUF_SIZE = 512;
#define TXD_PIN (GPIO_NUM_14)
#define RXD_PIN (GPIO_NUM_21)

static uint8_t *recv_data_buff = NULL;

/************************************************************************
 * HID模式
************************************************************************/

/// @brief 设置HID模式
/// @param cmd 
void appUartSetHidMode(uint8_t cmd)
{
    sys_param_t *param = settings_get_parameter();
    switch (cmd)
    {
    case 0x11:
        param->mode_hid = MODE_HID_USB;
        break;
    case 0x12:
        param->mode_hid = MODE_HID_BLE;
        break;
    case 0x13:
        param->mode_hid = MODE_HID_ESPNOW;
        break;
    case 0x14:
        param->mode_hid = MODE_HID_UDP;
        break;
    default:
        break;
    }
    settings_write_parameter_to_nvs();
    vTaskDelay(pdMS_TO_TICKS(500));
    bspWs2812Enable(false);
    bsp_power_off();
}

/************************************************************************
 * uart接收任务
************************************************************************/

static void app_uart_rx_task(void *arg)
{
    while (1)
    {
        memset(recv_data_buff, '\0', RX_BUF_SIZE);
        int rxBytes = uart_read_bytes(UART_NUM_1, recv_data_buff, RX_BUF_SIZE, pdMS_TO_TICKS(10));
        if (rxBytes <= 0)
            continue;
        printf("UART RX TASK: ");
        printf("%02X %02X %02X %02X %02X %02X %02X %02X. %d\r\n", recv_data_buff[0], recv_data_buff[1], recv_data_buff[2], recv_data_buff[3], recv_data_buff[4], recv_data_buff[5], recv_data_buff[6], recv_data_buff[7], rxBytes);
        if (recv_data_buff[0] != 0xAA && recv_data_buff[1] != 0x55 && recv_data_buff[6] != 0x55 && recv_data_buff[7] != 0xAA)
            continue;
        if (recv_data_buff[2] >= 0x11 && recv_data_buff[2] <= 0x14)
        {
            appUartSetHidMode(recv_data_buff[2]);
        }
        else if (recv_data_buff[2] == 0x21)
        {
            if (rgb_matrix_get_mode() != 1)
                rgb_matrix_mode(1);
            rgb_matrix_sethsv(recv_data_buff[3], recv_data_buff[4], recv_data_buff[5]);
        }
        else if (recv_data_buff[2] == 0x22)
        {
            // CONFIG_ENABLE_RGB_MATRIX_BREATHING
            if (rgb_matrix_get_mode() != 2)
                rgb_matrix_mode(2);
        }
        else if (recv_data_buff[2] == 0x23)
        {
            // CONFIG_ENABLE_RGB_MATRIX_CYCLE_OUT_IN
            if (rgb_matrix_get_mode() != 3)
                rgb_matrix_mode(3);
        }
        else if (recv_data_buff[2] == 0x31)
        {
            if (recv_data_buff[3] == 0x01)
            {
                bsp_audio_mute_enable(false);
            }
            else
            {
                bsp_audio_mute_enable(true);
            }
        }
    }
    free(recv_data_buff);
    vTaskDelete(NULL);
}

void app_uart_init(void)
{
    recv_data_buff = heap_caps_malloc(RX_BUF_SIZE + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(recv_data_buff);

    const uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);

    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << RXD_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TXD_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(app_uart_rx_task, "uart_rx_task", 3 * 1024, NULL, 3, NULL);
}
