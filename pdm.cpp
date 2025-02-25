#include "pdm.h"
#include "ch.hpp"
#include "hal.h"
#include "port.h"
#include "mcu_utils.h"
#include "dingopdm_config.h"
#include "config.h"
#include "config_handler.h"
#include "hw_devices.h"
#include "can.h"
#include "can_input.h"
#include "virtual_input.h"
#include "wiper.h"
#include "starter.h"
#include "flasher.h"
#include "counter.h"
#include "condition.h"
#include "mailbox.h"
#include "msg.h"
#include "error.h"
#include "usb.h"

CanInput canIn[PDM_NUM_CAN_INPUTS];
VirtualInput virtIn[PDM_NUM_VIRT_INPUTS];
Wiper wiper;
Starter starter;
Flasher flasher[PDM_NUM_FLASHERS];
Counter counter[PDM_NUM_COUNTERS];
Condition condition[PDM_NUM_CONDITIONS];

PdmState eState = PdmState::Run;
FatalErrorType eError = FatalErrorType::NoError;

PdmConfig stConfig;
uint16_t *pVarMap[PDM_VAR_MAP_SIZE];

uint16_t nAlwaysFalse = 0;
uint16_t nAlwaysTrue = 1;
float fBattVolt;
float fTempSensor;
bool bDeviceOverTemp;
bool bDeviceCriticalTemp;
bool bSleepRequest;
bool bBootloaderRequest;

uint8_t nNumOutputsOn;
uint8_t nLastNumOutputsOn;
uint32_t nAllOutputsOffTime;

void InitVarMap();
void ApplyAllConfig();
void ApplyConfig(MsgCmd eCmd);
void CyclicUpdate();
void States();
void SendInfoMsgs();
void InitInfoMsgs();
void CheckRequestMsgs(CANRxFrame *frame);
bool GetAnyOvercurrent();
bool GetAnyFault();
bool CheckEnterSleep();

static InfoMsg StateRunMsg(MsgType::Info, MsgSrc::State_Run);
static InfoMsg StateSleepMsg(MsgType::Info, MsgSrc::State_Sleep);
static InfoMsg StateOvertempMsg(MsgType::Error, MsgSrc::State_Overtemp);
static InfoMsg StateErrorMsg(MsgType::Error, MsgSrc::State_Error);

static InfoMsg BattOvervoltageMsg(MsgType::Warning, MsgSrc::Voltage);
static InfoMsg BattUndervoltageMsg(MsgType::Warning, MsgSrc::Voltage);

InfoMsg OutputOvercurrentMsg[PDM_NUM_OUTPUTS];
InfoMsg OutputFaultMsg[PDM_NUM_OUTPUTS];

struct PdmThread : chibios_rt::BaseStaticThread<2048>
{
    void main()
    {
        setName("PdmThread");

        while (true)
        {
            CyclicUpdate();
            States();
            palToggleLine(LINE_E1);
            chThdSleepMilliseconds(2);
        }
    }
};
static PdmThread pdmThread;

struct SlowThread : chibios_rt::BaseStaticThread<256>
{
    void main()
    {
        setName("SlowThread");

        uint8_t dc = 50;
        while (true)
        {
            //=================================================================
            // Perform tasks that don't need to be done every cycle here
            //=================================================================
            if (chThdShouldTerminateX())
                chThdExit(MSG_OK);

            fBattVolt = GetBattVolt();

            // Temp sensor is I2C, takes a while to read
            // Don't want to slow down main thread
            fTempSensor = tempSensor.GetTemp();
            bDeviceOverTemp = tempSensor.OverTempLimit();
            bDeviceCriticalTemp = tempSensor.CritTempLimit();

            for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
            {
                pf[i].SetDutyCycle(dc);
                dc += 1;
                if (dc > 100)
                    dc = 10;
            }
            // palToggleLine(LINE_E2);
            chThdSleepMilliseconds(250);
        }
    }
};
static SlowThread slowThread;
static chibios_rt::ThreadReference slowThreadRef;

