#include "sd_spi.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

#define SD_DUMMY_BYTE      0xFF
#define SD_TOKEN_START     0xFE
#define SD_TOKEN_MULTI_WR  0xFC
#define SD_TOKEN_STOP_TRAN 0xFD

#define CMD0   (0x40+0)
#define CMD1   (0x40+1)
#define CMD8   (0x40+8)
#define CMD9   (0x40+9)
#define CMD10  (0x40+10)
#define CMD12  (0x40+12)
#define CMD16  (0x40+16)
#define CMD17  (0x40+17)
#define CMD18  (0x40+18)
#define CMD24  (0x40+24)
#define CMD25  (0x40+25)
#define CMD55  (0x40+55)
#define CMD58  (0x40+58)
#define ACMD41 (0x40+41)
#define ACMD23 (0x40+23)

#define CT_MMC  0x01
#define CT_SD1  0x02
#define CT_SD2  0x04
#define CT_SDC  (CT_SD1|CT_SD2)
#define CT_BLOCK 0x08

static uint8_t CardType = 0;

static void SD_CS_LOW(void)  { HAL_GPIO_WritePin(TF_CS_GPIO_Port, TF_CS_Pin, GPIO_PIN_RESET); }
static void SD_CS_HIGH(void) { HAL_GPIO_WritePin(TF_CS_GPIO_Port, TF_CS_Pin, GPIO_PIN_SET); }

static uint8_t spi_rw(uint8_t data)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, 100);
    return rx;
}

static void spi_clocks(uint16_t n)
{
    while (n--) spi_rw(0xFF);
}

static void spi_set_slow(void)
{
    HAL_SPI_DeInit(&hspi1);
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    HAL_SPI_Init(&hspi1);
}

static void spi_set_fast(void)
{
    HAL_SPI_DeInit(&hspi1);
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    HAL_SPI_Init(&hspi1);
}

static uint8_t wait_ready(uint32_t timeout_ms)
{
    uint32_t t = HAL_GetTick();
    uint8_t d;
    do {
        d = spi_rw(0xFF);
        if (d == 0xFF) return 1;
    } while ((HAL_GetTick() - t) < timeout_ms);
    return 0;
}

static void deselect(void)
{
    SD_CS_HIGH();
    spi_rw(0xFF);
}

static uint8_t select_card(void)
{
    SD_CS_LOW();
    spi_rw(0xFF);
    if (wait_ready(500)) return 1;
    deselect();
    return 0;
}

static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t n, res;

    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }

    if (cmd != CMD12) {
        deselect();
        if (!select_card()) return 0xFF;
    }

    spi_rw(cmd);
    spi_rw((uint8_t)(arg >> 24));
    spi_rw((uint8_t)(arg >> 16));
    spi_rw((uint8_t)(arg >> 8));
    spi_rw((uint8_t)arg);

    n = 0x01;
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    spi_rw(n);

    if (cmd == CMD12) spi_rw(0xFF);

    n = 10;
    do {
        res = spi_rw(0xFF);
    } while ((res & 0x80) && --n);

    return res;
}

static uint8_t read_datablock(uint8_t *buff, uint16_t btr)
{
    uint8_t token;
    uint32_t t = HAL_GetTick();

    do {
        token = spi_rw(0xFF);
        if (token == SD_TOKEN_START) break;
    } while ((HAL_GetTick() - t) < 200);

    if (token != SD_TOKEN_START) return 0;

    while (btr--) *buff++ = spi_rw(0xFF);
    spi_rw(0xFF);  /* CRC */
    spi_rw(0xFF);
    return 1;
}

static uint8_t write_datablock(const uint8_t *buff, uint8_t token)
{
    uint16_t i;
    uint8_t resp;

    if (!wait_ready(500)) return 0;

    spi_rw(token);
    if (token != SD_TOKEN_STOP_TRAN) {
        for (i = 0; i < 512; i++) spi_rw(buff[i]);
        spi_rw(0xFF);
        spi_rw(0xFF);
        resp = spi_rw(0xFF);
        if ((resp & 0x1F) != 0x05) return 0;
    }
    return 1;
}

uint8_t SD_SPI_Init(void)
{
    uint8_t n, type, ocr[4];
    uint32_t t;

    CardType = 0;
    SD_CS_HIGH();
    spi_set_slow();
    spi_clocks(10);

    type = 0;
    if (send_cmd(CMD0, 0) == 1) {
        t = HAL_GetTick();
        if (send_cmd(CMD8, 0x1AA) == 1) {
            for (n = 0; n < 4; n++) ocr[n] = spi_rw(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                while ((HAL_GetTick() - t) < 3000 && send_cmd(ACMD41 | 0x80, 1UL << 30));
                if ((HAL_GetTick() - t) < 3000 && send_cmd(CMD58, 0) == 0) {
                    for (n = 0; n < 4; n++) ocr[n] = spi_rw(0xFF);
                    type = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
                }
            }
        } else {
            if (send_cmd(ACMD41 | 0x80, 0) <= 1) {
                type = CT_SD1;
                t = HAL_GetTick();
                while ((HAL_GetTick() - t) < 3000 && send_cmd(ACMD41 | 0x80, 0));
            } else {
                type = CT_MMC;
                t = HAL_GetTick();
                while ((HAL_GetTick() - t) < 3000 && send_cmd(CMD1, 0));
            }
            if (!type || send_cmd(CMD16, 512) != 0) type = 0;
        }
    }

    CardType = type;
    deselect();

    if (type) {
        spi_set_fast();
        return 0;
    }
    return 1;
}

uint8_t SD_SPI_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count)
{
    if (!count || !CardType) return 1;
    if (!(CardType & CT_BLOCK)) sector *= 512;

    if (count == 1) {
        if (send_cmd(CMD17, sector) == 0 && read_datablock(buff, 512)) count = 0;
    } else {
        if (send_cmd(CMD18, sector) == 0) {
            do {
                if (!read_datablock(buff, 512)) break;
                buff += 512;
            } while (--count);
            send_cmd(CMD12, 0);
        }
    }
    deselect();
    return count ? 1 : 0;
}

uint8_t SD_SPI_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count)
{
    if (!count || !CardType) return 1;
    if (!(CardType & CT_BLOCK)) sector *= 512;

    if (count == 1) {
        if (send_cmd(CMD24, sector) == 0 && write_datablock(buff, SD_TOKEN_START)) count = 0;
    } else {
        if (CardType & CT_SDC) send_cmd(ACMD23 | 0x80, count);
        if (send_cmd(CMD25, sector) == 0) {
            do {
                if (!write_datablock(buff, SD_TOKEN_MULTI_WR)) break;
                buff += 512;
            } while (--count);
            if (!write_datablock(0, SD_TOKEN_STOP_TRAN)) count = 1;
        }
    }
    deselect();
    return count ? 1 : 0;
}

uint32_t SD_SPI_GetSectorCount(void)
{
    uint8_t csd[16];
    uint32_t csize;

    if (send_cmd(CMD9, 0) != 0 || !read_datablock(csd, 16)) {
        deselect();
        return 0;
    }
    deselect();

    if ((csd[0] >> 6) == 1) { /* SDHC/SDXC */
        csize = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        return (csize + 1) << 10;
    } else { /* SDSC/MMC */
        uint8_t n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = ((uint32_t)(csd[8] >> 6) | ((uint32_t)csd[7] << 2) | ((uint32_t)(csd[6] & 3) << 10)) + 1;
        return csize << (n - 9);
    }
}
