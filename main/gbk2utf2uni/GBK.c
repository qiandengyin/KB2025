#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"

#define TAG "GBK"
#define OEM2UNI_FILE_PATH "/spiffs/oem2uni.bin"
#define UNI2OEM_FILE_PATH "/spiffs/uni2oem.bin"

unsigned short *oem2uni = NULL;
size_t oem2uni_len = 0;
unsigned short *uni2oem = NULL;
size_t uni2oem_len = 0;

// 从SPIFFS加载数组到PSRAM的函数
void *load_array_from_spiffs(const char *file_path, size_t *size)
{
	void *array = NULL;

	// 获取文件大小
	struct stat st;
	if (stat(file_path, &st) != 0)
	{
		ESP_LOGE(TAG, "Failed to get file status");
		return NULL;
	}

	// 在PSRAM中分配内存
	array = heap_caps_malloc(st.st_size, MALLOC_CAP_SPIRAM);
	if (array == NULL)
	{
		ESP_LOGE(TAG, "Failed to allocate PSRAM memory");
		return NULL;
	}

	// 读取文件
	FILE *f = fopen(file_path, "rb");
	if (f == NULL)
	{
		ESP_LOGE(TAG, "Failed to open file for reading");
		heap_caps_free(array);
		return NULL;
	}

	size_t read = fread(array, 1, st.st_size, f);
	fclose(f);

	if (read != st.st_size)
	{
		ESP_LOGE(TAG, "Failed to read complete data");
		heap_caps_free(array);
		return NULL;
	}

	*size = st.st_size;
	ESP_LOGI(TAG, "Successfully loaded %ld bytes to PSRAM", st.st_size);
	return array;
}

/* Converted code, 0 means conversion error */
/* Character code to be converted */
/* 0: Unicode to OEM code, 1: OEM code to Unicode */
unsigned short ff_convert(unsigned short chr, unsigned int dir)
{
	const unsigned short *p;
	unsigned short c;
	int i, n, li, hi;

	if (oem2uni == NULL)
	{
		oem2uni = load_array_from_spiffs(OEM2UNI_FILE_PATH, &oem2uni_len);
		printf("oem2uni: %d\r\n", oem2uni_len);
		for (int i = 0; i < 10; i++)
		{
			if (oem2uni == NULL)
			{
				break;
			}
			printf("%04X ", oem2uni[i]);
		}
		printf("\r\n");
	}

	if (uni2oem == NULL)
	{
		uni2oem = load_array_from_spiffs(UNI2OEM_FILE_PATH, &uni2oem_len);
		printf("uni2oem: %d\r\n", uni2oem_len);
		for (int i = 0; i < 10; i++)
		{
			if (uni2oem == NULL)
			{
				break;
			}
			printf("%04X ", uni2oem[i]);
		}
		printf("\r\n");
	}

	if (oem2uni == NULL || uni2oem == NULL)
	{
		ESP_LOGI(TAG, "GBK table NULL");
		return 0;
	}

	if (chr < 0x80)
	{
		/* ASCII */
		c = chr;
	}
	else
	{
		if (dir)
		{
			/* OEM code to unicode */
			p = oem2uni;
			hi = oem2uni_len / 4 - 1;
		}
		else
		{
			/* Unicode to OEM code */
			p = uni2oem;
			hi = uni2oem_len / 4 - 1;
		}

		li = 0;
		for (n = 16; n; n--)
		{
			i = li + (hi - li) / 2;
			if (chr == p[i * 2])
				break;
			if (chr > p[i * 2])
				li = i;
			else
				hi = i;
		}
		
		c = n ? p[i * 2 + 1] : 0;
	}

	return c;
}
