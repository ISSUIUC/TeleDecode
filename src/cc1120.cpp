#include <SPI.h>
#include <Arduino.h>
#include "cc1120.h"
#include "ao.h"

#include "cc1120_config.h"

void CC1120::applyConfiguration(const registerSetting_t* regs, int num_regs)
{
    for (int i=0; i < num_regs; i++) {
        rfStatus_t status = writeRegister(regs[i].addr, regs[i].data);
        Serial.println(status);
    }
}

int CC1120::getNextPacket(uint8_t *packet, uint8_t packet_length)
{
    // TODO: verify GPIO2 is configured to CRC_OK
    if (!digitalRead(pin_gpio2)) {
        return -1; // no packet available
    }

    uint8_t available_bytes; // number of bytes in packet
    readRegister(CC1200_NUM_RXBYTES, &available_bytes);

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

    // normal register range (upper bits are 0x00)
    if ((address >> 8) == 0x00) {
        uint8_t header = ((uint8_t) address) | SINGLE_REGISTER_WRITE;
        SPI.transfer(header);
    }
    // extended register range (upper bits are 0x2F)
    else if ((address >> 8) == 0x2F){
        SPI.transfer(SINGLE_EXTENDED_REGISTER_WRITE);
        SPI.transfer((uint8_t) address);
    }

    else {
        Serial.println("write register not valid");
        return 0;
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

    rfStatus_t status;

    // normal register range
    if ((address >> 8) == 0x00) {
        uint8_t header = ((uint8_t) address) | SINGLE_REGISTER_READ; 
        status = SPI.transfer(header);
    }
    // extended register range
    else if ((address >> 8) == 0x2F){
        SPI.transfer(SINGLE_EXTENDED_REGISTER_READ);
        status = SPI.transfer((uint8_t) address);
    }
    else {
        Serial.println("read register not valid");
        return 0;
    }

    *buffer = SPI.transfer(0x00);

    digitalWrite(pin_cs, HIGH);
    SPI.endTransaction();

    return status;
}

rfStatus_t CC1120::sendCommandStrobe(uint8_t command)
{
    digitalWrite(pin_cs, LOW);
    SPI.beginTransaction(spiSettings);

    rfStatus_t status = SPI.transfer(command);

    SPI.endTransaction();

    if (command == CC1200_SRES) {
        while (digitalRead(pin_miso)) {
            Serial.println("CC1120 Reset Not Ready");
            sleep(1);
        }
    }

    digitalWrite(pin_cs, HIGH);

    return status;
}

rfStatus_t CC1120::getStatus()
{
    return sendCommandStrobe(CC1200_SNOP);
}

rfStatus_t CC1120::setupRadio() {
    sendCommandStrobe(CC1200_SRES);
    // Then set the registers
    applyConfiguration(cc1200_settings, sizeof(cc1200_settings) / sizeof(cc1200_settings[0]));

    configured = true;
}

rfStatus_t CC1120::setupPacketConfig() {
    // assume 38400 because it is the default for altos
    applyConfiguration(packet_setup, sizeof(packet_setup) / sizeof(packet_setup[0]));
    applyConfiguration(packet_setup_384, sizeof(packet_setup_384) / sizeof(packet_setup_384[0]));
}

rfStatus_t CC1120::setFrequency(const uint32_t radio_setting) {
    writeRegister(CC1200_FREQ2, (uint8_t) (radio_setting >> 16));
    writeRegister(CC1200_FREQ1, (uint8_t) (radio_setting >> 8));
    writeRegister(CC1200_FREQ0, (uint8_t) (radio_setting));
}

int CC1120::recvPacket(uint8_t* packet, uint8_t packet_length, int32_t timeout) {
    uint8_t		len;
	uint8_t		radio_rssi = 0;
	uint8_t		rssi0;
	uint8_t		ret;
    packet_length -= 2;
    uint8_t rx_data[(CC1200_MAX_RECV + 4) * 2 * 8];
    if (packet_length > CC1200_MAX_RECV) {
		return 0;
	}

    len = packet_length + 2;			/* CRC bytes */
	len += (uint8_t) (1 + ~(len & 1)); /* 1 or two pad bytes */
	len *= 2;			/* 1/2 rate convolution */

    writeRegister(CC1200_PKT_LEN, len);
    applyConfiguration(packet_rx_setup, 1);
    writeRegister(CC1200_IOCFG2, CC1200_IOCFG_GPIO_CFG_CLKEN_SOFT);
    sendCommandStrobe(CC1200_SRX);
    // Then we should check for stuff

    while (timeout > 0) {
        if (digitalRead(pin_gpio2) == LOW) {
            // Then we got something
            break;
        }
        timeout -= 1;
        delay(1);
    }
    
    // Check if pin 
    if (getNextPacket(rx_data, len) == 0) {
    //     // Then we succeeded in reading so we decode fec. packet stores decoded data
        int ret = ao_fec_decode(rx_data, len, packet, packet_length + 2, NULL); //should return 1 if it succeeds
        return 1;
    } else {
    //     // Error?
        return 0;
    }
}

uint8_t ao_radio_recv(void *d, uint8_t size, uint16_t timeout)
{
    uint8_t		len;
	uint8_t		radio_rssi = 0;
	uint8_t		rssi0;
	uint8_t		ret;
}
