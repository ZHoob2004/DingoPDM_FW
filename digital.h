#pragma once

#include "port.h"
#include "enums.h"
#include "config.h"
#include "input.h"

class Digital
{
public:
    Digital(ioline_t line)
        : m_line(line)
    {};

    uint16_t nVal;

    void Update();

    void SetConfig(Config_Input *config)
    {
        pConfig = config;

        SetPull(config->ePull);
    }

private:
    const ioline_t m_line;

    void SetPull(InputPull pull);

    Config_Input *pConfig;

    Input input;

    bool bInit;
    bool bLast;
    bool bCheck;
    uint32_t nLastTrigTime;
};