void InitPdm()
{
    Error::Initialize(&statusLed, &errorLed);

    InitVarMap(); // Set val pointers

    if (!i2cStart(&I2CD1, &i2cConfig) == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrI2C, MsgSrc::Init);

    InitConfig(); // Read config from FRAM

    ApplyAllConfig();

    InitAdc();
    InitCan(stConfig.stDevConfig.eCanSpeed); // Starts CAN threads
    // InitUsb(); // Starts USB threads

    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
        pf[i].SetDutyCycle(0);

    if (!InitPwm() == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrPwm, MsgSrc::Init);

    //*******REMOVE */
    pf[0].SetDutyCycle(50);
    pf[1].SetDutyCycle(50);
    pf[2].SetDutyCycle(50);
    pf[3].SetDutyCycle(50);
    pf[4].SetDutyCycle(50);
    pf[5].SetDutyCycle(50);
    pf[6].SetDutyCycle(50);
    pf[7].SetDutyCycle(50);

    stConfig.stOutput[0].bPwmEnabled = true;
    stConfig.stOutput[1].bPwmEnabled = true;
    stConfig.stOutput[2].bPwmEnabled = true;
    stConfig.stOutput[3].bPwmEnabled = true;
    stConfig.stOutput[4].bPwmEnabled = true;
    stConfig.stOutput[5].bPwmEnabled = true;
    stConfig.stOutput[6].bPwmEnabled = true;
    stConfig.stOutput[7].bPwmEnabled = true;

    //*******END REMOVE */

    if (!tempSensor.Init(BOARD_TEMP_WARN, BOARD_TEMP_CRIT))
        Error::SetFatalError(FatalErrorType::ErrTempSensor, MsgSrc::Init);

    InitInfoMsgs();

    palClearLine(LINE_CAN_STANDBY); // CAN enabled

    slowThreadRef = slowThread.start(NORMALPRIO);
    pdmThread.start(NORMALPRIO);
}

void States()
{

    switch (eState)
    {

    case PdmState::Run:

        if (bDeviceCriticalTemp)
            Error::SetFatalError(FatalErrorType::ErrTemp, MsgSrc::State_Overtemp);

        if (bDeviceOverTemp)
            eState = PdmState::OverTemp;

        if (GetAnyOvercurrent() && !GetAnyFault())
        {
            statusLed.Blink();
            errorLed.Solid(false);
        }

        if (GetAnyFault())
        {
            statusLed.Blink();
            errorLed.Solid(true);
        }

        if (!GetAnyOvercurrent() && !GetAnyFault())
        {
            statusLed.Solid(true);
            errorLed.Solid(false);
        }

        if (CheckEnterSleep())
        {
            statusLed.Solid(false);
            errorLed.Solid(false);
            eState = PdmState::Sleep;
        }

        break;

    case PdmState::Sleep:
        bSleepRequest = false;
        palSetLine(LINE_CAN_STANDBY); // CAN disabled
        EnterStopMode();
        break;

    case PdmState::OverTemp:
        statusLed.Blink();
        errorLed.Blink();

        if (bDeviceCriticalTemp)
            Error::SetFatalError(FatalErrorType::ErrTemp, MsgSrc::State_Overtemp);

        if (!bDeviceOverTemp)
            eState = PdmState::Run;

        break;

    case PdmState::Error:
        // Not required?
        Error::SetFatalError(eError, MsgSrc::State_Error);
        break;
    }

    SendInfoMsgs();
}

void CyclicUpdate()
{
    CANRxFrame rxMsg;

    while (!RxFramesEmpty())
    {
        msg_t res = FetchRxFrame(&rxMsg);
        if (res == MSG_OK)
        {
            for (uint8_t i = 0; i < PDM_NUM_CAN_INPUTS; i++)
                canIn[i].CheckMsg(rxMsg);

            CheckRequestMsgs(&rxMsg);
            ApplyConfig(ConfigHandler(&rxMsg));
        }
    }

    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
        pf[i].Update((eState == PdmState::Run) && starter.nVal[i]);

    for (uint8_t i = 0; i < PDM_NUM_INPUTS; i++)
        in[i].Update();

    for (uint8_t i = 0; i < PDM_NUM_CAN_INPUTS; i++)
        canIn[i].CheckTimeout();

    for (uint8_t i = 0; i < PDM_NUM_VIRT_INPUTS; i++)
        virtIn[i].Update();

    wiper.Update(SYS_TIME);

    starter.Update();

    for (uint8_t i = 0; i < PDM_NUM_FLASHERS; i++)
        flasher[i].Update(SYS_TIME);

    for (uint8_t i = 0; i < PDM_NUM_COUNTERS; i++)
        counter[i].Update();

    for (uint8_t i = 0; i < PDM_NUM_CONDITIONS; i++)
        condition[i].Update();
}

