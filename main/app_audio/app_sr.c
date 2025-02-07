/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"

#include "bsp_keyboard.h"
#include "app_sr.h"
#include "app_audio.h"
#include "app_wifi.h"
#include "function_keys.h"

static const char *TAG = "app_sr";

#define I2S_CHANNEL_NUM 2

static esp_afe_sr_iface_t *afe_handle = NULL;
static srmodel_list_t *models = NULL;
static bool manul_detect_flag = false;
sr_data_t *g_sr_data = NULL;

static QueueHandle_t g_audio_chat_mode_que = NULL;

static void audio_feed_task(void *arg)
{
    size_t bytes_read = 0;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_channel = afe_handle->get_total_channel_num(afe_data); // 3;
    ESP_LOGI(TAG, "audio_chunksize = %d, feed_channel = %d", audio_chunksize, feed_channel);
    int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * feed_channel, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(audio_buffer);
    g_sr_data->afe_in_buffer = audio_buffer;

    while (true)
    {
        /* Read audio data from I2S bus */
        bsp_i2s_read((char *)audio_buffer, audio_chunksize * I2S_CHANNEL_NUM * sizeof(int16_t), &bytes_read);

        // AFE需要3通道数据, 将第3通道（参考回路）置0
        // 如不需要AEC功能, 只需两通道mic数据即可
        for (int i = audio_chunksize - 1; i >= 0; i--)
        {
            audio_buffer[i * 3 + 0] = audio_buffer[i * 2 + 0]; // mic_l
            audio_buffer[i * 3 + 1] = audio_buffer[i * 2 + 1]; // mic_r
            audio_buffer[i * 3 + 2] = 0;                       // ref
        }

        /* Feed samples of an audio stream to the AFE_SR */
        afe_handle->feed(afe_data, audio_buffer);

        // 保存音频数据
        audio_record_save(audio_buffer, audio_chunksize);

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    afe_handle->destroy(afe_data);
    vTaskDelete(NULL);
}

static void audio_detect_task(void *arg)
{
    static afe_vad_state_t local_state;
    static uint8_t frame_keep = 0;

    bool detect_flag = false;
    esp_afe_sr_data_t *afe_data = arg;

    while (true)
    {
        // 从AFE获取数据
        // fetch 内部可以进行 VAD 处理并检测唤醒词等动作
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            ESP_LOGW(TAG, "AFE Fetch Fail");
            continue;
        }

        // -------------------------------------------------------------------------------
        // 按下按键开始录音
        if (getRecKey())
        {
            if (!manul_detect_flag)
            {
                detect_flag = false;
                frame_keep = 0;
                manul_detect_flag = true;
                g_sr_data->afe_handle->disable_wakenet(afe_data);
                sr_result_t result = {
                    .wakenet_mode = WAKENET_DETECTED,
                    .state = ESP_MN_STATE_DETECTING,
                    .command_id = 0x55,
                };
                app_sr_set_result(&result, 0);
                ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_RED) "manual detect");
            }
            continue;
        }
        else
        {
            if (manul_detect_flag)
            {
                detect_flag = false;
                frame_keep = 0;
                manul_detect_flag = false;
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = ESP_MN_STATE_DETECTED,
                    .command_id = 0x55,// 先随便用一下, 后面再改
                };
                app_sr_set_result(&result, 0);
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_RED) "manual detect end");
                continue;
            }
        }
        // -------------------------------------------------------------------------------

        // 如果检测到唤醒词, 将结果通过消息队列发送给处理任务
        if (res->wakeup_state == WAKENET_DETECTED)
        {
            sr_result_t result = {
                .wakenet_mode = WAKENET_DETECTED,
                .state = ESP_MN_STATE_DETECTING,
                .command_id = 0,
            };
            app_sr_set_result(&result, 0);
            ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_PURPLE) "wakeword detected");
        }
        else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            frame_keep = 0;
            detect_flag = true;                               // 使能VAD检测
            g_sr_data->afe_handle->disable_wakenet(afe_data); // 关闭唤醒词检测
            ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_GREEN) "AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
        }

        if (true == detect_flag)
        {
            if (local_state != res->vad_state)
            {
                local_state = res->vad_state;
                frame_keep = 0;
            }
            else
            {
                frame_keep++;
            }

            // 如果连续100帧没有检测到人声, 则检测结束, 通知处理任务处理结果
            if ((100 == frame_keep) && (AFE_VAD_SILENCE == res->vad_state))
            {
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = ESP_MN_STATE_TIMEOUT,
                    .command_id = 0,
                };
                app_sr_set_result(&result, 0);
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                detect_flag = false;
                ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_PURPLE) "wakeword timeout");
            }
        }
    }
    afe_handle->destroy(afe_data);
    vTaskDelete(NULL);
}

esp_err_t app_sr_set_language(sr_language_t new_lang)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    if (new_lang == g_sr_data->lang)
    {
        ESP_LOGW(TAG, "nothing to do");
        return ESP_OK;
    }
    else
    {
        g_sr_data->lang = new_lang;
    }
    ESP_LOGI(TAG, "Set language %s", SR_LANG_EN == g_sr_data->lang ? "EN" : "CN");
    if (g_sr_data->model_data)
    {
        g_sr_data->multinet->destroy(g_sr_data->model_data);
    }
    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, "");
    ESP_LOGI(TAG, "load wakenet:%s", wn_name);
    g_sr_data->afe_handle->set_wakenet(g_sr_data->afe_data, wn_name);
    return ESP_OK;
}

