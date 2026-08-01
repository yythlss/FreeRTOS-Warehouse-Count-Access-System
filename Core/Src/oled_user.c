#include "oled_user.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c2;

#define OLED_ADDRESS_0  (0x3CU << 1)
#define OLED_ADDRESS_1  (0x3DU << 1)
#define OLED_COLUMN_OFFSET  2U

static uint8_t oled_gram[128U * 8U];
static uint8_t oled_ready = 0U;
static uint16_t oled_address = OLED_ADDRESS_0;

static const uint8_t glyph_blank[5] = {0U, 0U, 0U, 0U, 0U};
static const uint8_t glyph_0[5] = {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU};
static const uint8_t glyph_1[5] = {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U};
static const uint8_t glyph_2[5] = {0x42U, 0x61U, 0x51U, 0x49U, 0x46U};
static const uint8_t glyph_3[5] = {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U};
static const uint8_t glyph_4[5] = {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U};
static const uint8_t glyph_5[5] = {0x27U, 0x45U, 0x45U, 0x45U, 0x39U};
static const uint8_t glyph_6[5] = {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U};
static const uint8_t glyph_7[5] = {0x01U, 0x71U, 0x09U, 0x05U, 0x03U};
static const uint8_t glyph_8[5] = {0x36U, 0x49U, 0x49U, 0x49U, 0x36U};
static const uint8_t glyph_9[5] = {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU};
static const uint8_t glyph_colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
static const uint8_t glyph_slash[5] = {0x20U, 0x10U, 0x08U, 0x04U, 0x02U};
static const uint8_t glyph_P[5] = {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U};
static const uint8_t glyph_A[5] = {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU};
static const uint8_t glyph_D[5] = {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU};
static const uint8_t glyph_E[5] = {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U};
static const uint8_t glyph_F[5] = {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U};
static const uint8_t glyph_H[5] = {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU};
static const uint8_t glyph_I[5] = {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U};
static const uint8_t glyph_M[5] = {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU};
static const uint8_t glyph_N[5] = {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU};
static const uint8_t glyph_O[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU};
static const uint8_t glyph_R[5] = {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U};
static const uint8_t glyph_S[5] = {0x46U, 0x49U, 0x49U, 0x49U, 0x31U};
static const uint8_t glyph_T[5] = {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U};
static const uint8_t glyph_U[5] = {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU};
static const uint8_t glyph_W[5] = {0x7FU, 0x20U, 0x18U, 0x20U, 0x7FU};
static const uint8_t glyph_e[5] = {0x38U, 0x54U, 0x54U, 0x54U, 0x18U};
static const uint8_t glyph_l[5] = {0x00U, 0x41U, 0x7FU, 0x40U, 0x00U};
static const uint8_t glyph_o[5] = {0x38U, 0x44U, 0x44U, 0x44U, 0x38U};
static const uint8_t glyph_p[5] = {0x7CU, 0x14U, 0x14U, 0x14U, 0x08U};

static const uint8_t *OLED_GetGlyph(char character)
{
  switch (character)
  {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case ':': return glyph_colon;
    case '/': return glyph_slash;
    case 'P': return glyph_P;
    case 'A': return glyph_A;
    case 'D': return glyph_D;
    case 'E': return glyph_E;
    case 'F': return glyph_F;
    case 'H': return glyph_H;
    case 'I': return glyph_I;
    case 'M': return glyph_M;
    case 'N': return glyph_N;
    case 'O': return glyph_O;
    case 'R': return glyph_R;
    case 'S': return glyph_S;
    case 'T': return glyph_T;
    case 'U': return glyph_U;
    case 'W': return glyph_W;
    case 'e': return glyph_e;
    case 'l': return glyph_l;
    case 'o': return glyph_o;
    case 'p': return glyph_p;
    case ' ': return glyph_blank;
    default: return glyph_blank;
  }
}

static void OLED_WriteCommand(uint8_t command)
{
  uint8_t data[2] = {0x00U, command};

  if (oled_ready != 0U)
  {
    (void)HAL_I2C_Master_Transmit(&hi2c2, oled_address, data, 2U, 20U);
  }
}

static void OLED_WriteData(const uint8_t *data, uint16_t length)
{
  uint8_t packet[17];

  if (oled_ready == 0U)
  {
    return;
  }

  while (length > 0U)
  {
    uint8_t count = (length > 16U) ? 16U : (uint8_t)length;

    packet[0] = 0x40U;
    memcpy(&packet[1], data, count);
    (void)HAL_I2C_Master_Transmit(&hi2c2, oled_address, packet,
                                  (uint16_t)count + 1U, 20U);
    data += count;
    length -= count;
  }
}

