#include <Arduino.h>
#include "cc1120.h"


/* RX filter BW = 100.000000 */
/* Address config = No address check */
/* Packet length = 255 */
/* Symbol rate = 38.3606 */
/* PA ramping = false */
/* Carrier frequency = 434.549988 */
/* Bit rate = 38.3606 */
/* Whitening = true */
/* Manchester enable = false */
/* Modulation format = 2-GFSK */
/* Packet length mode = Variable */
/* Device address = 0 */
/* TX power = 15 */
/* Deviation = 20.507812 */
/***************************************************************
 *  SmartRF Studio(tm) Export
 *
 *  Radio register settings specifed with address, value
 *
 *  RF device: CC1120
 *
 ***************************************************************/

#ifndef AO_CC1120_AGC_GAIN_ADJUST
#define AO_CC1120_AGC_GAIN_ADJUST	(uint8_t)-80
#endif

typedef struct
{
  uint16_t  addr;
  uint8_t   data;
} registerSetting_t;


const registerSetting_t cc1120_settings[] =
{
        {CC112X_SYNC3, 0xD3},       /* Sync Word Configuration [31:24] */
        {CC112X_SYNC2, 0x91},       /* Sync Word Configuration [23:16] */
        {CC112X_SYNC1, 0xD3},       /* Sync Word Configuration [15:8] */
        {CC112X_SYNC0, 0x91},       /* Sync Word Configuration [7:0] */

        {CC112X_SYNC_CFG1,				   /* Sync Word Detection Configuration */
            (uint8_t)((CC1120_SYNC_CFG1_DEM_CFG_PQT_GATING_DISABLED << CC1120_SYNC_CFG1_DEM_CFG) |
            (0xc << CC1120_SYNC_CFG1_SYNC_THR))},
        {CC112X_SYNC_CFG0,
            (uint8_t)((CC1120_SYNC_CFG0_SYNC_MODE_16_BITS << CC1120_SYNC_CFG0_SYNC_MODE) |
            (CC1120_SYNC_CFG0_SYNC_NUM_ERROR_DISABLED << CC1120_SYNC_CFG0_SYNC_NUM_ERROR))},
        {CC112X_DCFILT_CFG,                     0x15},       /* Digital DC Removal Configuration */
        {CC112X_PREAMBLE_CFG1,                         	   /* Preamble Length Configuration */
		(uint8_t)(CC1120_PREAMBLE_CFG1_NUM_PREAMBLE_4_BYTES << CC1120_PREAMBLE_CFG1_NUM_PREAMBLE) |
		(uint8_t)(CC1120_PREAMBLE_CFG1_PREAMBLE_WORD_AA << CC1120_PREAMBLE_CFG1_PREAMBLE_WORD)},
        {CC112X_PREAMBLE_CFG0,
		(uint8_t)((0 << CC1120_PREAMBLE_CFG0_PQT_EN) |
		(0xe << CC1120_PREAMBLE_CFG0_PQT))},

	/* Adjust PQT lower to accept fewer packets */

        {CC112X_FREQ_IF_CFG,                    0x3a},       /* RX Mixer Frequency Configuration */
        {CC112X_IQIC,                           0x00},       /* Digital Image Channel Compensation Configuration */
        {CC112X_CHAN_BW,                        0x02},       /* Channel Filter Configuration */

        {CC112X_MDMCFG1,                     		   /* General Modem Parameter Configuration */
		(uint8_t)((0 << CC1120_MDMCFG1_CARRIER_SENSE_GATE) |
		(1 << CC1120_MDMCFG1_FIFO_EN) |
		(0 << CC1120_MDMCFG1_MANCHESTER_EN) |
		(0 << CC1120_MDMCFG1_INVERT_DATA_EN) |
		(0 << CC1120_MDMCFG1_COLLISION_DETECT_EN) |
		(CC1120_MDMCFG1_DVGA_GAIN_0 << CC1120_MDMCFG1_DVGA_GAIN) |
		(0 << CC1120_MDMCFG1_SINGLE_ADC_EN))},
        {CC112X_MDMCFG0,                        0x0d},       /* General Modem Parameter Configuration */

	/* AGC reference = 10 * log10(receive BW) - 4 = 10 * log10(100e3) - 4 = 46 */
        {CC112X_AGC_REF,                        0x36},         /* AGC Reference Level Configuration */

	/* Carrier sense threshold - 25dB above the noise */
        {CC112X_AGC_CS_THR,                     25},         /* Carrier Sense Threshold Configuration */
        {CC112X_AGC_GAIN_ADJUST,				   /* RSSI Offset Configuration */
                (uint16_t) AO_CC1120_AGC_GAIN_ADJUST},

	    {CC112X_AGC_CFG3,                              	   /* AGC Configuration */
		(uint8_t)((1 << CC1120_AGC_CFG3_RSSI_STEP_THR) |
		(17 << CC1120_AGC_CFG3_AGC_MIN_GAIN))},

        {CC112X_AGC_CFG2,       				   /* AGC Configuration */
		(uint8_t)((0 << CC1120_AGC_CFG2_START_PREVIOUS_GAIN_EN) |
		(CC1120_AGC_CFG2_FE_PERFORMANCE_MODE_NORMAL << CC1120_AGC_CFG2_FE_PERFORMANCE_MODE) |
		(0 << CC1120_AGC_CFG2_AGC_MAX_GAIN))},

        {CC112X_AGC_CFG1,       				   /* AGC Configuration */
		(uint8_t)((CC1120_AGC_CFG1_AGC_SYNC_BEHAVIOR_UPDATE_AGC_UPDATE_RSSI_SLOW << CC1120_AGC_CFG1_AGC_SYNC_BEHAVIOR) |
		(CC1120_AGC_CFG1_AGC_WIN_SIZE_32 << CC1120_AGC_CFG1_AGC_WIN_SIZE) |
		(CC1120_AGC_CFG1_AGC_SETTLE_WAIT_32 << CC1120_AGC_CFG1_AGC_SETTLE_WAIT))},

        {CC112X_AGC_CFG0,       				   /* AGC Configuration */
		(uint8_t)((CC1120_AGC_CFG0_AGC_HYST_LEVEL_10 << CC1120_AGC_CFG0_AGC_HYST_LEVEL) |
		(CC1120_AGC_CFG0_AGC_SLEWRATE_LIMIT_60 << CC1120_AGC_CFG0_AGC_SLEWRATE_LIMIT) |
		(CC1120_AGC_CFG0_RSSI_VALID_CNT_9 << CC1120_AGC_CFG0_RSSI_VALID_CNT) |
		(CC1120_AGC_CFG0_AGC_ASK_DECAY_1_128 << CC1120_AGC_CFG0_AGC_ASK_DECAY))},

        {CC112X_FIFO_CFG,		       		   /* FIFO Configuration */
		(uint8_t)((0 << CC1120_FIFO_CFG_CRC_AUTOFLUSH) |
		(0x40 << CC1120_FIFO_CFG_FIFO_THR))},

        {CC112X_DEV_ADDR,                       0x00},       /* Device Address Configuration */

        {CC112X_SETTLING_CFG,                          	   /* Frequency Synthesizer Calibration and Settling Configuration */
		(uint8_t)((CC1120_SETTLING_CFG_FS_AUTOCAL_IDLE_TO_ON << CC1120_SETTLING_CFG_FS_AUTOCAL) |
		(CC1120_SETTLING_CFG_LOCK_TIME_75_30 << CC1120_SETTLING_CFG_LOCK_TIME) |
		(CC1120_SETTLING_CFG_FSREG_TIME_60 << CC1120_SETTLING_CFG_FSREG_TIME))},

        {CC112X_FS_CFG,                                	   /* Frequency Synthesizer Configuration */
		(uint8_t)((1 << CC1120_FS_CFG_LOCK_EN) |
		(CC1120_FS_CFG_FSD_BANDSELECT_410_480 << CC1120_FS_CFG_FSD_BANDSELECT))},

        {CC112X_WOR_CFG1,                       0x08},       /* eWOR Configuration, Reg 1 */
        {CC112X_WOR_CFG0,                       0x21},       /* eWOR Configuration, Reg 0 */
        {CC112X_WOR_EVENT0_MSB,                 0x00},       /* Event 0 Configuration */
        {CC112X_WOR_EVENT0_LSB,                 0x00},       /* Event 0 Configuration */
        {CC112X_PKT_CFG2,                       0x00},       /* Packet Configuration, Reg 2 */
#if 0
        {CC112X_PKT_CFG1,                       0x45},       /* Packet Configuration, Reg 1 */
        {CC112X_PKT_CFG0,                       0x00},       /* Packet Configuration, Reg 0 */
#endif
        {CC112X_RFEND_CFG1,                     0x0e},       /* RFEND Configuration, Reg 1 */
        {CC112X_RFEND_CFG0,                     0x00},       /* RFEND Configuration, Reg 0 */
	//        CC112X_PA_CFG2,                        0x3f,       /* Power Amplifier Configuration, Reg 2 */
	    {CC112X_PA_CFG2,                        0x3f},       /* Power Amplifier Configuration, Reg 2 */
        {CC112X_PA_CFG1,     0x56},       /* Power Amplifier Configuration, Reg 1 */
        {CC112X_PA_CFG0,     0x7b},       /* Power Amplifier Configuration, Reg 0 */
        {CC112X_PKT_LEN,     0xff},       /* Packet Length Configuration */
        {CC112X_IF_MIX_CFG,     0x00},       /* IF Mix Configuration */
        {CC112X_FREQOFF_CFG,     0x20},       /* Frequency Offset Correction Configuration */
        {CC112X_TOC_CFG,      0x0a},       /* Timing Offset Correction Configuration */
        {CC112X_MARC_SPARE,      0x00},       /* MARC Spare */
        {CC112X_ECG_CFG,     0x00},       /* External Clock Frequency Configuration */
        {CC1120_SOFT_TX_DATA_CFG,      0x00},       /* Soft TX Data Configuration */
        {CC112X_EXT_CTRL,      0x00},       /* External Control Configuration */
        {CC112X_RCCAL_FINE,      0x00},       /* RC Oscillator Calibration (fine) */
        {CC112X_RCCAL_COARSE,      0x00},       /* RC Oscillator Calibration (coarse) */
        {CC112X_RCCAL_OFFSET,      0x00},       /* RC Oscillator Calibration Clock Offset */
        {CC112X_FREQOFF1,      0x00},       /* Frequency Offset (MSB) */
        {CC112X_FREQOFF0,      0x00},       /* Frequency Offset (LSB) */
        {CC112X_IF_ADC2,      0x02},       /* Analog to Digital Converter Configuration, Reg 2 */
        {CC112X_IF_ADC1,      0xa6},       /* Analog to Digital Converter Configuration, Reg 1 */
        {CC112X_IF_ADC0,                        0x04},       /* Analog to Digital Converter Configuration, Reg 0 */
        {CC112X_FS_DIG1,                        0x00},       /*  */
        {CC112X_FS_DIG0,                        0x5f},       /*  */
        {CC112X_FS_CAL3,                        0x00},       /*  */
        {CC112X_FS_CAL2,                        0x20},       /*  */
        {CC112X_FS_CAL1,                        0x40},       /*  */
        {CC112X_FS_CAL0,                        0x0e},       /*  */
        {CC112X_FS_CHP,                         0x28},       /* Charge Pump Configuration */
        {CC112X_FS_DIVTWO,                      0x03},       /* Divide by 2 */
        {CC112X_FS_DSM1,                        0x00},       /* Digital Synthesizer Module Configuration, Reg 1 */
        {CC112X_FS_DSM0,                        0x33},       /* Digital Synthesizer Module Configuration, Reg 0 */
        {CC112X_FS_DVC1,                        0xff},       /* Divider Chain Configuration, Reg 1 */
        {CC112X_FS_DVC0,                        0x17},       /* Divider Chain Configuration, Reg 0 */
        {CC112X_FS_LBI,                         0x00},       /* Local Bias Configuration */
        {CC112X_FS_PFD,                         0x50},       /* Phase Frequency Detector Configuration */
        {CC112X_FS_PRE,                         0x6e},       /* Prescaler Configuration */
        {CC112X_FS_REG_DIV_CML,                 0x14},       /*  */
        {CC112X_FS_SPARE,                       0xac},       /*  */
        {CC112X_FS_VCO4,                        0x14},       /* VCO Configuration, Reg 4 */
        {CC112X_FS_VCO3,                        0x00},       /* VCO Configuration, Reg 3 */
        {CC112X_FS_VCO2,                        0x00},       /* VCO Configuration, Reg 2 */
        {CC112X_FS_VCO1,                        0x00},       /* VCO Configuration, Reg 1 */
        {CC112X_FS_VCO0,                        0xb4},       /* VCO Configuration, Reg 0 */
        {CC112X_GBIAS6,                         0x00},       /* Global Bias Configuration, Reg 6 */
        {CC112X_GBIAS5,                         0x02},       /* Global Bias Configuration, Reg 5 */
        {CC112X_GBIAS4,                         0x00},       /* Global Bias Configuration, Reg 4 */
        {CC112X_GBIAS3,                         0x00},       /* Global Bias Configuration, Reg 3 */
        {CC112X_GBIAS2,                         0x10},       /* Global Bias Configuration, Reg 2 */
        {CC112X_GBIAS1,                         0x00},       /* Global Bias Configuration, Reg 1 */
        {CC112X_GBIAS0,                         0x00},       /* Global Bias Configuration, Reg 0 */
        {CC112X_IFAMP,                          0x01},       /* Intermediate Frequency Amplifier Configuration */
        {CC112X_LNA,                            0x01},       /* Low Noise Amplifier Configuration */
        {CC112X_RXMIX,                          0x01},       /* RX Mixer Configuration */
        {CC112X_XOSC5,                          0x0e},       /* Crystal Oscillator Configuration, Reg 5 */
        {CC112X_XOSC4,                          0xa0},       /* Crystal Oscillator Configuration, Reg 4 */
        {CC112X_XOSC3,                          0x03},       /* Crystal Oscillator Configuration, Reg 3 */
        {CC112X_XOSC2,                          0x04},       /* Crystal Oscillator Configuration, Reg 2 */
        {CC112X_XOSC1,                          0x03},       /* Crystal Oscillator Configuration, Reg 1 */
        {CC112X_XOSC0,                          0x00},       /* Crystal Oscillator Configuration, Reg 0 */
        {CC112X_ANALOG_SPARE,                   0x00},       /*  */
        {CC112X_PA_CFG3,                        0x00},       /* Power Amplifier Configuration, Reg 3 */
        {CC112X_WOR_TIME1,                      0x00},       /* eWOR Timer Status (MSB) */
        {CC112X_WOR_TIME0,                      0x00},       /* eWOR Timer Status (LSB) */
        {CC112X_WOR_CAPTURE1,                   0x00},       /* eWOR Timer Capture (MSB) */
        {CC112X_WOR_CAPTURE0,                   0x00},       /* eWOR Timer Capture (LSB) */
        {CC112X_BIST,                           0x00},       /* MARC BIST */
        {CC112X_DCFILTOFFSET_I1,                0x00},       /* DC Filter Offset I (MSB) */
        {CC112X_DCFILTOFFSET_I0,                0x00},       /* DC Filter Offset I (LSB) */
        {CC112X_DCFILTOFFSET_Q1,                0x00},       /* DC Filter Offset Q (MSB) */
        {CC112X_DCFILTOFFSET_Q0,                0x00},       /* DC Filter Offset Q (LSB) */
        {CC112X_IQIE_I1,                        0x00},       /* IQ Imbalance Value I (MSB) */
        {CC112X_IQIE_I0,                        0x00},       /* IQ Imbalance Value I (LSB) */
        {CC112X_IQIE_Q1,                        0x00},       /* IQ Imbalance Value Q (MSB) */
        {CC112X_IQIE_Q0,                        0x00},       /* IQ Imbalance Value Q (LSB) */
        {CC112X_RSSI1,                          0x80},       /* Received Signal Strength Indicator (MSB) */
        {CC112X_RSSI0,                          0x00},       /* Received Signal Strength Indicator (LSB) */
        {CC112X_MARCSTATE,                      0x41},       /* MARC State */
        {CC112X_LQI_VAL,                        0x00},       /* Link Quality Indicator Value */
        {CC112X_PQT_SYNC_ERR,                   0xff},       /* Preamble and Sync Word Error */
        {CC112X_DEM_STATUS,                     0x00},       /* Demodulator Status */
        {CC112X_FREQOFF_EST1,                   0x00},       /* Frequency Offset Estimate (MSB) */
        {CC112X_FREQOFF_EST0,                   0x00},       /* Frequency Offset Estimate (LSB) */
        {CC112X_AGC_GAIN3,                      0x00},       /* AGC Gain, Reg 3 */
        {CC112X_AGC_GAIN2,                      0xd1},       /* AGC Gain, Reg 2 */
        {CC112X_AGC_GAIN1,                      0x00},       /* AGC Gain, Reg 1 */
        {CC112X_AGC_GAIN0,                      0x3f},       /* AGC Gain, Reg 0 */
        {CC112X_RNDGEN,                         0x7f},       /* Random Number Value */
        {CC112X_FSCAL_CTRL,                     0x01},       /*  */
        {CC112X_PHASE_ADJUST,                   0x00},       /*  */
        {CC112X_SERIAL_STATUS,                  0x00},       /* Serial Status */
        {CC112X_PA_IFAMP_TEST,                  0x00},       /*  */
        {CC112X_FSRF_TEST,                      0x00},       /*  */
        {CC112X_PRE_TEST,                       0x00},       /*  */
        {CC112X_PRE_OVR,                        0x00},       /*  */
        {CC112X_ADC_TEST,                       0x00},       /* ADC Test */
        {CC112X_DVC_TEST,                       0x0b},       /* DVC Test */
        {CC112X_ATEST,                          0x40},       /*  */
        {CC112X_ATEST_LVDS,                     0x00},       /*  */
        {CC112X_ATEST_MODE,                     0x00},       /*  */
        {CC112X_XOSC_TEST1,                     0x3c},       /*  */
        {CC112X_XOSC_TEST0,                     0x00},       /*  */
        {CC112X_RXFIRST,                        0x00},       /* RX FIFO Pointer (first entry) */
        {CC112X_TXFIRST,                        0x00},       /* TX FIFO Pointer (first entry) */
        {CC112X_RXLAST,                         0x00},       /* RX FIFO Pointer (last entry) */
        {CC112X_TXLAST,                         0x00},       /* TX FIFO Pointer (last entry) */
};

