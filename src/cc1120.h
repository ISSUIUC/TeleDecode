// Based on datasheet: https://www.ti.com/lit/ug/swru295e/swru295e.pdf
#ifndef CC1120_H
#define CC1120_H

#include "cc1120_config.h"

#include <stdint.h>
#include <SPI.h>

// TODO: Add state definitions (See table 2)
typedef uint8_t rfStatus_t; // See table 2

class CC1120 {
    public:
        CC1120(SPIClass& spi, uint8_t pin_cs, uint8_t pin_miso, uint8_t pin_gpio2): spi(spi), pin_cs(pin_cs), pin_miso(pin_miso), pin_gpio2(pin_gpio2){}
        void applyConfiguration(const registerSetting_t *regs, int num_regs);
        /* @brief Reads the next available packet. Returns 1 if successful */
        int getNextPacket(uint8_t *packet, uint8_t packet_length);
        int recvPacket(uint8_t *packet, uint8_t packet_length, int32_t timeout);

        rfStatus_t setFrequency(const uint32_t radio_setting);

        rfStatus_t setupRadio();

        rfStatus_t getStatus();

        rfStatus_t setupPacketConfig();
        /* @brief Writes a buffer in the register space */
        rfStatus_t writeRegister(uint16_t address, uint8_t buffer);
        /* @brief Reads a buffer from the register space */
        rfStatus_t readRegister(uint16_t address, uint8_t* buffer);
        /* @brief Sends a command strobe */
        rfStatus_t sendCommandStrobe(uint8_t command);
        /* @brief Gets the current status */

    private:
        SPIClass& spi;
        SPISettings spiSettings = SPISettings(10000000, SPI_MSBFIRST, SPI_MODE0); // copied from E22 Driver, need to verify

        uint8_t pin_cs;
        uint8_t pin_miso;
        uint8_t pin_gpio2;
        bool configured = false;
};

#endif /* CC1120_H */