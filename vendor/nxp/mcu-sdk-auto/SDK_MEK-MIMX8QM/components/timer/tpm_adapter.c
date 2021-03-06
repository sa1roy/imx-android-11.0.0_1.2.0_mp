/*
 * Copyright 2018-2019 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_common.h"
#include "fsl_device_registers.h"
#include "timer.h"
#include "fsl_tpm.h"

#ifndef TIMER_ADAPTER_TPM1_ENABLE
#define TIMER_ADAPTER_TPM1_ENABLE (0)
#endif

#ifndef TIMER_ADAPTER_TPM2_ENABLE
#define TIMER_ADAPTER_TPM2_ENABLE (0)
#endif

typedef struct _hal_timer_handle_struct_t
{
    uint32_t timeout;
    uint32_t timerClock_Hz;
    hal_timer_callback_t callback;
    void *callbackParam;
#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U)) || \
    (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
    uint8_t instance;
#endif
} hal_timer_handle_struct_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/

static TPM_Type *const s_TPMBase[] = TPM_BASE_PTRS;
static IRQn_Type const mTPMIrqId[] = TPM_IRQS;

#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U)) || \
    (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
static hal_timer_handle_struct_t *s_halTimerState[sizeof(s_TPMBase) / sizeof(TPM_Type *)];
#else
static hal_timer_handle_struct_t *s_halTimerState[1];
#endif
/************************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
static void HAL_TimerInterruptHandle(uint8_t instance)
{
    TPM_ClearStatusFlags(s_TPMBase[instance], (uint32_t)kTPM_TimeOverflowFlag);
    if (s_halTimerState[instance]->callback != NULL)
    {
        s_halTimerState[instance]->callback(s_halTimerState[instance]->callbackParam);
    }
}
static uint8_t HAL_TimerGetInstance(hal_timer_handle_t halTimerHandle)
{
#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U)) || \
    (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
    hal_timer_handle_struct_t *halTimerState = halTimerHandle;
    return halTimerState->instance;
#endif
    return 0;
}

static void HAL_TimerHwInit(hal_timer_handle_t halTimerHandle)
{
    hal_timer_handle_struct_t *halTimerState = halTimerHandle;
    IRQn_Type irqId;
    tpm_config_t tpmInfo;
    uint8_t instance = 0;

    assert(halTimerHandle);
    TPM_GetDefaultConfig(&tpmInfo);
#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U)) || \
    (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
    instance = halTimerConfig->instance;
#endif
    /* Initialize TPM module */
    tpmInfo.prescale = kTPM_Prescale_Divide_128;
    TPM_Init(s_TPMBase[instance], (void *)&tpmInfo);
    TPM_StopTimer(s_TPMBase[instance]);

    /* Set the timer to be in free-running mode */
    s_TPMBase[instance]->MOD  = 0xFFFF;
    s_halTimerState[instance] = halTimerHandle;
    (void)HAL_TimerUpdateTimeout(halTimerHandle, halTimerState->timeout);

    /* Install ISR */
    irqId = mTPMIrqId[instance];
    TPM_EnableInterrupts(s_TPMBase[instance], (uint32_t)kTPM_TimeOverflowInterruptEnable);

    NVIC_SetPriority(irqId, HAL_TIMER_ISR_PRIORITY);
    (void)EnableIRQ(irqId);
}
void TPM0_IRQHandler(void);

void TPM0_IRQHandler(void)
{
    HAL_TimerInterruptHandle(0);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
   exception return operation might vector to incorrect interrupt */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}

#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U))
void TPM1_IRQHandler(void);
void TPM1_IRQHandler(void)
{
    HAL_TimerInterruptHandle(1);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
  exception return operation might vector to incorrect interrupt */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}
#endif /*TIMER_ADAPTER_TPM1_ENABLE*/

#if (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
void TPM2_IRQHandler(void);
void TPM2_IRQHandler(void)
{
    HAL_TimerInterruptHandle(2);
/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
 exception return operation might vector to incorrect interrupt */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}