void InitVarMap()
{
    // 0
    // Always false
    pVarMap[0] = &nAlwaysFalse;

    // 1-2
    // Digital inputs
    pVarMap[1] = &in[0].nVal;
    pVarMap[2] = &in[1].nVal;

    // 3-34
    // CAN Inputs
    for (uint8_t i = 0; i < PDM_NUM_CAN_INPUTS; i++)
    {
        pVarMap[i + 3] = &canIn[i].nOutput;
    }

    // 35-66
    // CAN Input val
    for (uint8_t i = 0; i < PDM_NUM_CAN_INPUTS; i++)
    {
        pVarMap[i + 35] = &canIn[i].nOutput;
    }

    // 67-84
    // Virtual Inputs
    for (uint8_t i = 0; i < PDM_NUM_VIRT_INPUTS; i++)
    {
        pVarMap[i + 67] = &virtIn[i].nVal;
    }

    // 83-90
    // Outputs
    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        pVarMap[i + 83] = &pf[i].nOutput;
    }

    // 91-92
    // Wiper
    pVarMap[91] = &wiper.nSlowOut;
    pVarMap[92] = &wiper.nFastOut;

    // 93-96
    // Flashers
    for (uint8_t i = 0; i < PDM_NUM_FLASHERS; i++)
    {
        pVarMap[i + 93] = &flasher[i].nVal;
    }

    // 97-100
    // Counters
    for (uint8_t i = 0; i < PDM_NUM_COUNTERS; i++)
    {
        pVarMap[i + 97] = &counter[i].nVal;
    }

    // 101 - 132
    // Conditions
    for (uint8_t i = 0; i < PDM_NUM_CONDITIONS; i++)
    {
        pVarMap[i + 101] = &condition[i].nVal;
    }

    // 133
    // Always true
    pVarMap[133] = &nAlwaysTrue;
}

void ApplyAllConfig()
{
    ApplyConfig(MsgCmd::Inputs);
    ApplyConfig(MsgCmd::CanInputs);
    ApplyConfig(MsgCmd::VirtualInputs);
    ApplyConfig(MsgCmd::Outputs);
    ApplyConfig(MsgCmd::Wiper);
    ApplyConfig(MsgCmd::StarterDisable);
    ApplyConfig(MsgCmd::Flashers);
    ApplyConfig(MsgCmd::Counters);
    ApplyConfig(MsgCmd::Conditions);
}

void ApplyConfig(MsgCmd eCmd)
{
    if (eCmd == MsgCmd::Can)
    {
        // TODO: Change CAN speed
    }

    if (eCmd == MsgCmd::Inputs)
    {
        for (uint8_t i = 0; i < PDM_NUM_INPUTS; i++)
            in[i].SetConfig(&stConfig.stInput[i]);
    }

    if ((eCmd == MsgCmd::CanInputs) || (eCmd == MsgCmd::CanInputsId))
    {
        for (uint8_t i = 0; i < PDM_NUM_CAN_INPUTS; i++)
            canIn[i].SetConfig(&stConfig.stCanInput[i]);
    }

    if (eCmd == MsgCmd::VirtualInputs)
    {
        for (uint8_t i = 0; i < PDM_NUM_VIRT_INPUTS; i++)
            virtIn[i].SetConfig(&stConfig.stVirtualInput[i], pVarMap);
    }

    if (eCmd == MsgCmd::Outputs)
    {
        for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
            pf[i].SetConfig(&stConfig.stOutput[i], pVarMap);
    }

    if ((eCmd == MsgCmd::Wiper) || (eCmd == MsgCmd::WiperSpeed) || (eCmd == MsgCmd::WiperDelays))
    {
        wiper.SetConfig(&stConfig.stWiper, pVarMap);
    }

    if (eCmd == MsgCmd::StarterDisable)
    {
        starter.SetConfig(&stConfig.stStarter, pVarMap);
    }

    if (eCmd == MsgCmd::Flashers)
    {
        for (uint8_t i = 0; i < PDM_NUM_FLASHERS; i++)
            flasher[i].SetConfig(&stConfig.stFlasher[i], pVarMap);
    }

    if (eCmd == MsgCmd::Counters)
    {
        for (uint8_t i = 0; i < PDM_NUM_COUNTERS; i++)
            counter[i].SetConfig(&stConfig.stCounter[i], pVarMap);
    }

    if (eCmd == MsgCmd::Conditions)
    {
        for (uint8_t i = 0; i < PDM_NUM_CONDITIONS; i++)
            condition[i].SetConfig(&stConfig.stCondition[i], pVarMap);
    }
}

