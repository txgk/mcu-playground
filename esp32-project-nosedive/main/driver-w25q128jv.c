// Source: https://github.com/nopnop2002/esp-idf-w25q64
// Thanks nopnop2002
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver-w25q128jv.h"

//#define ADDRESS_MODE_4BYTE 1

//
//
//
//                                 TODO
//
//                 SECURE ALL THESE CALLS TO W25Q128JV
//                       WITH MUTEXES AND BE HAPPY
//                            ALL YOUR LIFE
//                                 <3
//
//
//

static spi_device_handle_t winbond = NULL;

#define WINBOND_INFO_BUF_SIZE 500
static char winbond_info_buf[WINBOND_INFO_BUF_SIZE];
static struct data_piece winbond_info;

bool
winbond_initialize(void)
{
	spi_device_interface_config_t winbond_cfg = {
		.mode = 0,
		.spics_io_num = WINBOND_CS_PIN,
		.clock_speed_hz = 1000000,
		.queue_size = 7,
	};
	if (spi_bus_add_device(WINBOND_SPI_HOST, &winbond_cfg, &winbond) == ESP_OK) {
		return true;
	}
	return false;
}

esp_err_t
w25q128jv_readStatusReg1(uint8_t *reg1)
{
	spi_transaction_t SPITransaction;
	uint8_t data[2];
	data[0] = CMD_READ_STATUS_R1;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 2 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit(winbond, &SPITransaction);
	assert(ret==ESP_OK);
	*reg1 = data[1];
	return ret;
}

esp_err_t
w25q128jv_readStatusReg2(uint8_t * reg2)
{
	spi_transaction_t SPITransaction;
	uint8_t data[2];
	data[0] = CMD_READ_STATUS_R2;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 2 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	*reg2 = data[1];
	return ret;
}

esp_err_t
w25q128jv_read_unique_id(uint8_t *id)
{
	spi_transaction_t SPITransaction;
	uint8_t data[13];
	data[0] = CMD_READ_UNIQUE_ID;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 13 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	memcpy(id, &data[5], 8);
	return ret ;
}

//
// Get JEDEC ID(Manufacture, Memory Type,Capacity)
// d(out):Stores 3 bytes of Manufacture, Memory Type, Capacity
//
esp_err_t w25q128jv_readManufacturer(uint8_t * id)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	data[0] = CMD_JEDEC_ID;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 4 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	memcpy(id, &data[1], 3);
	return ret ;
}

bool
w25q128jv_IsBusy(void)
{
	spi_transaction_t SPITransaction;
	uint8_t data[2];
	data[0] = CMD_READ_STATUS_R1;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 2 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return false;
	if( (data[1] & SR1_BUSY_MASK) != 0) return true;
	return false;
}

esp_err_t
w25q128jv_powerDown(void)
{
	spi_transaction_t SPITransaction;
	uint8_t data[1];
	data[0] = CMD_POWER_DOWN;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 1 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	return ret;
}

//
// Write permission setting
//
esp_err_t w25q128jv_WriteEnable(void)
{
	spi_transaction_t SPITransaction;
	uint8_t data[1];
	data[0] = CMD_WRITE_ENABLE;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 1 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	return ret;
}


//
// Write-protected setting
//
esp_err_t w25q128jv_WriteDisable(void)
{
	spi_transaction_t SPITransaction;
	uint8_t data[1];
	data[0] = CMD_WRITE_DISABLE;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 1 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	return ret;
}

//
// Read data
// addr(in):Read start address
//          3 Bytes Address Mode : 24 Bits 0x000000 - 0xFFFFFF
//          4 Bytes Address Mode : 32 Bits 0x00000000 - 0xFFFFFFFF
// n(in):Number of read data
//
uint16_t w25q128jv_read(uint32_t addr, uint8_t *buf, uint16_t n)
{ 
	spi_transaction_t SPITransaction;
	uint8_t *data;
	data = (uint8_t *)malloc(n+5);
	size_t offset;
#ifdef ADDRESS_MODE_4BYTE
		data[0] = CMD_READ_DATA4B;
		data[1] = (addr>>24) & 0xFF; // A31-A24
		data[2] = (addr>>16) & 0xFF; // A23-A16
		data[3] = (addr>>8) & 0xFF; // A15-A08
		data[4] = addr & 0xFF; // A07-A00
		offset = 5;
#else
		data[0] = CMD_READ_DATA;
		data[1] = (addr>>16) & 0xFF; // A23-A16
		data[2] = (addr>>8) & 0xFF; // A15-A08
		data[3] = addr & 0xFF; // A07-A00
		offset = 4;
#endif
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = (n+offset) * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	memcpy(buf, &data[offset], n);
	free(data);
	if (ret != ESP_OK) return 0;
	return n;
}

