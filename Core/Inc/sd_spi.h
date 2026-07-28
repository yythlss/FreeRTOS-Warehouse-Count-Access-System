#ifndef __SD_SPI_H
#define __SD_SPI_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

uint8_t SD_SPI_Init(void);
uint8_t SD_SPI_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count);
uint8_t SD_SPI_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count);
uint32_t SD_SPI_GetSectorCount(void);

#endif
