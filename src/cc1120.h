// Based on datasheet: https://www.ti.com/lit/ug/swru295e/swru295e.pdf
#include <stdint.h>
#include <SPI.h>

// TODO: Add state definitions (See table 2)
typedef uint8_t rfStatus_t; // See table 2

class CC1120 {
    public:
        CC1120(SPIClass& spi, uint8_t pin_cs, uint8_t pin_gpio2): spi(spi), pin_cs(pin_cs), pin_gpio2(pin_gpio2){}
        void applyConfiguration();
        /* @brief Reads the next available packet. Returns 1 if successful */
        int getNextPacket(uint8_t *packet, uint8_t packet_length);

        rfStatus_t setupRadio();
    private:
        /* @brief Writes a buffer in the register space*/
        rfStatus_t writeRegister(uint8_t address, uint8_t buffer);
        /* @brief Reads a buffer from the register space*/
        rfStatus_t readRegister(uint8_t address, uint8_t* buffer);
        /* @brief Writes a buffer in the extended register space*/
        rfStatus_t writeRegisterExtended(uint8_t address, uint8_t buffer);
        /* @brief Reads a buffer from the extended register space*/
        rfStatus_t readRegisterExtended(uint8_t address, uint8_t* buffer);
        /* @brief Sends a command strobe */
        rfStatus_t sendCommandStrobe(uint8_t command);
        /* @brief Gets the current status */
        rfStatus_t getStatus();

        SPIClass& spi;
        SPISettings spiSettings = SPISettings(10000000, SPI_MSBFIRST, SPI_MODE0); // copied from E22 Driver, need to verify

        uint8_t pin_cs;
        uint8_t pin_gpio2;
        bool configured = false;
};

/* SPI Access Bitmasks */ 
#define SINGLE_REGISTER_WRITE           0x00
#define SINGLE_REGISTER_READ            0x80

#define SINGLE_EXTENDED_REGISTER_WRITE  0x2F
#define SINGLE_EXTENDED_REGISTER_READ   0xAF

#define FIFO_BURST_ACCESS               0x7F

/* configuration registers */
// benson start

// GPIO Configuration - Most likely default is fine
#define CC112X_IOCFG3                   0x0000
/*
GPIO2: Asserted in RX when PKT_CFG1.CRC_CFG = 1 or 10b and a good
packet is received. This signal is always on if the radio is in TX or if the
radio is in RX and PKT_CFG1.CRC_CFG = 0. The signal is de-asserted
when RX mode is entered and PKT_CFG1.CRC_CFG ≠ 0. This signal is
also available in the LQI_VAL register
*/
#define CC112X_IOCFG2                   0x0001 // only GPIO connected to esp32 (can change pcb to use others if needed)
#define CC112X_IOCFG1                   0x0002 // shared pin with MISO
#define CC112X_IOCFG0                   0x0003

// Sync configuration
// from rust: const SYNC_PATTERN: &[u8] = "1010101010101010101101001110010001".as_bytes();`
#define CC112X_SYNC3                    0x0004
#define CC112X_SYNC2                    0x0005
#define CC112X_SYNC1                    0x0006
#define CC112X_SYNC0                    0x0007
#define CC112X_SYNC_CFG1                0x0008
#define CC112X_SYNC_CFG0                0x0009
#define CC112X_DEVIATION_M              0x000A
#define CC112X_MODCFG_DEV_E             0x000B
#define CC112X_DCFILT_CFG               0x000C
#define CC112X_PREAMBLE_CFG1            0x000D
#define CC112X_PREAMBLE_CFG0            0x000E
#define CC112X_FREQ_IF_CFG              0x000F
#define CC112X_IQIC                     0x0010
#define CC112X_CHAN_BW                  0x0011
#define CC112X_MDMCFG1                  0x0012
#define CC112X_MDMCFG0                  0x0013
#define CC112X_SYMBOL_RATE2             0x0014
#define CC112X_SYMBOL_RATE1             0x0015
#define CC112X_SYMBOL_RATE0             0x0016
#define CC112X_AGC_REF                  0x0017
#define CC112X_AGC_CS_THR               0x0018
#define CC112X_AGC_GAIN_ADJUST          0x0019
#define CC112X_AGC_CFG3                 0x001A
#define CC112X_AGC_CFG2                 0x001B
#define CC112X_AGC_CFG1                 0x001C
#define CC112X_AGC_CFG0                 0x001D
#define CC112X_FIFO_CFG                 0x001E
#define CC112X_DEV_ADDR                 0x001F
#define CC112X_SETTLING_CFG             0x0020
#define CC112X_FS_CFG                   0x0021
#define CC112X_WOR_CFG1                 0x0022
#define CC112X_WOR_CFG0                 0x0023
#define CC112X_WOR_EVENT0_MSB           0x0024
#define CC112X_WOR_EVENT0_LSB           0x0025
#define CC112X_PKT_CFG2                 0x0026
#define CC112X_PKT_CFG1                 0x0027
#define CC112X_PKT_CFG0                 0x0028
#define CC112X_RFEND_CFG1               0x0029
#define CC112X_RFEND_CFG0               0x002A
#define CC112X_PA_CFG2                  0x002B
#define CC112X_PA_CFG1                  0x002C
#define CC112X_PA_CFG0                  0x002D
#define CC112X_PKT_LEN                  0x002E

