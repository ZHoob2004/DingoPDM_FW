#pragma once

#include "hal.h"

//===============================================
// Output PWM period callbacks
//===============================================
static void pwmOut1pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF1_IN);
}

static void pwmOut2pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF2_IN);
}

static void pwmOut3pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF3_IN);
}

static void pwmOut4pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF4_IN);
}

static void pwmOut5pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF5_IN);
}

static void pwmOut6pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF6_IN);
}

static void pwmOut7pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF7_IN);
}

static void pwmOut8pcb(PWMDriver *pwmp)
{
    (void)pwmp;
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_PF8_IN);
}

//===============================================
// Output PWM duty cycle callbacks
//===============================================
static void pwmOut1cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF1_IN);
}
static void pwmOut2cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF2_IN);
}
static void pwmOut3cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF3_IN);
}
static void pwmOut4cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF4_IN);
}
static void pwmOut5cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF5_IN);
}
static void pwmOut6cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF6_IN);
}
static void pwmOut7cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF7_IN);
}
static void pwmOut8cb(PWMDriver *pwmp)
{
    (void)pwmp;
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_PF8_IN);
}

//===============================================
// Output PWM configurations
//===============================================
static const PWMConfig pwm3Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut1pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut1cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL} 
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm4Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut2pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut2cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL} 
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm5Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut3pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut3cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}  
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm9Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut4pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut4cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}  
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm10Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut5pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut5cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}  
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm11Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut6pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut6cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}  
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm12Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut7pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut7cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL} 
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm13Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmOut8pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut8cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL} 
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};