void CheckRequestMsgs(CANRxFrame *frame)
{
    // Check for sleep request
    if (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Sleep))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stCanOutput.nBaseId + TX_SETTINGS_ID_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 2;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Sleep) + 128;
        txMsg.data8[1] = 1;

        PostTxFrame(&txMsg);

        bSleepRequest = true;
    }

    // Check for burn request
    if (frame->data8[0] == static_cast<uint8_t>(MsgCmd::BurnSettings))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stCanOutput.nBaseId + TX_SETTINGS_ID_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 2;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::BurnSettings) + 128;
        txMsg.data8[1] = WriteConfig();

        PostTxFrame(&txMsg);
    }

    // Check for bootloader request
    if (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Bootloader))
    {
        RequestBootloader();
    }

    // Check for version request
    if (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Version))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stCanOutput.nBaseId + TX_SETTINGS_ID_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 5;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Version) + 128;
        txMsg.data8[1] = MAJOR_VERSION;
        txMsg.data8[2] = MINOR_VERSION;
        txMsg.data8[3] = BUILD >> 8;
        txMsg.data8[4] = BUILD & 0xFF;

        PostTxFrame(&txMsg);
    }
}

void SendInfoMsgs()
{
    StateRunMsg.Check(eState == PdmState::Run, stConfig.stCanOutput.nBaseId, 0, 0, 0);
    StateSleepMsg.Check(eState == PdmState::Sleep, stConfig.stCanOutput.nBaseId, 0, 0, 0);
    StateOvertempMsg.Check(eState == PdmState::OverTemp, stConfig.stCanOutput.nBaseId, GetBoardTemp() * 10, 0, 0);
    StateErrorMsg.Check(eState == PdmState::Error, stConfig.stCanOutput.nBaseId, GetBoardTemp() * 10, GetTotalCurrent() * 10, 0);

    BattOvervoltageMsg.Check(fBattVolt > BATT_HIGH_VOLT, stConfig.stCanOutput.nBaseId, fBattVolt * 10, 0, 0);
    BattUndervoltageMsg.Check(fBattVolt < BATT_LOW_VOLT, stConfig.stCanOutput.nBaseId, fBattVolt * 10, 0, 0);

    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        OutputOvercurrentMsg[i].Check(pf[i].GetState() == ProfetState::Overcurrent, stConfig.stCanOutput.nBaseId, i, pf[i].GetCurrent(), 0);
        OutputFaultMsg[i].Check(pf[i].GetState() == ProfetState::Fault, stConfig.stCanOutput.nBaseId, i, pf[i].GetCurrent(), 0);
    }
}

void InitInfoMsgs()
{
    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        OutputOvercurrentMsg[i] = InfoMsg(MsgType::Warning, MsgSrc::Overcurrent);
        OutputFaultMsg[i] = InfoMsg(MsgType::Error, MsgSrc::Overcurrent);
    }
}

PdmState GetPdmState()
{
    return eState;
}

float GetBoardTemp()
{
    return fTempSensor;
}

float GetTotalCurrent()
{
    float fTotalCurrent = 0.0;

    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        fTotalCurrent += pf[i].GetCurrent();
    }

    return fTotalCurrent;
}

