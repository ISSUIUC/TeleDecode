#include <SPI.h>
#include <Arduino.h>
#include "cc1120.h"

#include "cc1120_config.h"

void CC1120::applyConfiguration(const registerSetting_t *regs, int num_regs)
{
    for (int i=0; i < num_regs; i++) {
        writeRegister(cc1120_settings[i].addr, cc1120_settings[i].data);
    }
}

int CC1120::getNextPacket(uint8_t *packet, uint8_t packet_length)
{
    // TODO: verify GPIO2 is configured to CRC_OK
    if (!digitalRead(pin_gpio2)) {
        return -1; // no packet available
    }

    uint8_t available_bytes; // number of bytes in packet
    readRegister(CC112X_NUM_RXBYTES, &available_bytes);

    if (available_bytes < packet_length) {
        return -1; 
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

    return 0;
}

rfStatus_t CC1120::writeRegister(uint16_t address, uint8_t buffer)
{
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    // normal register range
    if (address & 0x3f == address) {
        uint8_t header = ((uint8_t) address) | SINGLE_REGISTER_WRITE;
        SPI.transfer(header);
    }
    // extended register range
    else {
        SPI.transfer(SINGLE_EXTENDED_REGISTER_WRITE);
        SPI.transfer((uint8_t) address);
    }

    rfStatus_t status = SPI.transfer(buffer);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::readRegister(uint16_t address, uint8_t *buffer)
{
    SPI.beginTransaction(spiSettings);
    digitalWrite(pin_cs, LOW);

    // normal register range
    if (address & 0x3f == address) {
        uint8_t header = ((uint8_t) address) | SINGLE_REGISTER_READ; 
        SPI.transfer(header);
    }
    // extended register range
    else {
        SPI.transfer(SINGLE_EXTENDED_REGISTER_READ);
        SPI.transfer((uint8_t) address);
    }

    rfStatus_t status = *buffer = SPI.transfer(0x00);

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
    applyConfiguration(cc1120_settings, sizeof(cc1120_settings) / sizeof(cc1120_settings[0]));

    configured = true;
}

rfStatus_t CC1120::setupPacketConfig() {
    // assume 38400 because it is the default for altos
    applyConfiguration(packet_setup, sizeof(packet_setup) / sizeof(packet_setup[0]));
    applyConfiguration(packet_setup_384, sizeof(packet_setup_384) / sizeof(packet_setup_384[0]));
}

rfStatus_t CC1120::setFrequency(const uint32_t radio_setting) {
    writeRegister(CC112X_FREQ2, (uint8_t) (radio_setting >> 16));
    writeRegister(CC112X_FREQ1, (uint8_t) (radio_setting >> 8));
    writeRegister(CC112X_FREQ0, (uint8_t) (radio_setting));
}

uint8_t ao_radio_recv(void *d, uint8_t size, uint16_t timeout)
{
    uint8_t		len;
	uint8_t		radio_rssi = 0;
	uint8_t		rssi0;
	uint8_t		ret;
}