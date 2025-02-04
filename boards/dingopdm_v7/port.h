#pragma once

#include "hal.h"
#include "enums.h"

#define PDM_TYPE 0 //0 = PDM, 1 = PDM-MAX 

#define PDM_NUM_OUTPUTS 8
#define PDM_NUM_INPUTS 2
#define PDM_NUM_VIRT_INPUTS 16
#define PDM_NUM_CAN_INPUTS 32
#define PDM_NUM_FLASHERS 4
#define PDM_NUM_WIPER_INTER_DELAYS 6
#define PDM_NUM_WIPER_SPEED_MAP 8

#define PDM_VAR_MAP_SIZE 66

#define PDM_NUM_TX_MSGS 6

#define ADC1_NUM_CHANNELS 8
#define ADC1_BUF_DEPTH 1

#define BTS7002_1EPP_KILIS 229500
#define BTS7008_2EPA_KILIS 59500
#define BTS70012_1ESP_KILIS 350000

#define ENABLE_SLEEP 1
#define SLEEP_TIMEOUT 30000

#define SYS_TIME TIME_I2MS(chVTGetSystemTimeX())

enum class AnalogChannel
{
    IS1 = 0,
    IS2,
    IS3_4,
    IS5_6,
    IS7_8,
    BattVolt,
    TempSensor,
    VRefInt
};

enum class LedType
{
    Status,
    Error
};

const CANConfig &GetCanConfig(CanBitrate bitrate);

const I2CConfig i2cConfig = {
    OPMODE_I2C,
    400000,
    FAST_DUTY_CYCLE_2,
};

void InitAdc();
void DeInitAdc();
uint16_t GetAdcRaw(AnalogChannel channel);
float GetBattVolt();
float GetTemperature();
float GetVDDA();