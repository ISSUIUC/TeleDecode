#include <SPI.h>
#include <Arduino.h>
#include "cc1120.h"

void CC1120::applyConfiguration() {

}

rfStatus_t CC1120::writeRegister(uint8_t address, uint8_t buffer){
    uint8_t header = (address & 0x3f) | SINGLE_REGISTER_WRITE;

    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(header);
    SPI.transfer(buffer);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::readRegister(uint8_t address, uint8_t* buffer){
    uint8_t header = (address & 0x3f) | SINGLE_REGISTER_READ; 

    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(header);
    *buffer = SPI.transfer(0x00);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::writeRegisterExtended(uint8_t address, uint8_t buffer)
{
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(SINGLE_EXTENDED_REGISTER_WRITE);
    SPI.transfer(address);
    SPI.transfer(buffer);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::readRegisterExtended(uint8_t address, uint8_t *buffer)
{
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(SINGLE_EXTENDED_REGISTER_WRITE);
    SPI.transfer(address);
    *buffer = SPI.transfer(0x00);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

int CC1120::FIFOBytesAvailable()
{
    uint8_t buffer;
    readRegisterExtended(CC112X_NUM_RXBYTES, &buffer);
    return buffer;
}
