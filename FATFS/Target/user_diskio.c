/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   SPI TF card diskio driver for FatFs.
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN DECL */
#include <string.h>
#include "ff_gen_drv.h"
#include "sd_spi.h"
/* USER CODE END DECL */

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif

Diskio_drvTypeDef USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if _USE_WRITE
  USER_write,
#endif
#if _USE_IOCTL == 1
  USER_ioctl,
#endif
};

DSTATUS USER_initialize (BYTE pdrv)
{
  if (pdrv != 0) return STA_NOINIT;
  Stat = SD_SPI_Init() ? STA_NOINIT : 0;
  return Stat;
}

DSTATUS USER_status (BYTE pdrv)
{
  if (pdrv != 0) return STA_NOINIT;
  return Stat;
}

DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  if (pdrv != 0 || buff == 0 || count == 0) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  return SD_SPI_ReadBlocks(buff, sector, count) ? RES_ERROR : RES_OK;
}

#if _USE_WRITE == 1
DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  if (pdrv != 0 || buff == 0 || count == 0) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  return SD_SPI_WriteBlocks(buff, sector, count) ? RES_ERROR : RES_OK;
}
#endif

#if _USE_IOCTL == 1
DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
  if (pdrv != 0) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd) {
    case CTRL_SYNC:
      return RES_OK;
    case GET_SECTOR_SIZE:
      *(WORD*)buff = 512;
      return RES_OK;
    case GET_BLOCK_SIZE:
      *(DWORD*)buff = 1;
      return RES_OK;
    case GET_SECTOR_COUNT:
      *(DWORD*)buff = SD_SPI_GetSectorCount();
      return (*(DWORD*)buff) ? RES_OK : RES_ERROR;
    default:
      return RES_PARERR;
  }
}
#endif
