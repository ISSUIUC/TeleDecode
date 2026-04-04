/* type definitions */

#include <stdint.h>

typedef uint8_t  uint8; 
typedef uint16_t uint16; 
typedef uint32_t uint32;
typedef uint8_t rfStatus_t;


// Radio GPIO pins
#define GPIO_RADIO_INT 17
#define GPIO_RADIO_RESET_N 18

// Radio SPI
#define SPI_RADIO_CS 10
#define SPI_RADIO_MOSI 11
#define SPI_RADIO_SCLK 12
#define SPI_RADIO_MISO 13

#define RADIO_BURST_ACCESS   0x40
#define RADIO_SINGLE_ACCESS  0x00
#define RADIO_READ_ACCESS    0x80
#define RADIO_WRITE_ACCESS   0x00

/* Bit fields in the chip status byte */
#define STATUS_CHIP_RDYn_BM             0x80
#define STATUS_STATE_BM                 0x70
#define STATUS_FIFO_BYTES_AVAILABLE_BM  0x0F

/* functions */
rfStatus_t trx8BitRegAccess(uint8 accessType, uint8 addrByte, uint8 *pData, uint16 len);
rfStatus_t trxSpiCmdStrobe(uint8 cmd);

/* CC112X specific prototype function */
rfStatus_t trx16BitRegAccess(uint8 accessType, uint8 extAddr, uint8 regAddr, uint8 *pData, uint8 len);
