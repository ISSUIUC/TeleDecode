// Based on datasheet: https://www.ti.com/lit/ug/swru295e/swru295e.pdf
#include <stdint.h>
#include <SPI.h>

// TODO: Add state definitions (See table 2)
typedef uint8_t rfStatus_t; // See table 2

class CC1120 {
    public:
        CC1120(SPIClass& spi, uint8_t pin_cs): spi(spi), pin_cs(pin_cs){}
    private:
        /* @brief Writes a buffer in the register space*/
        rfStatus_t writeRegister(uint8_t address, uint8_t buffer);
        /* @brief Reads a buffer from the register space*/
        rfStatus_t readRegister(uint8_t address, uint8_t* buffer);
        /* @brief Writes a buffer in the extended register space*/
        rfStatus_t writeRegisterExtended(uint8_t address, uint8_t buffer);
        /* @brief Reads a buffer from the extended register space*/
        rfStatus_t readRegisterExtended(uint8_t address, uint8_t* buffer);

        SPIClass& spi;
        SPISettings spiSettings = SPISettings(10000000, SPI_MSBFIRST, SPI_MODE0); // copied from E22 Driver, need to verify

        uint8_t pin_cs;
};

/* SPI Access Bitmasks */ 
#define SINGLE_REGISTER_WRITE 0x00
#define SINGLE_REGISTER_READ 0x80

#define SINGLE_EXTENDED_REGISTER_WRITE 0x2F
#define SINGLE_EXTENDED_REGISTER_READ 0xAF