#ifndef __OLED_USER_H
#define __OLED_USER_H

#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t page, const char *s);
void OLED_ShowString2x(uint8_t x, uint8_t y, const char *s);
void OLED_ShowChinese16(uint8_t x, uint8_t y, uint8_t index);
void OLED_ShowChinese8(uint8_t x, uint8_t page, uint8_t index);
void OLED_Refresh(void);
void OLED_ShowChinese12x16(uint8_t x, uint8_t page, uint8_t index);
#endif
