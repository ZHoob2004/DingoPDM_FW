/*
 * ads1x15.c
 *
 *  Created on: Jul 28, 2020
 *      Author: coryg
 */

#include "ads1x15.h"

#define I2C_TIMEOUT 100

HAL_StatusTypeDef ADS1x15_SendRegs(I2C_HandleTypeDef* hi2c, uint16_t addr, ads1x15Settings_t *settings, uint8_t channel)
{
  HAL_StatusTypeDef eStatus;

	if(channel > 3) return HAL_ERROR;

	uint16_t config =
				ADS1015_REG_CONFIG_CQUE_NONE |    // Disable the comparator (default val)
				ADS1015_REG_CONFIG_CLAT_NONLAT |  // Non-latching (default val)
				ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
				ADS1015_REG_CONFIG_CMODE_TRAD |   // Traditional comparator (default val)
				ADS1015_REG_CONFIG_MODE_SINGLE;

	config |= settings->dataRate;
	config |= settings->gain;

	switch(channel){
	case (0):
		config |= ADS1015_REG_CONFIG_MUX_SINGLE_0;
		break;
	case (1):
		config |= ADS1015_REG_CONFIG_MUX_SINGLE_1;
		break;
	case (2):
		config |= ADS1015_REG_CONFIG_MUX_SINGLE_2;
		break;
	case (3):
		config |= ADS1015_REG_CONFIG_MUX_SINGLE_3;
		break;
	}

	config |= ADS1015_REG_CONFIG_OS_SINGLE;

	uint8_t writeVals[3];

	writeVals[0] = ADS1015_REG_POINTER_CONFIG;
	writeVals[1] = config >> 8;
	writeVals[2] = config & 0xFF;

	eStatus = HAL_I2C_Master_Transmit(hi2c, addr << 1, writeVals, 3, I2C_TIMEOUT);
  if( eStatus != HAL_OK)
  {
    return eStatus;
  }

	//Send convert register
	writeVals[0] = ADS1015_REG_POINTER_CONVERT;

  eStatus = HAL_I2C_Master_Transmit(hi2c, addr << 1, writeVals, 1, I2C_TIMEOUT);
  if( eStatus != HAL_OK)
  {
    return eStatus;
  }

  return HAL_OK;
}

HAL_StatusTypeDef ADS1x15_ReadADC(I2C_HandleTypeDef* hi2c, uint16_t addr, ads1x15Settings_t *settings, uint16_t* val)
{
  HAL_StatusTypeDef eStatus;

  //Read received values
	uint8_t readVals[2];

	//Msg received - comms OK
	settings->commsOk = 1;

	eStatus = HAL_I2C_Master_Receive(hi2c, addr << 1, readVals, 2, I2C_TIMEOUT);
	if( eStatus != HAL_OK)
  {
    return eStatus;
  }

	uint16_t valRead = (readVals[0] << 8 | readVals[1]) >> settings->bitShift;

	if (settings->deviceType == ADS1115) {
	  *val = valRead;
	  return HAL_OK;
  }
	else {
    // Shift 12-bit results right 4 bits for the ADS1015,
    // making sure we keep the sign bit intact
    if (valRead > 0x07FF) {
      // negative number - extend the sign to 16th bit
      valRead |= 0xF000;
    }
    *val = valRead;
    return HAL_OK;
  }
}