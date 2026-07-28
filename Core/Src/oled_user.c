#include "oled_user.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

#define OLED_ADDR  (0x3C << 1)
static uint8_t gram[128 * 8];

static void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, data, 2, 100);
}

static void OLED_WriteData(uint8_t *data, uint16_t len)
{
    uint8_t buf[17];
    while (len) {
        uint8_t n = (len > 16) ? 16 : len;
        buf[0] = 0x40;
        memcpy(&buf[1], data, n);
        HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, n + 1, 100);
        data += n;
        len -= n;
    }
}

static const uint8_t blank[5] = {0,0,0,0,0};
static const uint8_t glyph_0[5] = {0x3E,0x51,0x49,0x45,0x3E};
static const uint8_t glyph_1[5] = {0x00,0x42,0x7F,0x40,0x00};
static const uint8_t glyph_2[5] = {0x42,0x61,0x51,0x49,0x46};
static const uint8_t glyph_3[5] = {0x21,0x41,0x45,0x4B,0x31};
static const uint8_t glyph_4[5] = {0x18,0x14,0x12,0x7F,0x10};
static const uint8_t glyph_5[5] = {0x27,0x45,0x45,0x45,0x39};
static const uint8_t glyph_6[5] = {0x3C,0x4A,0x49,0x49,0x30};
static const uint8_t glyph_7[5] = {0x01,0x71,0x09,0x05,0x03};
static const uint8_t glyph_8[5] = {0x36,0x49,0x49,0x49,0x36};
static const uint8_t glyph_9[5] = {0x06,0x49,0x49,0x29,0x1E};
static const uint8_t glyph_colon[5] = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t glyph_slash[5] = {0x20,0x10,0x08,0x04,0x02};
static const uint8_t glyph_space[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t glyph_P[5] = {0x7F,0x09,0x09,0x09,0x06};
static const uint8_t glyph_M[5] = {0x7F,0x02,0x0C,0x02,0x7F};
static const uint8_t glyph_T[5] = {0x01,0x01,0x7F,0x01,0x01};
static const uint8_t glyph_a[5] = {0x20,0x54,0x54,0x54,0x78};
static const uint8_t glyph_e[5] = {0x38,0x54,0x54,0x54,0x18};
static const uint8_t glyph_i[5] = {0x00,0x44,0x7D,0x40,0x00};
static const uint8_t glyph_l[5] = {0x00,0x41,0x7F,0x40,0x00};
static const uint8_t glyph_m[5] = {0x7C,0x04,0x18,0x04,0x78};
static const uint8_t glyph_o[5] = {0x38,0x44,0x44,0x44,0x38};
static const uint8_t glyph_p[5] = {0x7C,0x14,0x14,0x14,0x08};
static const uint8_t glyph_x[5] = {0x44,0x28,0x10,0x28,0x44};

static const uint8_t *get_glyph(char c)
{
    switch (c) {
        case '0': return glyph_0; case '1': return glyph_1; case '2': return glyph_2;
        case '3': return glyph_3; case '4': return glyph_4; case '5': return glyph_5;
        case '6': return glyph_6; case '7': return glyph_7; case '8': return glyph_8;
        case '9': return glyph_9; case ':': return glyph_colon; case '/': return glyph_slash;
        case ' ': return glyph_space; case 'P': return glyph_P; case 'M': return glyph_M;
        case 'T': return glyph_T; case 'a': return glyph_a; case 'e': return glyph_e;
        case 'i': return glyph_i; case 'l': return glyph_l; case 'm': return glyph_m;
        case 'o': return glyph_o; case 'p': return glyph_p; case 'x': return glyph_x;
        default: return blank;
    }
}

void OLED_Init(void)
{
    HAL_Delay(100);
    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x02);
    OLED_WriteCmd(0xB0);
    OLED_WriteCmd(0xC8);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x10);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x81); OLED_WriteCmd(0x7F);
    OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF);
    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    memset(gram, 0x00, sizeof(gram));
}

void OLED_ShowString(uint8_t x, uint8_t page, const char *s)
{
    while (*s && page < 8 && x < 122) {
        const uint8_t *g = get_glyph(*s++);
        for (uint8_t i = 0; i < 5; i++) gram[page * 128 + x + i] = g[i];
        gram[page * 128 + x + 5] = 0x00;
        x += 6;
    }
}

void OLED_Refresh(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        OLED_WriteData(&gram[page * 128], 128);
    }
}



