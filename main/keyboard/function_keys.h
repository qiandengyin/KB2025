#ifndef FUNCTION_KEYS_H
#define FUNCTION_KEYS_H

uint8_t getRecKey(void);
uint8_t functionKeys(void);
void shutdownByFn(void);
void gbkStrToHex(char *gbk_str, int gbk_str_len);
void gbkBufferClear(void);
void gbkHexToHidMessage(void);
uint8_t gbkHidGetState(void);
void gbkHidSetState(uint8_t state);
uint8_t gbkGetKeypad(void);
void gbkHidClearState(void);

enum
{
    GBK_TASK_IDLE = 0,
    GBK_HEX_TO_NUMPAD,
    GBK_NUMPAD_PRESSED,
    GBK_NUMPAD_RELEASE,
    GBK_ALTKEY_PRESSED,
    GBK_ALTKEY_RELEASE,
};

#endif