#define SR_HANDLE_TASK_STACK_SIZE (4 * 1024)
static StaticTask_t xSrHandleTaskBuffer;
static StackType_t *xSrHandleTaskStack;

#define CHAT_TASK_STACK_SIZE (10 * 1024)
static StaticTask_t xChatTaskBuffer;
static StackType_t *xChatTaskStack;

esp_err_t app_sr_start(void)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(NULL == g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR already running");

    g_sr_data = heap_caps_calloc(1, sizeof(sr_data_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_NO_MEM, TAG, "Failed create sr data");

    g_sr_data->result_que = xQueueCreate(3, sizeof(sr_result_t));
    ESP_GOTO_ON_FALSE(NULL != g_sr_data->result_que, ESP_ERR_NO_MEM, err, TAG, "Failed create result queue");

    g_audio_chat_mode_que = xQueueCreate(1, sizeof(audio_chat_mode_t));
    ESP_GOTO_ON_FALSE(NULL != g_audio_chat_mode_que, ESP_ERR_NO_MEM, err, TAG, "Failed create audio chat mode queue");

    BaseType_t ret_val;
    models = esp_srmodel_init("model");
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config.aec_init = false; //aec不使能：BSS/NS 算法处理在 feed() 中进行

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);
    g_sr_data->afe_handle = afe_handle;
    g_sr_data->afe_data   = afe_data;

    g_sr_data->lang = SR_LANG_MAX;
    ret = app_sr_set_language(SR_LANG_CN);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, err, TAG, "Failed to set language");

    // 采集音频数据
    ret_val = xTaskCreatePinnedToCore(&audio_feed_task, "Feed Task", 6 * 1024, (void *)afe_data, 5, &g_sr_data->feed_task, 0);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG, "Failed create audio feed task");
    // 音频检测
    ret_val = xTaskCreatePinnedToCore(&audio_detect_task, "Detect Task", 6 * 1024, (void *)afe_data, 5, &g_sr_data->detect_task, 1);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG, "Failed create audio detect task");
    // 语音处理任务
    xSrHandleTaskStack = (StackType_t *)heap_caps_malloc(SR_HANDLE_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(xSrHandleTaskStack);
    TaskHandle_t xSrHandleTask = xTaskCreateStaticPinnedToCore(&sr_handler_task, "SR Handler Task", SR_HANDLE_TASK_STACK_SIZE, NULL, 4, xSrHandleTaskStack, &xSrHandleTaskBuffer, 1);
    ESP_GOTO_ON_FALSE(xSrHandleTask != NULL, ESP_FAIL, err, TAG, "Failed create audio handler task");
    // 聊天任务
    xChatTaskStack = (StackType_t *)heap_caps_malloc(CHAT_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(xChatTaskStack);
    TaskHandle_t xChatTask = xTaskCreateStaticPinnedToCore(&audio_chat_task, "Audio Chat Task", CHAT_TASK_STACK_SIZE, NULL, 3, xChatTaskStack, &xChatTaskBuffer, 1);
    ESP_GOTO_ON_FALSE(xChatTask != NULL, ESP_FAIL, err, TAG, "Failed create audio chat task");
    // 语音处理任务
    // ret_val = xTaskCreatePinnedToCore(&sr_handler_task, "SR Handler Task", 4 * 1024, NULL, 4, &g_sr_data->handle_task, 0);
    // ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG, "Failed create audio handler task");
    // 聊天任务
    // ret_val = xTaskCreatePinnedToCore(&audio_chat_task, "Audio Chat Task", 10 * 1024, NULL, 3, &g_sr_data->chat_task, 0);
    // ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG, "Failed create audio chat task");

    audio_record_init();

    return ESP_OK;
err:
    app_sr_stop();
    return ret;
}

esp_err_t app_sr_stop(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    if (g_sr_data->result_que)
    {
        vQueueDelete(g_sr_data->result_que);
        g_sr_data->result_que = NULL;
    }

    if (g_sr_data->fp)
    {
        fclose(g_sr_data->fp);
        g_sr_data->fp = NULL;
    }

    if (g_sr_data->model_data)
    {
        g_sr_data->multinet->destroy(g_sr_data->model_data);
    }

    if (g_sr_data->afe_data)
    {
        g_sr_data->afe_handle->destroy(g_sr_data->afe_data);
    }

    if (g_sr_data->afe_in_buffer)
    {
        heap_caps_free(g_sr_data->afe_in_buffer);
    }

    if (g_sr_data->afe_out_buffer)
    {
        heap_caps_free(g_sr_data->afe_out_buffer);
    }

    heap_caps_free(g_sr_data);
    g_sr_data = NULL;
    return ESP_OK;
}

esp_err_t app_sr_get_result(sr_result_t *result, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    xQueueReceive(g_sr_data->result_que, result, xTicksToWait);
    return ESP_OK;
}

esp_err_t app_sr_set_result(sr_result_t *result, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    xQueueSend(g_sr_data->result_que, result, xTicksToWait);
    return ESP_OK;
}

BaseType_t app_sr_get_chat_mode(audio_chat_mode_t *chat_mode, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_audio_chat_mode_que, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    return xQueueReceive(g_audio_chat_mode_que, chat_mode, xTicksToWait);
}

esp_err_t app_sr_set_chat_mode(audio_chat_mode_t *chat_mode, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_audio_chat_mode_que, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    xQueueSend(g_audio_chat_mode_que, chat_mode, xTicksToWait);
    return ESP_OK;
}