static void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64) return;

    if (color)
        gram[(y / 8) * 128 + x] |= (1 << (y % 8));
    else
        gram[(y / 8) * 128 + x] &= ~(1 << (y % 8));
}


/* 12x16 中文字模：人、数
 * 每个字 12列 × 16行
 * 每列 2 个字节：上8点 + 下8点
 */
static const uint8_t Chinese12x16[][24] =
{
    /* 人 */
    {
        0x00,0x00,
        0x00,0x00,
        0x80,0x00,
        0x80,0x01,
        0x40,0x03,
        0x40,0x06,
        0x20,0x0C,
        0x10,0x18,
        0x08,0x30,
        0x04,0x60,
        0x02,0x40,
        0x01,0x80
    },

    /* 数 */
    {
        0x44,0x02,
        0x48,0x02,
        0xFE,0x7F,
        0x10,0x02,
        0x28,0x05,
        0xC6,0x08,
        0x10,0x10,
        0xFE,0x3F,
        0x10,0x04,
        0x28,0x0A,
        0xC6,0x11,
        0x00,0x00
    }
};

void OLED_ShowChinese12x16(uint8_t x, uint8_t page, uint8_t index)
{
    uint8_t i;

    if (index >= 2) return;
    if (x > 116) return;
    if (page > 6) return;

    for (i = 0; i < 12; i++)
    {
        gram[page * 128 + x + i] = Chinese12x16[index][i * 2];
        gram[(page + 1) * 128 + x + i] = Chinese12x16[index][i * 2 + 1];
    }
}





/* 16x16 Chinese bitmap: index 0 = 人, index 1 = 数
 * Each value is one row; bit15 is the leftmost pixel.
 */
static const uint16_t chinese16[][16] = {
    { /* 人 */
        0x0000,0x0080,0x0080,0x0080,
        0x0080,0x0080,0x0180,0x0180,
        0x0240,0x0240,0x0660,0x0420,
        0x0818,0x100C,0x0004,0x0000
    },
    { /* 数 */
        0x0000,0x1810,0x3A60,0x1C60,
        0x3F7C,0x1C40,0x1BB8,0x0808,
        0x0918,0x3F30,0x1230,0x3E30,
        0x0638,0x0CEC,0x0004,0x0000
    }
};

void OLED_ShowChinese16(uint8_t x, uint8_t y, uint8_t index)
{
    if (index >= 2) return;

    for (uint8_t row = 0; row < 16; row++)
    {
        uint16_t data = chinese16[index][row];
        for (uint8_t col = 0; col < 16; col++)
        {
            if (data & (0x8000 >> col))
            {
                OLED_DrawPixel(x + col, y + row, 1);
            }
        }
    }
}

/* 8x8 small Chinese bitmap: index 0 = 人, index 1 = 数
 * Stored the same way as the 5x7 ASCII font: each byte is one vertical column.
 * This makes the Chinese label height close to normal ASCII letters/numbers.
 */
static const uint8_t chinese8[][8] = {
    { /* 人 */
        0x00, 0x40, 0x20, 0x18, 0x18, 0x20, 0x40, 0x00
    },
    { /* 数 - simplified 8x8 */
        0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x44, 0x38, 0x44
    }
};

void OLED_ShowChinese8(uint8_t x, uint8_t page, uint8_t index)
{
    if (index >= 2 || page >= 8 || x >= 121) return;

    for (uint8_t i = 0; i < 8; i++)
    {
        gram[page * 128 + x + i] = chinese8[index][i];
    }
}

static void OLED_ShowChar2x(uint8_t x, uint8_t y, char c)
{
    const uint8_t *g = get_glyph(c);

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t data = g[col];

        for (uint8_t row = 0; row < 7; row++)
        {
            if (data & (1 << row))
            {
                OLED_DrawPixel(x + col * 2,     y + row * 2,     1);
                OLED_DrawPixel(x + col * 2 + 1, y + row * 2,     1);
                OLED_DrawPixel(x + col * 2,     y + row * 2 + 1, 1);
                OLED_DrawPixel(x + col * 2 + 1, y + row * 2 + 1, 1);
            }
        }
    }
}

void OLED_ShowString2x(uint8_t x, uint8_t y, const char *s)
{
    while (*s && x < 118)
    {
        OLED_ShowChar2x(x, y, *s++);
        x += 12;
    }
}