static void OLED_DrawPixel(uint8_t x, uint8_t y)
{
  if ((x < 128U) && (y < 64U))
  {
    oled_gram[((uint16_t)(y / 8U) * 128U) + x] |= (uint8_t)(1U << (y % 8U));
  }
}

void OLED_Init(void)
{
  HAL_Delay(100U);

  if (HAL_I2C_IsDeviceReady(&hi2c2, OLED_ADDRESS_0, 2U, 20U) == HAL_OK)
  {
    oled_address = OLED_ADDRESS_0;
  }
  else if (HAL_I2C_IsDeviceReady(&hi2c2, OLED_ADDRESS_1, 2U, 20U) == HAL_OK)
  {
    oled_address = OLED_ADDRESS_1;
  }
  else
  {
    oled_ready = 0U;
    return;
  }

  oled_ready = 1U;
  OLED_WriteCommand(0xAEU);
  OLED_WriteCommand(0x20U);
  OLED_WriteCommand(0x02U);
  OLED_WriteCommand(0xB0U);
  OLED_WriteCommand(0xC8U);
  OLED_WriteCommand((uint8_t)(0x00U | (OLED_COLUMN_OFFSET & 0x0FU)));
  OLED_WriteCommand((uint8_t)(0x10U | (OLED_COLUMN_OFFSET >> 4U)));
  OLED_WriteCommand(0x40U);
  OLED_WriteCommand(0x81U);
  OLED_WriteCommand(0x7FU);
  OLED_WriteCommand(0xA1U);
  OLED_WriteCommand(0xA6U);
  OLED_WriteCommand(0xA8U);
  OLED_WriteCommand(0x3FU);
  OLED_WriteCommand(0xA4U);
  OLED_WriteCommand(0xD3U);
  OLED_WriteCommand(0x00U);
  OLED_WriteCommand(0xD5U);
  OLED_WriteCommand(0x80U);
  OLED_WriteCommand(0xD9U);
  OLED_WriteCommand(0xF1U);
  OLED_WriteCommand(0xDAU);
  OLED_WriteCommand(0x12U);
  OLED_WriteCommand(0xDBU);
  OLED_WriteCommand(0x40U);
  OLED_WriteCommand(0x8DU);
  OLED_WriteCommand(0x14U);
  OLED_WriteCommand(0xAFU);
  OLED_Clear();
  OLED_Refresh();
}

void OLED_Clear(void)
{
  memset(oled_gram, 0, sizeof(oled_gram));
}

void OLED_ShowString(uint8_t x, uint8_t page, const char *text)
{
  while ((*text != '\0') && (page < 8U) && (x <= 122U))
  {
    uint8_t index;
    const uint8_t *glyph = OLED_GetGlyph(*text);

    for (index = 0U; index < 5U; index++)
    {
      oled_gram[((uint16_t)page * 128U) + x + index] = glyph[index];
    }
    oled_gram[((uint16_t)page * 128U) + x + 5U] = 0U;
    x += 6U;
    text++;
  }
}

void OLED_ShowString2x(uint8_t x, uint8_t y, const char *text)
{
  while ((*text != '\0') && (x < 118U))
  {
    uint8_t column;
    const uint8_t *glyph = OLED_GetGlyph(*text);

    for (column = 0U; column < 5U; column++)
    {
      uint8_t row;
      for (row = 0U; row < 7U; row++)
      {
        if ((glyph[column] & (uint8_t)(1U << row)) != 0U)
        {
          OLED_DrawPixel((uint8_t)(x + column * 2U),
                         (uint8_t)(y + row * 2U));
          OLED_DrawPixel((uint8_t)(x + column * 2U + 1U),
                         (uint8_t)(y + row * 2U));
          OLED_DrawPixel((uint8_t)(x + column * 2U),
                         (uint8_t)(y + row * 2U + 1U));
          OLED_DrawPixel((uint8_t)(x + column * 2U + 1U),
                         (uint8_t)(y + row * 2U + 1U));
        }
      }
    }
    x += 12U;
    text++;
  }
}

void OLED_Refresh(void)
{
  uint8_t page;

  if (oled_ready == 0U)
  {
    return;
  }

  for (page = 0U; page < 8U; page++)
  {
    OLED_WriteCommand((uint8_t)(0xB0U + page));
    OLED_WriteCommand((uint8_t)(0x00U | (OLED_COLUMN_OFFSET & 0x0FU)));
    OLED_WriteCommand((uint8_t)(0x10U | (OLED_COLUMN_OFFSET >> 4U)));
    OLED_WriteData(&oled_gram[(uint16_t)page * 128U], 128U);
  }
}
