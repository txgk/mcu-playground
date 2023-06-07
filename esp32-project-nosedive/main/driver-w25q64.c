// Source: https://github.com/nopnop2002/esp-idf-w25q64
// Thanks nopnop2002
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver-w25q64.h"

//#define ADDRESS_MODE_4BYTE 1

static spi_device_handle_t winbond;

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
W25Q64_readStatusReg1(uint8_t *reg1)
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
W25Q64_readStatusReg2(uint8_t * reg2)
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
W25Q64_readUniqieID(uint8_t *id)
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
esp_err_t W25Q64_readManufacturer(uint8_t * id)
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
W25Q64_IsBusy(void)
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
W25Q64_powerDown(void)
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
esp_err_t W25Q64_WriteEnable(void)
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
esp_err_t W25Q64_WriteDisable(void)
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
uint16_t W25Q64_read(uint32_t addr, uint8_t *buf, uint16_t n)
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
uint16_t W25Q64_fastread(uint32_t addr, uint8_t *buf, uint16_t n)
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
bool W25Q64_eraseSector(uint16_t sect_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = sect_no;
	addr<<=12;

	// Write permission setting
	esp_err_t ret;
	ret = W25Q64_WriteEnable();
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
	while( W25Q64_IsBusy() & flgwait) {
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
bool W25Q64_erase64Block(uint16_t blk_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = blk_no;
	addr<<=16;

	// Write permission setting
	esp_err_t ret;
	ret = W25Q64_WriteEnable();
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
	while( W25Q64_IsBusy() & flgwait) {
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
bool W25Q64_erase32Block(uint16_t blk_no, bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[4];
	uint32_t addr = blk_no;
	addr<<=15;

	// Write permission setting
	esp_err_t ret;
	ret = W25Q64_WriteEnable();
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
	while( W25Q64_IsBusy() & flgwait) {
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
bool W25Q64_eraseAll(bool flgwait)
{
	spi_transaction_t SPITransaction;
	uint8_t data[1];

	// Write permission setting
	esp_err_t ret;
	ret = W25Q64_WriteEnable();
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
	while( W25Q64_IsBusy() & flgwait) {
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
int16_t W25Q64_pageWrite(uint16_t sect_no, uint16_t inaddr, uint8_t* buf, int16_t n)
{
	if (n > 256) return 0;
	spi_transaction_t SPITransaction;
	uint8_t *data;

	uint32_t addr = sect_no;
	addr<<=12;
	addr += inaddr;

	// Write permission setting
	esp_err_t ret;
	ret = W25Q64_WriteEnable();
	if (ret != ESP_OK) return 0;

	// Busy check
	if (W25Q64_IsBusy()) return 0;

	data = (unsigned char*)malloc(n+4);
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
	while( W25Q64_IsBusy() ) {
		vTaskDelay(1);
	}
	return n;
}

