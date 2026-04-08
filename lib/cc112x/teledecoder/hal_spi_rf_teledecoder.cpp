#include "hal_spi_rf_teledecoder.h"
#include <Arduino.h>
#include <SPI.h>
#include <string.h>


/******************************************************************************
 * FUNCTIONS
 */

/******************************************************************************
 * @fn          trxRfSpiInterfaceInit
 *
 * @brief       Function to initialize TRX SPI. CC1101/CC112x is currently
 *              supported. The supported prescalerValue must be set so that
 *              SMCLK/prescalerValue does not violate radio SPI constraints.
 *
 * input parameters
 *
 * @param       prescalerValue - SMCLK/prescalerValue gives SCLK frequency
 *
 * output parameters
 *
 * @return      void
 */
void trxRfSpiInterfaceInit(uint8 prescalerValue)
{
  pinMode(SPI_RADIO_CS, OUTPUT);
  digitalWrite(SPI_RADIO_CS, HIGH);
  
  return;
}


/*******************************************************************************
 * @fn          trx8BitRegAccess
 *
 * @brief       This function performs a read or write from/to a 8bit register
 *              address space. The function handles burst and single read/write
 *              as specfied in addrByte. Function assumes that chip is ready.
 *
 * input parameters
 *
 * @param       accessType - Specifies if this is a read or write and if it's
 *                           a single or burst access. Bitmask made up of
 *                           RADIO_BURST_ACCESS/RADIO_SINGLE_ACCESS/
 *                           RADIO_WRITE_ACCESS/RADIO_READ_ACCESS.
 * @param       addrByte - address byte of register.
 * @param       pData    - data array
 * @param       len      - Length of array to be read(TX)/written(RX)
 *
 * output parameters
 *
 * @return      chip status
 */
rfStatus_t trx8BitRegAccess(uint8 accessType, uint8 addrByte, uint8 *pData, uint16 len)
{
  rfStatus_t readValue[sizeof(uint8_t)];

//   /* Pull CS_N low and wait for SO to go low before communication starts */
  // TRXEM_SPI_BEGIN();
  // while(TRXEM_PORT_IN & TRXEM_SPI_MISO_PIN);
  // /* send register address byte */
  // TRXEM_SPI_TX(accessType|addrByte);
  // TRXEM_SPI_WAIT_DONE();
  // /* Storing chip status */
  // readValue = TRXEM_SPI_RX();
  // trxReadWriteBurstSingle(accessType|addrByte,pData,len);
  // TRXEM_SPI_END();
//   /* return the status byte value */

  // AccessType is a Read
  if(accessType & RADIO_READ_ACCESS) {
    memset(readValue, addrByte, len);
  }

  // AccessType is a Write
  if(accessType & RADIO_WRITE_ACCESS){
    volatile int* accessed_reg = reinterpret_cast<volatile int*>(addrByte);
  }

  // AccessType is a Write and an Array? (Unsure about this)
  if(accessType & (RADIO_BURST_ACCESS|RADIO_WRITE_ACCESS)){

  }

  return(*readValue);
}

/******************************************************************************
 * @fn          trx16BitRegAccess
 *
 * @brief       This function performs a read or write in the extended adress
 *              space of CC112X.
 *
 * input parameters
 *
 * @param       accessType - Specifies if this is a read or write and if it's
 *                           a single or burst access. Bitmask made up of
 *                           RADIO_BURST_ACCESS/RADIO_SINGLE_ACCESS/
 *                           RADIO_WRITE_ACCESS/RADIO_READ_ACCESS.
 * @param       extAddr - Extended register space address = 0x2F.
 * @param       regAddr - Register address in the extended address space.
 * @param       *pData  - Pointer to data array for communication
 * @param       len     - Length of bytes to be read/written from/to radio
 *
 * output parameters
 *
 * @return      rfStatus_t
 */