const registerSetting_t packet_setup[] = {
    {CC112X_SYMBOL_RATE1, (PACKET_DRATE_M >> 8) & 0xff},
    {CC112X_SYMBOL_RATE0, (PACKET_DRATE_M >> 0) & 0xff},
    {CC112X_PKT_CFG2, 0x00},
    {CC112X_PKT_CFG1, 0x00},
    {CC112X_PKT_CFG0, 0x00},
    {CC112X_PREAMBLE_CFG1, (6 << 2)}
};

const registerSetting_t packet_setup_384[] = {
    {CC112X_DEVIATION_M, 80}, 
    {CC112X_MODCFG_DEV_E, ((0 << 6) | (1 << 3) | (5 << 0))}, /* Modem mode normal, 2-GFSK */
    {CC112X_SYMBOL_RATE2, ((9 << 4) | (((PACKET_DRATE_M >> 16) & 0xf) << 0))},
    {CC112X_CHAN_BW, ((0 << 7) | (0 << 6) | (2 << 0))}, 
    {CC112X_PA_CFG0, 0x7b}
};

const registerSetting_t packet_setup_96[] = {
    {CC112X_DEVIATION_M, 80}, 
    {CC112X_MODCFG_DEV_E, ((0 << 6) | (1 << 3) | (3 << 0))}, /* Modem mode normal, 2-GFSK */
    {CC112X_SYMBOL_RATE2, ((7 << 4) | (((PACKET_DRATE_M >> 16) & 0xf) << 0))},
    {CC112X_CHAN_BW, ((0 << 7) | (0 << 6) | (10 << 0))}, 
    {CC112X_PA_CFG0, 0x7d}
};