#endif /*TIMER_ADAPTER_TPM2_ENABLE*/
/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
hal_timer_status_t HAL_TimerInit(hal_timer_handle_t halTimerHandle, hal_timer_config_t *halTimerConfig)
{
    hal_timer_handle_struct_t *halTimerState = halTimerHandle;
    tpm_config_t tpmInfo;

    assert(sizeof(hal_timer_handle_struct_t) <= HAL_TIMER_HANDLE_SIZE);
    assert(halTimerConfig);
    assert(halTimerHandle);
    assert(halTimerConfig->instance < (sizeof(s_TPMBase) / sizeof(TPM_Type *)));

    halTimerState->timeout = halTimerConfig->timeout;
#if (defined(TIMER_ADAPTER_TPM1_ENABLE) && (TIMER_ADAPTER_TPM1_ENABLE > 0U)) || \
    (defined(TIMER_ADAPTER_TPM2_ENABLE) && (TIMER_ADAPTER_TPM2_ENABLE > 0U))
    halTimerState->instance = halTimerConfig->instance;
    assert(s_TPMBase[halTimerState->instance]);
#endif
    /* TPM clock divide by 128 */
    tpmInfo.prescale             = kTPM_Prescale_Divide_128;
    halTimerState->timerClock_Hz = halTimerConfig->srcClock_Hz / (1UL << (uint8_t)tpmInfo.prescale);

    HAL_TimerHwInit(halTimerHandle);
    return kStatus_HAL_TimerSuccess;
}
void HAL_TimerDeinit(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);
    assert(halTimerHandle);

    s_halTimerState[HAL_TimerGetInstance(halTimerHandle)] = NULL;
    TPM_Deinit(s_TPMBase[HAL_TimerGetInstance(halTimerHandle)]);
}
/*************************************************************************************/
void HAL_TimerEnable(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);
    TPM_StartTimer(s_TPMBase[HAL_TimerGetInstance(halTimerHandle)], kTPM_SystemClock);
}

/*************************************************************************************/
void HAL_TimerDisable(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);
    TPM_StopTimer(s_TPMBase[HAL_TimerGetInstance(halTimerHandle)]);
}

/*************************************************************************************/
void HAL_TimerInstallCallback(hal_timer_handle_t halTimerHandle, hal_timer_callback_t callback, void *callbackParam)
{
    assert(halTimerHandle);
    s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->callback      = callback;
    s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->callbackParam = callbackParam;
}
uint32_t HAL_TimerGetMaxTimeout(hal_timer_handle_t halTimerHandle)
{
    uint32_t reserveCount;
    assert(halTimerHandle);

    reserveCount = (uint32_t)MSEC_TO_COUNT((4), (s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timerClock_Hz));
    if (reserveCount < MSEC_TO_COUNT((1), (s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timerClock_Hz)))
    {
        return 1000;
    }
    return (uint32_t)COUNT_TO_USEC((0xFFFFUL - (uint64_t)reserveCount),
                                   (s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timerClock_Hz));
}
/* return micro us */
uint32_t HAL_TimerGetCurrentTimerCount(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);

    return (uint32_t)COUNT_TO_USEC((TPM_GetCurrentTimerCount(s_TPMBase[HAL_TimerGetInstance(halTimerHandle)])),
                                   (s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timerClock_Hz));
}

hal_timer_status_t HAL_TimerUpdateTimeout(hal_timer_handle_t halTimerHandle, uint32_t timeout)
{
    uint32_t tickCount;
    assert(halTimerHandle);

    s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timeout = timeout;
    tickCount = (uint32_t)USEC_TO_COUNT((s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timeout),
                                        (s_halTimerState[HAL_TimerGetInstance(halTimerHandle)]->timerClock_Hz));
    if ((tickCount < 1U) || (tickCount > 0xfff0U))
    {
        return kStatus_HAL_TimerOutOfRanger;
    }

    s_TPMBase[HAL_TimerGetInstance(halTimerHandle)]->CNT = 0;
    TPM_SetTimerPeriod(s_TPMBase[HAL_TimerGetInstance(halTimerHandle)], tickCount);
    return kStatus_HAL_TimerSuccess;
}

void HAL_TimerExitLowpower(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);
    HAL_TimerHwInit(halTimerHandle);
}

void HAL_TimerEnterLowpower(hal_timer_handle_t halTimerHandle)
{
    assert(halTimerHandle);
}
