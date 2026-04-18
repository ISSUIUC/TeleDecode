#include <SPI.h>
#include <Arduino.h>
#include "cc1120.h"

#include "cc1120_config.h"
void CC1120::applyConfiguration() {

}

int CC1120::getNextPacket(uint8_t *packet, uint8_t packet_length)
{
    // TODO: verify GPIO2 is configured to CRC_OK
    if (!digitalRead(pin_gpio2)) {
        return 0; // no packet available
    }

    uint8_t available_bytes; // number of bytes in packet
    readRegisterExtended(CC112X_NUM_RXBYTES, &available_bytes);

    if (available_bytes < packet_length) {
        return 0; 
    }

    // RX FIFO burst access
    uint8_t header = FIFO_BURST_ACCESS | SINGLE_REGISTER_READ;
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(header);

    for (int i=0; i<packet_length; i++) {
        packet[i] = SPI.transfer(0x00);
    }

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return 1;
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

rfStatus_t CC1120::sendCommandStrobe(uint8_t command)
{
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    rfStatus_t status = SPI.transfer(command);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::getStatus()
{
    return sendCommandStrobe(CC112X_CMD_SNOP);
}

rfStatus_t CC1120::setupRadio() {
    sendCommandStrobe(CC112X_CMD_SRES);
    // Then set the registers
    configured = true;
}