/* Extended Configuration Registers */
/*jennifer*/
#define CC112X_IF_MIX_CFG               0x04 //only other configuration in datasheet is 0x00
#define CC112X_FREQOFF_CFG              0x2F01 //0x22 disables PLL feedback, 0x34 / 0x30 enables PLL feedback
#define CC112X_TOC_CFG                  0x2F02
#define CC112X_MARC_SPARE               0x2F03
#define CC112X_ECG_CFG                  0x2F04
#define CC112X_CFM_DATA_CFG             0x2F05
#define CC112X_EXT_CTRL                 0x2F06
#define CC112X_RCCAL_FINE               0x2F07
#define CC112X_RCCAL_COARSE             0x2F08
#define CC112X_RCCAL_OFFSET             0x2F09
#define CC112X_FREQOFF1                 0x2F0A
#define CC112X_FREQOFF0                 0x2F0B
#define CC112X_FREQ2                    0x2F0C
#define CC112X_FREQ1                    0x2F0D
#define CC112X_FREQ0                    0x2F0E
#define CC112X_IF_ADC2                  0x2F0F
#define CC112X_IF_ADC1                  0x2F10
#define CC112X_IF_ADC0                  0x2F11
#define CC112X_FS_DIG1                  0x2F12
#define CC112X_FS_DIG0                  0x2F13
#define CC112X_FS_CAL3                  0x2F14
#define CC112X_FS_CAL2                  0x2F15
#define CC112X_FS_CAL1                  0x2F16
#define CC112X_FS_CAL0                  0x2F17
#define CC112X_FS_CHP                   0x2F18
#define CC112X_FS_DIVTWO                0x2F19
#define CC112X_FS_DSM1                  0x2F1A
#define CC112X_FS_DSM0                  0x2F1B
#define CC112X_FS_DVC1                  0x2F1C
#define CC112X_FS_DVC0                  0x2F1D
#define CC112X_FS_LBI                   0x2F1E
#define CC112X_FS_PFD                   0x2F1F
#define CC112X_FS_PRE                   0x2F20
#define CC112X_FS_REG_DIV_CML           0x2F21
#define CC112X_FS_SPARE                 0x2F22
#define CC112X_FS_VCO4                  0x2F23
#define CC112X_FS_VCO3                  0x2F24
#define CC112X_FS_VCO2                  0x2F25
#define CC112X_FS_VCO1                  0x2F26
#define CC112X_FS_VCO0                  0x2F27
#define CC112X_GBIAS6                   0x2F28
#define CC112X_GBIAS5                   0x2F29
#define CC112X_GBIAS4                   0x2F2A
#define CC112X_GBIAS3                   0x2F2B
#define CC112X_GBIAS2                   0x2F2C
#define CC112X_GBIAS1                   0x2F2D
#define CC112X_GBIAS0                   0x2F2E
#define CC112X_IFAMP                    0x2F2F
#define CC112X_LNA                      0x2F30
#define CC112X_RXMIX                    0x2F31
#define CC112X_XOSC5                    0x2F32
#define CC112X_XOSC4                    0x2F33
#define CC112X_XOSC3                    0x2F34
#define CC112X_XOSC2                    0x2F35
#define CC112X_XOSC1                    0x2F36
#define CC112X_XOSC0                    0x2F37
#define CC112X_ANALOG_SPARE             0x2F38
#define CC112X_PA_CFG3                  0x2F39
#define CC112X_IRQ0M                    0x2F3F
#define CC112X_IRQ0F                    0x2F40 

