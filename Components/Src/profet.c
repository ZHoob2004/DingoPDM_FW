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
  if ((profet->nIS_Avg > 30000) &&
      profet->bIS_Active){
    profet->eState = FAULT;
  }

  //IS is used, otherwise ST is used
  profet->bIS_Active = (  (profet->eModel == BTS7002_1EPP) ||
                          (profet->eModel == BTS7008_2EPA_CH1) ||
                          (profet->eModel == BTS7008_2EPA_CH2));

  switch (profet->eState) {

  case OFF:
    profet->cState = 'O';

    HAL_GPIO_WritePin(profet->nIN_Port, profet->nIN_Pin, GPIO_PIN_RESET);
    if(profet->bIS_Active){
      HAL_GPIO_WritePin(profet->nDEN_Port, profet->nDEN_Pin, GPIO_PIN_RESET);
    }
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
    if(profet->bIS_Active){
      //Turn DEN on = activates IS
      HAL_GPIO_WritePin(profet->nDEN_Port, profet->nDEN_Pin, GPIO_PIN_SET);
    }

    //Check for turn off
    if (profet->eReqState == OFF) {
      profet->eState = OFF;
    }

    //Non-IS Profet, use ST (status input)
    //Inrush ignored
    if ((!profet->bST) &&
        (!profet->bInRushActive) &&
        (!profet->bIS_Active)){
      profet->nOC_TriggerTime = HAL_GetTick();
      profet->nOC_Count++;
      profet->eState = OVERCURRENT;
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

void Profet_UpdateIS(volatile ProfetTypeDef *profet, uint16_t newVal)
{
  //Profet without IS
  if( (profet->eModel == BTS724_CH1) || (profet->eModel == BTS724_CH1) ||
      (profet->eModel == BTS724_CH1) || (profet->eModel == BTS724_CH1)){
    return;
  }

  //Moving average without array or dividing
  //Store the new val, incase we need a non-filtered val elsewhere
  //profet->nIS = newVal;
  //Add new value to old sum
  //profet->nIS_Sum += profet->nIS;
  //Shift sum by 1 which is equal to dividing by 2
  //profet->nIS_Avg = profet->nIS_Sum >> 1;
  //Remove the average from the sum, otherwise sum always goes up never down
  //profet->nIS_Sum -= profet->nIS_Avg;

  //Convert IS to IL (actual current)
  //profet->nIL = (uint16_t)(((float)newVal * profet->fM) + profet->fB);

  profet->nIL = (uint16_t)((((float)newVal * 0.000805664) / 1200) * profet->fKILIS);

  //Ignore current readings below low threshold
  //if((profet->eModel == BTS7002_1EPP) && (profet->nIL < 11)){
  //  profet->nIL = 0;
 // }

  //if(((profet->eModel == BTS7008_2EPA_CH1) || (profet->eModel == BTS7008_2EPA_CH1))
  //    && (profet->nIL < 1)){
  //    profet->nIL = 0;
  //}
}