bool GetAnyOvercurrent()
{
    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        if (pf[i].GetState() == ProfetState::Overcurrent)
        {
            // TODO: Send overcurrent message
            return true;
        }
    }

    return false;
}

bool GetAnyFault()
{
    for (uint8_t i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        if (pf[i].GetState() == ProfetState::Fault)
        {
            // TODO: Send fault message
            return true;
        }
    }

    return false;
}

bool GetInputVal(uint8_t nInput)
{
    if (nInput >= PDM_NUM_INPUTS)
        return false;

    return in[nInput].nVal;
}

uint16_t GetOutputCurrent(uint8_t nOutput)
{
    if (nOutput >= PDM_NUM_OUTPUTS)
        return 0;

    return pf[nOutput].GetCurrent();
}

ProfetState GetOutputState(uint8_t nOutput)
{
    if (nOutput >= PDM_NUM_OUTPUTS)
        return ProfetState::Off;

    return pf[nOutput].GetState();
}

uint8_t GetOutputOcCount(uint8_t nOutput)
{
    if (nOutput >= PDM_NUM_OUTPUTS)
        return 0;

    return pf[nOutput].GetOcCount();
}

bool GetCanInVal(uint8_t nInput)
{
    if (nInput >= PDM_NUM_CAN_INPUTS)
        return false;

    return canIn[nInput].nVal;
}

bool GetVirtInVal(uint8_t nInput)
{
    if (nInput >= PDM_NUM_VIRT_INPUTS)
        return false;

    return virtIn[nInput].nVal;
}

bool GetWiperFastOut()
{
    return wiper.nFastOut;
}

bool GetWiperSlowOut()
{
    return wiper.nSlowOut;
}

WiperState GetWiperState()
{
    return wiper.GetState();
}

WiperSpeed GetWiperSpeed()
{
    return wiper.GetSpeed();
}

bool GetFlasherVal(uint8_t nFlasher)
{
    if (nFlasher >= PDM_NUM_FLASHERS)
        return false;

    return flasher[nFlasher].nVal;
}

uint16_t GetCounterVal(uint8_t nCounter)
{
    if (nCounter >= PDM_NUM_COUNTERS)
        return 0;

    return counter[nCounter].nVal;
}

bool GetConditionVal(uint8_t nCondition)
{
    if (nCondition >= PDM_NUM_CONDITIONS)
        return false;

    return condition[nCondition].nVal;
}

bool CheckEnterSleep()
{
    bool bEnterSleep = false;

    // Count number of outputs on
    nNumOutputsOn = 0;
    for (int i = 0; i < PDM_NUM_OUTPUTS; i++)
    {
        if (pf[i].GetState() != ProfetState::Off)
            nNumOutputsOn++;
    }

    // All outputs just turned off, save time
    if ((nNumOutputsOn == 0) && (nLastNumOutputsOn > 0))
    {
        nAllOutputsOffTime = SYS_TIME;
    }
    nLastNumOutputsOn = nNumOutputsOn;

    // Had issue with SYS_TIME being < GetLastCanRxTime when msgs come in quickly
    // Check that SYS_TIME is greater than or equal to GetLastCanRxTime
    uint32_t sys = SYS_TIME;
    uint32_t canLast = GetLastCanRxTime();
    uint32_t nCanRxIdleTime;
    if (sys >= canLast)
        nCanRxIdleTime = sys - canLast;
    else
        nCanRxIdleTime = 0;

    // No outputs on, no CAN msgs received and no USB connected
    // Go to sleep after timeout
    bEnterSleep = ENABLE_SLEEP &&
                  (nNumOutputsOn == 0) &&
                  (nLastNumOutputsOn == 0) &&
                  !GetUsbConnected() &&
                  ((SYS_TIME - nAllOutputsOffTime) > SLEEP_TIMEOUT) &&
                  (nCanRxIdleTime > SLEEP_TIMEOUT);

    if (nCanRxIdleTime > SLEEP_TIMEOUT)
    {
        nCanRxIdleTime = SYS_TIME - GetLastCanRxTime();
    }

    return bEnterSleep || bSleepRequest;
}
