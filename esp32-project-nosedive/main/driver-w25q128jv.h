// Source: https://github.com/nopnop2002/esp-idf-w25q64
// Thanks nopnop2002
#ifndef WINBOND_H
#define WINBOND_H
#include "nosedive.h"

//#define MAX_BLOCKSIZE         128  // Total number of blocks
//#define MAX_SECTORSIZE        2048 // Total number of sectors

#define CMD_WRITE_ENABLE      0x06
#define CMD_WRITE_DISABLE     0x04
#define CMD_READ_STATUS_R1    0x05
#define CMD_READ_STATUS_R2    0x35
#define CMD_WRITE_STATUS_R    0x01 // Unimplemented
#define CMD_PAGE_PROGRAM      0x02
#define CMD_QUAD_PAGE_PROGRAM 0x32 // Unimplemented
#define CMD_BLOCK_ERASE64KB   0xd8
#define CMD_BLOCK_ERASE32KB   0x52
#define CMD_SECTOR_ERASE      0x20
#define CMD_CHIP_ERASE        0xC7
#define CMD_ERASE_SUPPEND     0x75 // Unimplemented
#define CMD_ERASE_RESUME      0x7A // Unimplemented
#define CMD_POWER_DOWN        0xB9
#define CMD_HIGH_PERFORM_MODE 0xA3 // Unimplemented
#define CMD_CNT_READ_MODE_RST 0xFF // Unimplemented
#define CMD_RELEASE_PDOWN_ID  0xAB // Unimplemented
#define CMD_MANUFACURER_ID    0x90
#define CMD_READ_UNIQUE_ID    0x4B
#define CMD_JEDEC_ID          0x9f

#define CMD_READ_DATA         0x03
#define CMD_READ_DATA4B       0x13
#define CMD_FAST_READ         0x0B
#define CMD_FAST_READ4B       0x0C
#define CMD_READ_DUAL_OUTPUT  0x3B // Unimplemented
#define CMD_READ_DUAL_IO      0xBB // Unimplemented
#define CMD_READ_QUAD_OUTPUT  0x6B // Unimplemented
#define CMD_READ_QUAD_IO      0xEB // Unimplemented
#define CMD_WORD_READ         0xE3 // Unimplemented

#define SR1_BUSY_MASK	0x01
#define SR1_WEN_MASK	0x02

bool winbond_initialize(void);
esp_err_t w25q128jv_readStatusReg1(uint8_t * reg1);
esp_err_t w25q128jv_readStatusReg2(uint8_t * reg2);
esp_err_t w25q128jv_read_unique_id(uint8_t *id);
esp_err_t w25q128jv_readManufacturer(uint8_t * id);
bool w25q128jv_IsBusy(void);
esp_err_t w25q128jv_powerDown(void);
esp_err_t w25q128jv_WriteEnable(void);
esp_err_t w25q128jv_WriteDisable(void);
uint16_t w25q128jv_read(uint32_t addr, uint8_t *buf, uint16_t n);
uint16_t w25q128jv_fastread(uint32_t addr, uint8_t *buf, uint16_t n);
bool w25q128jv_eraseSector(uint16_t sect_no, bool flgwait);
bool w25q128jv_erase64Block(uint16_t blk_no, bool flgwait);
bool w25q128jv_erase32Block(uint16_t blk_no, bool flgwait);
bool w25q128jv_eraseAll(bool flgwait);
int16_t w25q128jv_pageWrite(uint32_t addr, uint8_t* buf, int16_t n);
const struct data_piece *get_w25q128jv_info_string(const char *dummy);
#endif // WINBOND_H
