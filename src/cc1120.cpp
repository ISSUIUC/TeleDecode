#include <SPI.h>
#include <Arduino.h>
#include "cc1120.h"
#include "cc1120_config.h"

#define AO_RADIO_MAX_SEND	sizeof(struct ao_packet)
#define AO_CC1120_TX_BUFFER	64


static uint8_t ao_radio_wake;		/* radio ready. Also used as sleep address */
static uint8_t ao_radio_abort;		/* radio operation should abort */
static uint8_t ao_radio_mcu_wake;	/* MARC status change */
static uint8_t ao_radio_tx_finished;

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


static uint8_t	tx_data[(AO_RADIO_MAX_SEND + 4) * 2];
void CC1120::ao_radio_send(const void *d, uint8_t size)
{
    //uint8_t		*e = tx_data;
	uint8_t		encode_len;
	uint8_t		this_len;
	uint8_t		started = 0;

    encode_len = size; //ao_fec_encode(d, size, tx_data);

	// ao_radio_get(encode_len);

	ao_radio_abort = 0;

	/* Flush any pending TX bytes */
	sendCommandStrobe(CC1120_SFTX); 

	// while (encode_len) { //not necessary untilwe start sending multiple packets
	// 	this_len = encode_len;

	// 	if (started) {
	// 		ao_radio_wait_fifo();
	// 		if (ao_radio_abort)
	// 			break;
	// 	}

    if (this_len > AO_CC1120_TX_BUFFER) { 
		this_len = AO_CC1120_TX_BUFFER;
		writeRegister(CC112X_IOCFG2, 2);
	} else {
		writeRegister(CC112X_IOCFG2, 26);
	}

	//ao_radio_fifo_write(e, this_len);
	//e += this_len;
		// encode_len -= this_len;

	// if (!started) {
	// 	ao_radio_start_tx();
	// 	started = 1;
	// }
	// while (started && !ao_radio_abort && !ao_radio_tx_finished) {
	// 	ao_radio_wake = 0;
	// 	ao_radio_enable_isr();
	// 	ao_radio_wait_isr(0);
	// }
	// if (ao_radio_abort)
	// 	ao_radio_idle();
	// ao_radio_put();
}