//
// Fast read data
// addr(in):Read start address
//          3 Bytes Address Mode : 24 Bits 0x000000 - 0xFFFFFF
//          4 Bytes Address Mode : 32 Bits 0x00000000 - 0xFFFFFFFF
// n(in):Number of read data
//
uint16_t w25q128jv_fastread(uint32_t addr, uint8_t *buf, uint16_t n)
{
	spi_transaction_t SPITransaction;
	uint8_t *data;
	data = (uint8_t *)malloc(n+6);
	size_t offset;
#ifdef ADDRESS_MODE_4BYTE
		data[0] = CMD_FAST_READ4B;
		data[1] = (addr>>24) & 0xFF; // A31-A24
		data[2] = (addr>>16) & 0xFF; // A23-A16
		data[3] = (addr>>8) & 0xFF; // A15-A08
		data[4] = addr & 0xFF; // A07-A00
		data[5] = 0; // Dummy
		offset = 6;
#else
		data[0] = CMD_FAST_READ;
		data[1] = (addr>>16) & 0xFF; // A23-A16
		data[2] = (addr>>8) & 0xFF; // A15-A08
		data[3] = addr & 0xFF; // A07-A00
		data[4] = 0; // Dummy
		offset = 5;
#endif
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = (n+offset) * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	esp_err_t ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	memcpy(buf, &data[offset], n);
	free(data);
	if (ret != ESP_OK) return 0;
	return n;
}

//
// Erasing data in 4kb space units
// sect_no(in):Sector number(0 - 2048)
// flgwait(in):true:Wait for complete / false:No wait for complete
// Return value: true:success false:fail
//
// Note:
// The data sheet states that erasing usually takes 30ms and up to 400ms.
// The upper 11 bits of the 23 bits of the address correspond to the sector number.
// The lower 12 bits are the intra-sectoral address.
//
// 補足:
// データシートでは消去に通常 30ms 、最大400msかかると記載されている
// アドレス23ビットのうち上位 11ビットがセクタ番号の相当する。
// 下位12ビットはセクタ内アドレスとなる。
//
bool w25q128jv_eraseSector(uint16_t sect_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = sect_no;
	addr<<=12;

	// Write permission setting
	esp_err_t ret;
	ret = w25q128jv_WriteEnable();
	if (ret != ESP_OK) return false;

	data[0] = CMD_SECTOR_ERASE;
	data[1] = (addr>>16) & 0xff;
	data[2] = (addr>>8) & 0xff;
	data[3] = addr & 0xff;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 4 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return false;

	// Busy check
	while( w25q128jv_IsBusy() & flgwait) {
		vTaskDelay(1);
	}
	return true;
}

//
// Erasing data in 64kb space units
// blk_no(in):Block number(0 - 127)
// flgwait(in):true:Wait for complete / false:No wait for complete
// Return value: true:success false:fail
//
// Note:
// The data sheet states that erasing usually takes 150ms and up to 1000ms.
// The upper 7 bits of the 23 bits of the address correspond to the block.
// The lower 16 bits are the address in the block.
//
// 補足:
// データシートでは消去に通常 150ms 、最大1000msかかると記載されている
// アドレス23ビットのうち上位 7ビットがブロックの相当する。下位16ビットはブロック内アドレスとなる。
//
bool w25q128jv_erase64Block(uint16_t blk_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = blk_no;
	addr<<=16;

	// Write permission setting
	esp_err_t ret;
	ret = w25q128jv_WriteEnable();
	if (ret != ESP_OK) return false;

	data[0] = CMD_BLOCK_ERASE64KB;
	data[1] = (addr>>16) & 0xff;
	data[2] = (addr>>8) & 0xff;
	data[3] = addr & 0xff;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 4 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return false;

	// Busy check
	while( w25q128jv_IsBusy() & flgwait) {
		vTaskDelay(1);
	}
	return true;
}

//
// Erasing data in 32kb space units
// blk_no(in):Block number(0 - 255)
// flgwait(in):true:Wait for complete / false:No wait for complete
// Return value: true:success false:fail
//
// Note:
// The data sheet states that erasing usually takes 120ms and up to 800ms.
// The upper 8 bits of the 23 bits of the address correspond to the block.
// The lower 15 bits are the in-block address.
//
// 補足:
// データシートでは消去に通常 120ms 、最大800msかかると記載されている
// アドレス23ビットのうち上位 8ビットがブロックの相当する。下位15ビットはブロック内アドレスとなる。
//
bool w25q128jv_erase32Block(uint16_t blk_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = blk_no;
	addr<<=15;

	// Write permission setting
	esp_err_t ret;
	ret = w25q128jv_WriteEnable();
	if (ret != ESP_OK) return false;

	data[0] = CMD_BLOCK_ERASE32KB;
	data[1] = (addr>>16) & 0xff;
	data[2] = (addr>>8) & 0xff;
	data[3] = addr & 0xff;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 4 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return false;

	// Busy check
	while( w25q128jv_IsBusy() & flgwait) {
		vTaskDelay(1);
	}
	return true;
}

