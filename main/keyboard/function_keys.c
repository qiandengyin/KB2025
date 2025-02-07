#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp32s3/rom/ets_sys.h"
#include "app_audio.h"
#include "audio_player.h"
#include "rgb_matrix.h"
#include "app_led.h"
#include "bsp_keyboard.h"
#include "function_keys.h"
#include "keyboard.h"
#include "app_espnow.h"
#include "gbk2utf2uni.h"

enum {
    SPECIAL_KEY_CUSTOM_LEFT = 0,
    SPECIAL_KEY_CUSTOM_RIGHT,
    SPECIAL_KEY_UPARROW,
    SPECIAL_KEY_DOWNARROW,
    SPECIAL_KEY_RIGHTARROW,
    SPECIAL_KEY_LEFTARROW,
    SPECIAL_KEY_MAX,
};

/// @brief REC键
/// @param  
/// @return 
uint8_t getRecKey(void)
{
    uint8_t index = KEY_REC_INDEX / 8;
    uint8_t bitIndex = KEY_REC_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief FN键
/// @param  
/// @return 
static uint8_t getFnKey(void)
{
    uint8_t index = KEY_FN_INDEX / 8;
    uint8_t bitIndex = KEY_FN_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

#if 0

/// @brief 自定义键: 左键
/// @param  
/// @return 
static uint8_t getCustomLeftKey(void)
{
    uint8_t index = KEY_CUSTOM_LEFT_INDEX / 8;
    uint8_t bitIndex = KEY_CUSTOM_LEFT_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief 自定义键: 右键
/// @param  
/// @return 
static uint8_t getCustomRightKey(void)
{
    uint8_t index = KEY_CUSTOM_RIGHT_INDEX / 8;
    uint8_t bitIndex = KEY_CUSTOM_RIGHT_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief 箭头键: 上
/// @param  
/// @return 
static uint8_t getUpArrowKey(void)
{
    uint8_t index = KEY_UPARROW_INDEX / 8;
    uint8_t bitIndex = KEY_UPARROW_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief 箭头键: 下
/// @param  
/// @return 
static uint8_t getDownArrowKey(void)
{
    uint8_t index = KEY_DOWNARROW_INDEX / 8;
    uint8_t bitIndex = KEY_DOWNARROW_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief 箭头键: 右
/// @param  
/// @return 
static uint8_t getRightArrowKey(void)
{
    uint8_t index = KEY_RIGHTARROW_INDEX / 8;
    uint8_t bitIndex = KEY_RIGHTARROW_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief 箭头键: 左
/// @param  
/// @return 
static uint8_t getLeftArrowKey(void)
{
    uint8_t index = KEY_LEFTARROW_INDEX / 8;
    uint8_t bitIndex = KEY_LEFTARROW_INDEX % 8;
    return keyboardGetKeyState(index, bitIndex);
}

/// @brief fn + custom left
/// @param  
/// @return 
static uint8_t getFnCustomLeftKey(void)
{
    if (getFnKey() && getCustomLeftKey())
        return 1;
    return 0;
}

/// @brief fn + custom right
/// @param  
/// @return 
static uint8_t getFnCustomRightKey(void)
{
    if (getFnKey() && getCustomRightKey())
        return 1;
    return 0;
}

/// @brief fn + up arrow
/// @param  
/// @return 
static uint8_t getFnUpArrowKey(void)
{
    if (getFnKey() && getUpArrowKey())
        return 1;
    return 0;
}

/// @brief fn + down arrow
/// @param  
/// @return 
static uint8_t getFnDownArrowKey(void)
{
    if (getFnKey() && getDownArrowKey())
        return 1;
    return 0;
}

/// @brief FN + right arrow
/// @param  
/// @return 
static uint8_t getFnRightArrowKey(void)
{
    if (getFnKey() && getRightArrowKey())
        return 1;
    return 0;
}

/// @brief FN + left arrow
/// @param  
/// @return 
static uint8_t getFnLeftArrowKey(void)
{
    if (getFnKey() && getLeftArrowKey())
        return 1;
    return 0;
}

/// @brief 特殊键: 获取按键状态
static uint8_t (*specialKeyFunctions[SPECIAL_KEY_MAX])(void) = {
    getFnCustomLeftKey,
    getFnCustomRightKey,
    getFnUpArrowKey,
    getFnDownArrowKey,
    getFnRightArrowKey,
    getFnLeftArrowKey,
};

#define DEBOUNCE_TIME 50  // 去抖动时间（毫秒）
static uint32_t lastDebounceTime[SPECIAL_KEY_MAX] = {0};
static uint8_t lastButtonState[SPECIAL_KEY_MAX] = {0};
static uint8_t buttonState[SPECIAL_KEY_MAX] = {0};

/// @brief 特殊键
/// @param keyIndex 
static int functionKeysClick(uint8_t keyIndex)
{
    if (keyIndex >= SPECIAL_KEY_MAX)
        return -1;
    uint8_t reading =  specialKeyFunctions[keyIndex]();
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (reading != lastButtonState[keyIndex])
        lastDebounceTime[keyIndex] = currentTime;
    if ((currentTime - lastDebounceTime[keyIndex]) > DEBOUNCE_TIME)
    {
        // 如果按键状态已经稳定足够长的时间，则更新按键状态
        if (reading != buttonState[keyIndex])
        {
            buttonState[keyIndex] = reading;
            if (!buttonState[keyIndex])
                return -1;
            switch (keyIndex)
            {
            case SPECIAL_KEY_CUSTOM_LEFT:
            {
                // uint8_t index = rgb_matrix_get_mode() - 1;
                // if (index < 1)
                //     index = 15;
                // rgb_matrix_mode(index);
                // printf("rgb_matrix_mode - : %d\r\n", index);
                break;
            }
            case SPECIAL_KEY_CUSTOM_RIGHT:
            {
                // uint8_t index = rgb_matrix_get_mode() + 1;
                // if (index > 15)
                //     index = 3;
                // rgb_matrix_mode(index);
                // printf("rgb_matrix_mode + : %d\r\n", index);
                break;
            }
            case SPECIAL_KEY_UPARROW:
                // if (appUartGetHidMode() == MODE_HID_ESPNOW)
                //     app_espnow_bind();
                break;
            case SPECIAL_KEY_DOWNARROW:
                // if (appUartGetHidMode() == MODE_HID_ESPNOW)
                //     app_espnow_unbind();
                break;
            case SPECIAL_KEY_RIGHTARROW:
                // if (appUartGetHidMode() == MODE_HID_ESPNOW)
                //     app_espnow_send_wifi_config();
                break;
            case SPECIAL_KEY_LEFTARROW:
                printf("SPECIAL_KEY_LEFTARROW\r\n");
                break;
            default:
                break;
            }
        }
    }
    lastButtonState[keyIndex] = reading;
    return ((reading == 1) ? keyIndex : -1);
}

/// @brief 检查FN键功能
/// @param  
/// @return 
uint8_t functionKeys(void)
{
    uint8_t click_count = 0;
    for (uint8_t i = 0; i < SPECIAL_KEY_MAX; i++)
    {
        int keyIndex = functionKeysClick(i);
        if (keyIndex >= 0)
            click_count++;
    }
    return click_count;
}

#endif

/***************************************************************************
 * 长按FN键关机
***************************************************************************/
static uint32_t fnPressedTime = 0xffffffff;
static uint8_t  shutdownState = 0;
static uint8_t  bootState     = 0;

void shutdownByFn(void)
{
    bootState++;
    if (bootState < 100)
        return;
    bootState = 100;

    if (getFnKey())
    {
        if (fnPressedTime == 0)
        {
            fnPressedTime = xTaskGetTickCount();
        }
        else if (fnPressedTime != 0xffffffff)
        {
            if (xTaskGetTickCount() - fnPressedTime > 2000)
            {
                bspWs2812Enable(false);
                audio_play_filepath("/spiffs/powerOff.mp3");
                fnPressedTime = 0xffffffff;
                shutdownState = 1;
            }
        }
    }
    else
    {
        fnPressedTime = 0;
    }

    if (shutdownState && !getFnKey() && audio_player_get_state() == AUDIO_PLAYER_STATE_IDLE)
    {
        shutdownState = 0;
        bsp_power_off();
    }
}

/******************************************************************************
 * HID发送GBK字符:
 * GBK（汉字内码扩展规范）是中国国家标准GB2312的扩展，旨在解决汉字编码问题。GBK编码使用双字节编码方案，编码范围从8140到FEFE，共23940个码位，收录了21003个汉字
 * https://www.23bei.com/tool/54.html#
 * https://www.fileformat.info/tip/microsoft/enter_unicode.htm
 * 比如输入“你好”
 * 按住Alt，输入小键盘上的50403，松开Alt，输入“你”字
 * 按住Alt，输入小键盘上的47811，松开Alt，输入“好”字
 * 注意：使用时需要在电脑上通过NumLock打开小键盘
******************************************************************************/
#define MAX_BUFFER_SIZE (1024 * 4)
static unsigned int *gbk_hex_buffer =   NULL;
static int gbk_hex_count = 0;
static int gbk_hex_index = 0;
static uint8_t gbk_hid_buffer[10] = {0};
static int gbk_hid_count = 0;
static int gbk_hid_index = 0;
static uint8_t gbk_hid_state = GBK_TASK_IDLE;

void gbkStrToHex(char *gbk_str, int gbk_str_len)
{
    gbk_hex_index = 0;
    gbk_hex_count = 0;
    
    int tmpGBKSize = MAX_BUFFER_SIZE * sizeof(char);
    char *tmpGBK = heap_caps_malloc((const int)tmpGBKSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(tmpGBK);
    memset(tmpGBK, '\0', MAX_BUFFER_SIZE * sizeof(char));

    int tempUTF8Size = gbk_str_len + 1;
    char *tempUTF8 = heap_caps_malloc((const int)tempUTF8Size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(tempUTF8);
    memset(tempUTF8, '\0', tempUTF8Size);
    sprintf(tempUTF8, "%s", gbk_str);
    int gbk_len = utf82gbk(tmpGBK, tmpGBKSize, tempUTF8);
    free(tempUTF8);
    
    if (gbk_hex_buffer == NULL)
    {
        gbk_hex_buffer = heap_caps_malloc(MAX_BUFFER_SIZE * sizeof(unsigned int), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    assert(gbk_hex_buffer);
    
    memset(gbk_hex_buffer, 0, sizeof(gbk_hex_buffer));
    for (int i = 0; i < gbk_len; i++)
    {
        if (i >= tmpGBKSize)
        {
           break;
        }

        if (gbk_hex_count >= MAX_BUFFER_SIZE)
        {
            break;
        }
        
        int c1 = tmpGBK[i];
        if (c1 > 0x7F)
        {
            // 双字节字符
            if ((i + 1) >= tmpGBKSize)
            {
                break;
            }

            int c2 = tmpGBK[i + 1];
            int gbk_hex = (c1 << 8) | c2;
            gbk_hex_buffer[gbk_hex_count++] = gbk_hex;
            printf("%04X ", gbk_hex);
            i++;
        }
        else
        {
            // 单字节字符（ASCII）
            gbk_hex_buffer[gbk_hex_count++] = c1;
            printf("%02X ", c1);
        }
    }
    printf("\r\n");
    free(tmpGBK);
    gbkHidSetState(GBK_HEX_TO_NUMPAD);
}

// -------------------------------------------------------------------------------------

void gbkHidClearState(void)
{
    gbk_hid_index = 0;
    gbk_hid_count = 0;
    gbk_hex_index = 0;
    gbk_hex_count = 0;
}

uint8_t gbkHidGetState(void)
{
    return gbk_hid_state;
}

void gbkHidSetState(uint8_t state)
{
    gbk_hid_state = state;
}

uint8_t gbkGetKeypad(void)
{
    uint8_t keypad_num = gbk_hid_buffer[gbk_hid_index++];
    if (gbk_hid_index >= gbk_hid_count)
    {
        gbkHidSetState(GBK_ALTKEY_RELEASE);
    }
    return keypad_num;
}

void gbkHexToHidMessage(void)
{
    if (gbk_hex_index >= gbk_hex_count)
    {
        gbkHidSetState(GBK_TASK_IDLE);
        return;
    }

    if (gbk_hex_buffer == NULL)
    {
        gbk_hex_buffer = heap_caps_malloc(MAX_BUFFER_SIZE * sizeof(unsigned int), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    assert(gbk_hex_buffer);

    unsigned int gbk_code = gbk_hex_buffer[gbk_hex_index++];
    if (gbk_code == 0x0000 || gbk_code >= 100000)
    {
        gbkHidSetState(GBK_TASK_IDLE);
        return;
    }

    gbk_hid_count = 0;
    gbk_hid_index = 0;
    memset(gbk_hid_buffer, 0, sizeof(gbk_hid_buffer));
    if (gbk_code >= 10000)
    {
        gbk_hid_buffer[gbk_hid_count++] = (gbk_code / 10000);          // 万
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 10000) / 1000); // 千
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 1000) / 100);   // 百
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 100) / 10);     // 十
        gbk_hid_buffer[gbk_hid_count++] = (gbk_code % 10);             // 个
    }
    else if (gbk_code >= 1000)
    {
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 10000) / 1000); // 千
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 1000) / 100);   // 百
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 100) / 10);     // 十
        gbk_hid_buffer[gbk_hid_count++] = (gbk_code % 10);             // 个
    }
    else if (gbk_code >= 100)
    {
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 1000) / 100);   // 百
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 100) / 10);     // 十
        gbk_hid_buffer[gbk_hid_count++] = (gbk_code % 10);             // 个
    }
    else if (gbk_code >= 10)
    {
        gbk_hid_buffer[gbk_hid_count++] = ((gbk_code % 100) / 10);     // 十
        gbk_hid_buffer[gbk_hid_count++] = (gbk_code % 10);             // 个
    }

    printf("gbk_hid_count: %d\r\n", gbk_hid_count);
    gbkHidSetState(GBK_ALTKEY_PRESSED);
}