/* Status Registers */
#define CC112X_WOR_TIME1                0x2F64
#define CC112X_WOR_TIME0                0x2F65
#define CC112X_WOR_CAPTURE1             0x2F66
#define CC112X_WOR_CAPTURE0             0x2F67
#define CC112X_BIST                     0x2F68
#define CC112X_DCFILTOFFSET_I1          0x2F69
#define CC112X_DCFILTOFFSET_I0          0x2F6A
#define CC112X_DCFILTOFFSET_Q1          0x2F6B
#define CC112X_DCFILTOFFSET_Q0          0x2F6C
#define CC112X_IQIE_I1                  0x2F6D
#define CC112X_IQIE_I0                  0x2F6E
#define CC112X_IQIE_Q1                  0x2F6F
#define CC112X_IQIE_Q0                  0x2F70
#define CC112X_RSSI1                    0x2F71
#define CC112X_RSSI0                    0x2F72
#define CC112X_MARCSTATE                0x2F73
#define CC112X_LQI_VAL                  0x2F74
#define CC112X_PQT_SYNC_ERR             0x2F75
#define CC112X_DEM_STATUS               0x2F76
#define CC112X_FREQOFF_EST1             0x2F77
#define CC112X_FREQOFF_EST0             0x2F78
#define CC112X_AGC_GAIN3                0x2F79
#define CC112X_AGC_GAIN2                0x2F7A
#define CC112X_AGC_GAIN1                0x2F7B
#define CC112X_AGC_GAIN0                0x2F7C
#define CC112X_CFM_RX_DATA_OUT          0x2F7D
#define CC112X_CFM_TX_DATA_IN           0x2F7E
#define CC112X_ASK_SOFT_RX_DATA         0x2F7F
#define CC112X_RNDGEN                   0x2F80
#define CC112X_MAGN2                    0x2F81
#define CC112X_MAGN1                    0x2F82
#define CC112X_MAGN0                    0x2F83
#define CC112X_ANG1                     0x2F84
#define CC112X_ANG0                     0x2F85
#define CC112X_CHFILT_I2                0x2F86
#define CC112X_CHFILT_I1                0x2F87
#define CC112X_CHFILT_I0                0x2F88
#define CC112X_CHFILT_Q2                0x2F89
#define CC112X_CHFILT_Q1                0x2F8A
#define CC112X_CHFILT_Q0                0x2F8B
#define CC112X_GPIO_STATUS              0x2F8C
#define CC112X_FSCAL_CTRL               0x2F8D
#define CC112X_PHASE_ADJUST             0x2F8E
#define CC112X_PARTNUMBER               0x2F8F
#define CC112X_PARTVERSION              0x2F90
#define CC112X_SERIAL_STATUS            0x2F91
#define CC112X_MODEM_STATUS1            0x2F92
#define CC112X_MODEM_STATUS0            0x2F93
#define CC112X_MARC_STATUS1             0x2F94
#define CC112X_MARC_STATUS0             0x2F95
#define CC112X_PA_IFAMP_TEST            0x2F96
#define CC112X_FSRF_TEST                0x2F97
#define CC112X_PRE_TEST                 0x2F98
#define CC112X_PRE_OVR                  0x2F99
#define CC112X_ADC_TEST                 0x2F9A
#define CC112X_DVC_TEST                 0x2F9B
#define CC112X_ATEST                    0x2F9C
#define CC112X_ATEST_LVDS               0x2F9D
#define CC112X_ATEST_MODE               0x2F9E
#define CC112X_XOSC_TEST1               0x2F9F
#define CC112X_XOSC_TEST0               0x2FA0  
                                        
#define CC112X_RXFIRST                  0x2FD2   
#define CC112X_TXFIRST                  0x2FD3   
#define CC112X_RXLAST                   0x2FD4 
#define CC112X_TXLAST                   0x2FD5 
#define CC112X_NUM_TXBYTES              0x2FD6  /* Number of bytes in TXFIFO */ 
#define CC112X_NUM_RXBYTES              0x2FD7  /* Number of bytes in RXFIFO */
#define CC112X_FIFO_NUM_TXBYTES         0x2FD8  
#define CC112X_FIFO_NUM_RXBYTES         0x2FD9  

#define CC112X_CMD_SRES                 0x30 /* Reset chip (need to tweak sendCommandStrobe to work)*/
#define CC112X_CMD_SFSTXON              0x31 /* Enable and calibrate frequency synthesizer */
#define CC112X_CMD_SXOFF                0x32 /* Enter XOFF state when CSn is de-asserted */
#define CC112X_CMD_SCAL                 0x33 /* Calibrate frequency synthesizer and turn it off.*/
#define CC112X_CMD_SRX                  0x34 /* Enable RX */
#define CC112X_CMD_STX                  0x35 /* In IDLE state: Enable TX */
#define CC112X_CMD_SIDLE                0x36 /* Exit RX/TX, turn off frequency synthesizer and exit eWOR mode if applicable */
#define CC112X_CMD_SAFC                 0x37 /* Automatic Frequency Compensation */
#define CC112X_CMD_SWOR                 0x38 /* Start automatic RX polling sequence (eWOR) */
#define CC112X_CMD_SPWD                 0x39 /* Enter SLEEP mode when CSn is de-asserted */
#define CC112X_CMD_SFRX                 0x3A /* Flush the RX FIFO */
#define CC112X_CMD_SFTX                 0x3B /* Flush the TX FIFO */
#define CC112X_CMD_SWORRST              0x3C /* Reset the eWOR timer to the Event1 value */
#define CC112X_CMD_SNOP                 0x3D /* No operation. May be used to get access to the chip status byte */