//
// Erase all data
// flgwait(in):true:Wait for complete / false:No wait for complete
// Return value: true:success false:fail
//
// Note:
// The data sheet states that erasing usually takes 15s and up to 30s.
//
// 補足:
// データシートでは消去に通常 15s 、最大30sかかると記載されている
//
bool w25q128jv_eraseAll(bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[1];

	// Write permission setting
	esp_err_t ret;
	ret = w25q128jv_WriteEnable();
	if (ret != ESP_OK) return false;

	data[0] = CMD_CHIP_ERASE;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 1 * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	ret = spi_device_transmit( winbond, &SPITransaction );
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return false;

	// Busy check
	while( w25q128jv_IsBusy() & flgwait) {
		vTaskDelay(1);
	}
	return true;
}

//
// Page write
// sect_no(in):Sector number(0x00 - 0x7FF) 
// inaddr(in):In-sector address(0x00-0xFFF)
// data(in):Write data
// n(in):Number of bytes to write(0～256)
//
int16_t
w25q128jv_pageWrite(uint32_t addr, uint8_t* buf, int16_t n)
{
	if (n > 256) return 0;
	spi_transaction_t SPITransaction;
	uint8_t *data;

	// Write permission setting
	esp_err_t ret;
	ret = w25q128jv_WriteEnable();
	if (ret != ESP_OK) return 0;

	// Busy check
	if (w25q128jv_IsBusy()) return 0;

	data = malloc(n+4);
	data[0] = CMD_PAGE_PROGRAM;
	data[1] = (addr>>16) & 0xff;
	data[2] = (addr>>8) & 0xff;
	data[3] = addr & 0xFF;
	memcpy( &data[4], buf, n );
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = (n+4) * 8;
	SPITransaction.tx_buffer = data;
	SPITransaction.rx_buffer = data;
	ret = spi_device_transmit( winbond, &SPITransaction );
	free(data);
	assert(ret==ESP_OK);
	if (ret != ESP_OK) return 0;

	// Busy check
	while( w25q128jv_IsBusy() ) {
		vTaskDelay(1);
	}
	return n;
}

static bool
w25q128jv_check_condition(void)
{
	int64_t data_value = esp_timer_get_time();
	uint8_t data[8] = {
		data_value         & 0xFF,
		(data_value >> 8)  & 0xFF,
		(data_value >> 16) & 0xFF,
		(data_value >> 24) & 0xFF,
		(data_value >> 32) & 0xFF,
		(data_value >> 40) & 0xFF,
		(data_value >> 48) & 0xFF,
		(data_value >> 56) & 0xFF,
	};
	w25q128jv_eraseSector(0, true);
	w25q128jv_pageWrite(0, data, sizeof(data));
	uint8_t check[8] = {0};
	w25q128jv_fastread(0, check, sizeof(data));
	for (int i = 0; i < sizeof(data); ++i) {
		if (data[i] != check[i]) {
			return false;
		}
	}
	return true;
}

const struct data_piece *
get_w25q128jv_info_string(const char *dummy)
{
	if (winbond == NULL) {
		if (winbond_initialize() == false) {
			return NULL;
		}
	}
	uint8_t manufacturer[3] = {0};
	uint8_t uid[8] = {0};
	w25q128jv_readManufacturer(manufacturer);
	w25q128jv_read_unique_id(uid);
	winbond_info.len = snprintf(
		winbond_info_buf,
		WINBOND_INFO_BUF_SIZE,
		"\n"
		"Winbond W25Q128JV\n"
		"Manufacturer: %02X %02X %02X\n"
		"Unique ID: %02X %02X %02X %02X %02X %02X %02X %02X\n"
		"Состояние: %s\n",
		manufacturer[0], manufacturer[1], manufacturer[2],
		uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7],
		w25q128jv_check_condition() ? "исправен" : "не исправен"
	);
	winbond_info.ptr = winbond_info_buf;
	return winbond_info.len > 0 && winbond_info.len < WINBOND_INFO_BUF_SIZE ? &winbond_info : NULL;
}