rfStatus_t trx16BitRegAccess(uint8 accessType, uint8 extAddr, uint8 regAddr, uint8 *pData, uint8 len)
{
  rfStatus_t readValue[sizeof(uint16_t)];

  // AccessType is a Read
  if(accessType & RADIO_READ_ACCESS) {
    memset(readValue, regAddr, len);
  }

  // AccessType is a Write
  if(accessType & RADIO_WRITE_ACCESS){
    volatile int* accessed_reg = reinterpret_cast<volatile int*>(regAddr);
  }

  // AccessType is a Write and an Array? (Unsure about this)
  if(accessType & (RADIO_BURST_ACCESS|RADIO_WRITE_ACCESS)){

  }

  return(*readValue);
}

/*******************************************************************************
 * @fn          trxSpiCmdStrobe
 *
 * @brief       Send command strobe to the radio. Returns status byte read
 *              during transfer of command strobe. Validation of provided
 *              is not done. Function assumes chip is ready.
 *
 * input parameters
 *
 * @param       cmd - command strobe
 *
 * output parameters
 *
 * @return      status byte
 */
rfStatus_t trxSpiCmdStrobe(uint8 cmd)
{
    // uint8 rc;
    // TRXEM_SPI_BEGIN();
    // // while(TRXEM_PORT_IN & TRXEM_SPI_MISO_PIN);
    // TRXEM_SPI_TX(cmd);
    // TRXEM_SPI_WAIT_DONE();
    // rc = TRXEM_SPI_RX();
    // TRXEM_SPI_END();
    // return(rc);
}

// /*******************************************************************************
//  * @fn          trxReadWriteBurstSingle
//  *
//  * @brief       When the address byte is sent to the SPI slave, the next byte
//  *              communicated is the data to be written or read. The address
//  *              byte that holds information about read/write -and single/
//  *              burst-access is provided to this function.
//  *
//  *              Depending on these two bits this function will write len bytes to
//  *              the radio in burst mode or read len bytes from the radio in burst
//  *              mode if the burst bit is set. If the burst bit is not set, only
//  *              one data byte is communicated.
//  *
//  *              NOTE: This function is used in the following way:
//  *
//  *              TRXEM_SPI_BEGIN();
//  *              while(TRXEM_PORT_IN & TRXEM_SPI_MISO_PIN);
//  *              ...[Depending on type of register access]
//  *              trxReadWriteBurstSingle(uint8 addr,uint8 *pData,uint16 len);
//  *              TRXEM_SPI_END();
//  *
//  * input parameters
//  *
//  * @param       none
//  *
//  * output parameters
//  *
//  * @return      void
//  */
// static void trxReadWriteBurstSingle(uint8 addr,uint8 *pData,uint16 len)
// {
// 	uint16 i;
// 	/* Communicate len number of bytes: if RX - the procedure sends 0x00 to push bytes from slave*/
//   if(addr&RADIO_READ_ACCESS)
//   {
//     if(addr&RADIO_BURST_ACCESS)
//     {
//       for (i = 0; i < len; i++)
//       {
//           TRXEM_SPI_TX(0);            /* Possible to combining read and write as one access type */
//           TRXEM_SPI_WAIT_DONE();
//           *pData = TRXEM_SPI_RX();     /* Store pData from last pData RX */
//           pData++;
//       }
//     }
//     else
//     {
//       TRXEM_SPI_TX(0);
//       TRXEM_SPI_WAIT_DONE();
//       *pData = TRXEM_SPI_RX();
//     }
//   }
//   else
//   {
//     if(addr&RADIO_BURST_ACCESS)
//     {
//       /* Communicate len number of bytes: if TX - the procedure doesn't overwrite pData */
//       for (i = 0; i < len; i++)
//       {
//         TRXEM_SPI_TX(*pData);
//         TRXEM_SPI_WAIT_DONE();
//         pData++;
//       }
//     }
//     else
//     {
//       TRXEM_SPI_TX(*pData);
//       TRXEM_SPI_WAIT_DONE();
//     }
//   }
//   return;
// }
