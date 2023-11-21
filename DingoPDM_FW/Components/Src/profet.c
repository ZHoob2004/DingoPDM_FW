/*
 * profet.c
 *
 *  Created on: Nov 9, 2020
 *      Author: coryg
 */

#include "profet.h"

uint32_t GetTripTime(ProfetModelTypeDef eModel, uint16_t nIL, uint16_t nMaxIL);

void Profet_SM(volatile ProfetTypeDef *profet) {

  //Check for inrush
  profet->bInRushActive = (profet->nIL_InRushTime + profet->nInRushOnTime) > HAL_GetTick();

  //Check for fault (device overcurrent/overtemp/short)
  //IL will be very high
  //TODO: Calculate value from datasheet
  if (profet->nIS > 30000){
    profet->eState = FAULT;
  }

  switch (profet->eState) {

  case OFF:
    profet->cState = 'O';

    HAL_GPIO_WritePin(profet->nIN_Port, profet->nIN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(profet->nDEN_Port, profet->nDEN_Pin, GPIO_PIN_RESET);
    profet->nOC_Count = 0;

    //Check for turn on
    if (profet->eReqState == ON) {
      profet->nInRushOnTime = HAL_GetTick();
      profet->eState = ON;
    }
    break;

  case ON:
    profet->cState = '|';

    HAL_GPIO_WritePin(profet->nIN_Port, profet->nIN_Pin, GPIO_PIN_SET);
    //Turn DEN on = activates IS
    HAL_GPIO_WritePin(profet->nDEN_Port, profet->nDEN_Pin, GPIO_PIN_SET);

    //Check for turn off
    if (profet->eReqState == OFF) {
      profet->eState = OFF;
    }

    //Overcurrent
    if ((profet->nIL > profet->nIL_Limit) &&
        !profet->bInRushActive){
      profet->nOC_TriggerTime = HAL_GetTick();
      profet->nOC_Count++;
      profet->eState = OVERCURRENT;
    }

    //Inrush overcurrent
    if ((profet->nIL > profet->nIL_InRushLimit) &&
        profet->bInRushActive){
      profet->nOC_TriggerTime = HAL_GetTick();
      profet->nOC_Count++;
      profet->eState = OVERCURRENT;
    }
    break;

  case OVERCURRENT:
    profet->cState = 'C';
    HAL_GPIO_WritePin(profet->nIN_Port, profet->nIN_Pin, GPIO_PIN_RESET);

    //No reset, straight to fault
    if(profet->eResetMode == RESET_NONE){
      profet->eState = FAULT;
    }

    //Overcurrent count exceeded
    if((profet->nOC_Count >= profet->nOC_ResetLimit) &&
        (profet->eResetMode == RESET_COUNT)){
      profet->eState = FAULT;
    }

    //Overcurrent reset time exceeded
    //RESET_ENDLESS or RESET_COUNT
    if((profet->nOC_ResetTime + profet->nOC_TriggerTime) < HAL_GetTick()){
      profet->nInRushOnTime = HAL_GetTick();
      profet->eState = ON;
    }

    //Check for turn off
    if (profet->eReqState == OFF) {
      profet->eState = OFF;
    }
    break;

  case FAULT:
    profet->cState = 'F';
    HAL_GPIO_WritePin(profet->nIN_Port, profet->nIN_Pin, GPIO_PIN_RESET);
    //Fault requires power cycle, no way out
    break;

  }
}

void Profet_UpdateIS(volatile ProfetTypeDef *profet, volatile uint16_t newVal, volatile float fVDDA)
{

  //Moving average without array or dividing
  //Store the new val, incase we need a non-filtered val elsewhere
  profet->nIS = newVal;
  //Add new value to old sum
  //profet->nIS_Sum += profet->nIS;
  //Shift sum by 1 which is equal to dividing by 2
  //profet->nIS_Avg = profet->nIS_Sum >> 1;
  //Remove the average from the sum, otherwise sum always goes up never down
  //profet->nIS_Sum -= profet->nIS_Avg;

  //BTS7002 - IS inaccurate below 4A
  //BTS7008 - IS fairly accurate across range

  //Calculate current at ADC, multiply by kILIS ratio to get output current
  //Use the measured VDDA value to calculate volts/step
  //IL = (rawVal * (VDDA / 4095)) / 1.2k) * kILIS
  uint16_t nNewIL = (uint16_t)((((float)newVal * (fVDDA / 4095)) / 1200) * profet->fKILIS);

  //Ignore current less than or equal to 0.2A
  //Not capable of measuring that low
  //Noise causes small blips in current when output is off
  if(nNewIL <= 2){
    profet->nIL = 0;
  }else{
    profet->nIL = nNewIL;
  }
}