#define  CC1120_SYNC_CFG1_DEM_CFG	5
#define  CC1120_SYNC_CFG1_DEM_CFG_PQT_GATING_DISABLED	0
#define  CC1120_SYNC_CFG1_SYNC_THR	0
#define  CC1120_SYNC_CFG0_SYNC_MODE	2
#define  CC1120_SYNC_CFG0_SYNC_MODE_16_BITS		2
#define  CC1120_SYNC_CFG0_SYNC_NUM_ERROR		0
#define  CC1120_SYNC_CFG0_SYNC_NUM_ERROR_DISABLED	3
#define  CC1120_PREAMBLE_CFG1_NUM_PREAMBLE_4_BYTES	6
#define  CC1120_PREAMBLE_CFG1_NUM_PREAMBLE	2
#define  CC1120_PREAMBLE_CFG1_PREAMBLE_WORD	0
#define  CC1120_PREAMBLE_CFG1_PREAMBLE_WORD_AA		0
#define  CC1120_PREAMBLE_CFG0_PQT_EN		5
#define  CC1120_PREAMBLE_CFG0_PQT		0
#define  CC1120_MDMCFG1_CARRIER_SENSE_GATE	7
#define  CC1120_MDMCFG1_FIFO_EN			6
#define  CC1120_MDMCFG1_MANCHESTER_EN		5
#define  CC1120_MDMCFG1_INVERT_DATA_EN		4
#define  CC1120_MDMCFG1_COLLISION_DETECT_EN	3
#define  CC1120_MDMCFG1_DVGA_GAIN_0			0
#define  CC1120_MDMCFG1_DVGA_GAIN		1
#define  CC1120_MDMCFG1_SINGLE_ADC_EN		0
#define  CC1120_AGC_CFG3_RSSI_STEP_THR		7
#define  CC1120_AGC_CFG3_AGC_MIN_GAIN		0
#define  CC1120_AGC_CFG2_START_PREVIOUS_GAIN_EN	7
#define  CC1120_AGC_CFG2_FE_PERFORMANCE_MODE_NORMAL		1
#define  CC1120_AGC_CFG2_FE_PERFORMANCE_MODE	5
#define  CC1120_AGC_CFG2_AGC_MAX_GAIN		0
#define  CC1120_AGC_CFG1_AGC_SYNC_BEHAVIOR_UPDATE_AGC_UPDATE_RSSI_SLOW		2
#define  CC1120_AGC_CFG1_AGC_SYNC_BEHAVIOR	5
#define  CC1120_AGC_CFG1_AGC_WIN_SIZE_32		2
#define  CC1120_AGC_CFG1_AGC_WIN_SIZE		2
#define  CC1120_AGC_CFG1_AGC_SETTLE_WAIT_32		1
#define  CC1120_AGC_CFG1_AGC_SETTLE_WAIT	0
#define   CC1120_AGC_CFG0_AGC_HYST_LEVEL_10		3
#define  CC1120_AGC_CFG0_AGC_HYST_LEVEL		6
#define   CC1120_AGC_CFG0_AGC_SLEWRATE_LIMIT_60		0
#define  CC1120_AGC_CFG0_AGC_SLEWRATE_LIMIT	4
#define   CC1120_AGC_CFG0_RSSI_VALID_CNT_9		3
#define  CC1120_AGC_CFG0_RSSI_VALID_CNT		2
#define   CC1120_AGC_CFG0_AGC_ASK_DECAY_1_128		3
#define  CC1120_AGC_CFG0_AGC_ASK_DECAY		0
#define  CC1120_FIFO_CFG_CRC_AUTOFLUSH		7
#define  CC1120_FIFO_CFG_FIFO_THR		0
#define  CC1120_SETTLING_CFG_FS_AUTOCAL_IDLE_TO_ON	1
#define  CC1120_SETTLING_CFG_FS_AUTOCAL		3
#define  CC1120_SETTLING_CFG_LOCK_TIME_75_30		1
#define  CC1120_SETTLING_CFG_LOCK_TIME		1
#define  CC1120_SETTLING_CFG_FSREG_TIME_60		1
#define  CC1120_SETTLING_CFG_FSREG_TIME		0
#define  CC1120_FS_CFG_LOCK_EN			4
#define  CC1120_FS_CFG_FSD_BANDSELECT_410_480		4
#define  CC1120_FS_CFG_FSD_BANDSELECT		0
#define CC1120_EXTENDED_BIT	0x8000
#define CC1120_SOFT_TX_DATA_CFG	(CC1120_EXTENDED_BIT | 0x05)