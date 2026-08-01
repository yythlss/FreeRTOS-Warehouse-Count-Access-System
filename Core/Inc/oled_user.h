#ifndef OLED_USER_H
#define OLED_USER_H

#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t page, const char *text);
void OLED_ShowString2x(uint8_t x, uint8_t y, const char *text);
void OLED_Refresh(void);

#endif
