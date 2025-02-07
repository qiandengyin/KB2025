#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "baidu_api.h"
#include "chatgpt_api.h"

static char *TAG = "chatgpt_api";

// chatgpt API配置: https://www.closeai-asia.com/
// const char *url    = "https://api.closeai-proxy.xyz/v1/chat/completions";
// const char *apiKey = "apikey";
// const char *model  = "gpt-3.5-turbo";

// kimi API配置: https://platform.moonshot.cn/docs/api/chat#chat-completion
// const char *url    = "https://api.moonshot.cn/v1/chat/completions";
// const char *apiKey = "apikey";
// const char *model  = "moonshot-v1-8k"; // moonshot-v1-32k, moonshot-v1-128k

// DeepSeek API配置: https://platform.deepseek.com/usage
const char *url    = "https://api.deepseek.com/v1/chat/completions";
const char *apiKey = "Bearer 这里是自己的apikey";
const char *model  = "deepseek-chat";

#define MAX_BUFFER_SIZE (1024 * 8)
static char *response_data = NULL;
static uint32_t file_total_len = 0;

static const char *system_content = "你是一个乐于助人的个人助手,请简短且准确的回答用户问题.";

// http客户端的事件处理回调函数
static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        break;
    case HTTP_EVENT_ON_CONNECTED:
        file_total_len = 0;
        break;
    case HTTP_EVENT_HEADER_SENT:
        break;
    case HTTP_EVENT_ON_HEADER:
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=(%" PRIu32 " + %d) [%d]", file_total_len, evt->data_len, MAX_BUFFER_SIZE);
        if ((file_total_len + evt->data_len) < MAX_BUFFER_SIZE)
        {
            if (evt->user_data)
            {
                memcpy(evt->user_data + file_total_len, evt->data, evt->data_len);
            }
            file_total_len += evt->data_len;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        break;
    case HTTP_EVENT_DISCONNECTED:
        break;
    case HTTP_EVENT_REDIRECT:
        break;
    default:
        break;
    }

    return ESP_OK;
}

char *chatgpt_get_answer(char *request_params)
{
    if (request_params == NULL)
    {
        return NULL;
    }
    
    char *answer = NULL;

    if (response_data == NULL)
    {
        response_data = heap_caps_calloc(1, MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        assert(response_data);
        ESP_LOGI(TAG, "successfully created response_data with a size: %zu", MAX_BUFFER_SIZE);
    }

    // 发送HTTP请求
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_event_handler,
        .user_data = response_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", apiKey);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, request_params, strlen(request_params));
    esp_http_client_set_timeout_ms(client, 5000);
    esp_err_t err = esp_http_client_perform(client); // 执行HTTP请求,并等待响应
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Chat Response Data: %s", response_data);
        cJSON *json = cJSON_Parse(response_data);
        if (json != NULL)
        {
            cJSON *choices_array = cJSON_GetObjectItem(json, "choices");
            if (choices_array != NULL && cJSON_IsArray(choices_array) && cJSON_GetArraySize(choices_array) > 0)
            {
                cJSON *message_obj = cJSON_GetObjectItem(cJSON_GetArrayItem(choices_array, 0), "message");
                if (message_obj != NULL)
                {
                    answer = strdup(cJSON_GetObjectItem(message_obj, "content")->valuestring);
                }
            }
            cJSON_Delete(json);
        }
    }
    else
    {
        answer = "Chat HTTP Post failed";
    }

    free(request_params);
    request_params = NULL;
    esp_http_client_cleanup(client);
    return answer;
}

esp_err_t chatgpt_bot(uint8_t *audio, int audio_len)
{
    // 1.百度语音转文字
    ESP_LOGE(TAG, "start baidu asr");
    char *recognition_result = baidu_get_asr_result(audio, audio_len);
    if (recognition_result == NULL)
    {
        ESP_LOGE(TAG, "0. No text recognized");
        return ESP_FAIL;
    }

    if (strlen(recognition_result) == 0)
    {
        ESP_LOGE(TAG, "1. No text recognized");
        return ESP_FAIL;
    }
    ESP_LOGE(TAG, "++++++++++user input: %s", recognition_result);

    if (strcmp(recognition_result, "invalid_request_error") == 0)
    {
        ESP_LOGE(TAG, "Sorry, I can't understand.");
        return ESP_FAIL;
    }

    // 2.构建请求参数
    cJSON *root = cJSON_CreateObject();
    cJSON *messages_array = cJSON_CreateArray();
    // 添加系统信息
    cJSON *sys_message = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_message, "role", "system");
    cJSON_AddStringToObject(sys_message, "content", system_content);
    cJSON_AddItemToArray(messages_array, sys_message);
    // 添加当前用户的聊天记录
    cJSON *user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", recognition_result);
    cJSON_AddItemToArray(messages_array, user_message);

    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddItemToObject(root, "messages", messages_array);

    cJSON_AddNumberToObject(root, "temperature", 1);
    cJSON_AddNumberToObject(root, "presence_penalty", 0);
    cJSON_AddNumberToObject(root, "frequency_penalty", 0);

    char *request_params = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_LOGE(TAG, "Chat request: %s\n", request_params);

    // 3.获得chatgpt的回答
    ESP_LOGE(TAG, "start chatgpt");
    char *response = chatgpt_get_answer(request_params);
    if (response == NULL)
    {
        ESP_LOGE(TAG, "0. Sorry, I can't understand.");
        return ESP_FAIL;
    }
    ESP_LOGE(TAG, "++++++++++chatgpt response: %s\r\n", response);
    if (strcmp(response, "invalid_request_error") == 0)
    {
        ESP_LOGE(TAG, "1. Sorry, I can't understand.");
        return ESP_FAIL;
    }

    // 4.文字转语音
    ESP_LOGE(TAG, "start baidu tts");
    esp_err_t status = baidu_get_tts_result(response, strlen(response));
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Error post baidu tts: %s", esp_err_to_name(status));
    }

    // 5.释放内存
    if (recognition_result)
    {
        free(recognition_result);
        recognition_result = NULL;
    }
    if (response)
    {
        free(response);
        response = NULL;
    }

    return ESP_OK;
}
