/*
 * Copyright 2017-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c_freertos.h"
#include "fsl_mu.h"

#include "srtm_dispatcher.h"
#include "srtm_peercore.h"
#include "srtm_message.h"
#include "srtm_auto_service.h"
#include "srtm_rpmsg_endpoint.h"

#include "board.h"
#include "app_srtm.h"
#include "rsc_table.h"
#include "automotive.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef enum
{
    APP_SRTM_StateRun = 0x0U,
    APP_SRTM_StateLinkedUp,
    APP_SRTM_StateReboot,
    APP_SRTM_StateShutdown
} app_srtm_state_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static srtm_status_t APP_SRTM_Auto_RegisterEvent(srtm_auto_adapter_t adapter,
                                                 uint32_t clientId,
                                                 uint8_t *reqData,
                                                 uint32_t reqLen,
                                                 uint8_t *respData,
                                                 uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_UnregisterEvent(srtm_auto_adapter_t adapter,
                                                   uint32_t clientId,
                                                   uint8_t *reqData,
                                                   uint32_t reqLen,
                                                   uint8_t *respData,
                                                   uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_Control(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_PowerReport(srtm_auto_adapter_t adapter,
                                               uint32_t clientId,
                                               uint8_t *reqData,
                                               uint32_t reqLen,
                                               uint8_t *respData,
                                               uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_GetInfo(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen);
static srtm_status_t APP_SRTM_I2C_Read(srtm_i2c_adapter_t adapter,
                                       uint32_t base_addr,
                                       srtm_i2c_type_t type,
                                       uint16_t slaveAddr,
                                       uint8_t *buf,
                                       uint8_t len,
                                       uint16_t flags);
static srtm_status_t APP_SRTM_I2C_Write(srtm_i2c_adapter_t adapter,
                                        uint32_t base_addr,
                                        srtm_i2c_type_t type,
                                        uint16_t slaveAddr,
                                        uint8_t *buf,
                                        uint8_t len,
                                        uint16_t flags);
static void APP_SRTM_ResetServices(void);
/*******************************************************************************
 * Variables
 ******************************************************************************/
static srtm_dispatcher_t disp;
static srtm_peercore_t core;
static srtm_service_t autoService;
static SemaphoreHandle_t monSig;
static volatile app_srtm_state_t srtmState;
static struct rpmsg_lite_instance *rpmsgHandle;
static app_rpmsg_monitor_t rpmsgMonitor;
static void *rpmsgMonitorParam;
static TimerHandle_t linkupTimer;
static struct _srtm_auto_adapter autoAdapter = {.registerEvent   = APP_SRTM_Auto_RegisterEvent,
                                                .unregisterEvent = APP_SRTM_Auto_UnregisterEvent,
                                                .control         = APP_SRTM_Auto_Control,
                                                .powerReport     = APP_SRTM_Auto_PowerReport,
                                                .getInfo         = APP_SRTM_Auto_GetInfo};
static struct _i2c_bus qm_i2c_buses[] = {
    {.bus_id         = 1,
     .base_addr      = CM4_1__LPI2C_BASE,
     .type           = SRTM_I2C_TYPE_LPI2C,
     .switch_idx     = I2C_SWITCH_NONE,
     .switch_channel = SRTM_I2C_SWITCH_CHANNEL_UNSPECIFIED},
};
static struct _srtm_i2c_adapter qm_i2c_adapter = {.read          = APP_SRTM_I2C_Read,
                                                  .write         = APP_SRTM_I2C_Write,
                                                  .switchchannel = NULL,
                                                  .bus_structure = {
                                                      .buses      = qm_i2c_buses,
                                                      .bus_num    = sizeof(qm_i2c_buses) / sizeof(struct _i2c_bus),
                                                      .switches   = 0,
                                                      .switch_num = 0,
                                                  }};
static srtm_service_t i2cService;

static app_auto_register_handler_t appRegHandler;
static void *appRegParam;
/*******************************************************************************
 * Code
 ******************************************************************************/
static void APP_SRTM_SetRemoteReady(uint32_t ready)
{
    MU_SetFlagsNonBlocking(APP_KERNEL_MU, ready);
}

static srtm_status_t APP_SRTM_I2C_Write(srtm_i2c_adapter_t adapter,
                                        uint32_t base_addr,
                                        srtm_i2c_type_t type,
                                        uint16_t slaveAddr,
                                        uint8_t *buf,
                                        uint8_t len,
                                        uint16_t flags)
{
    status_t retVal = kStatus_Fail;
    uint8_t needStop;

    needStop = (flags & SRTM_I2C_FLAG_NEED_STOP) ? 1 : 0;

    switch (type)
    {
        case SRTM_I2C_TYPE_LPI2C:
            retVal = BOARD_LPI2C_SendWithoutSubAddr((LPI2C_Type *)base_addr, slaveAddr, buf, len, needStop);
            break;
        default:
            break;
    }
    return (retVal == kStatus_Success) ? SRTM_Status_Success : SRTM_Status_Error;
}

static srtm_status_t APP_SRTM_I2C_Read(srtm_i2c_adapter_t adapter,
                                       uint32_t base_addr,
                                       srtm_i2c_type_t type,
                                       uint16_t slaveAddr,
                                       uint8_t *buf,
                                       uint8_t len,
                                       uint16_t flags)
{
    status_t retVal = kStatus_Fail;
    uint8_t needStop;

    needStop = (flags & SRTM_I2C_FLAG_NEED_STOP) ? 1 : 0;

    switch (type)
    {
        case SRTM_I2C_TYPE_LPI2C:
            retVal = BOARD_LPI2C_ReceiveWithoutSubAddr((LPI2C_Type *)base_addr, slaveAddr, buf, len, needStop);
            break;
        default:
            break;
    }
    return (retVal == kStatus_Success) ? SRTM_Status_Success : SRTM_Status_Error;
}

static void APP_SRTM_PollLinkup(srtm_dispatcher_t dispatcher, void *param1, void *param2)
{
    if (srtmState == APP_SRTM_StateRun)
    {
        if (rpmsg_lite_is_link_up(rpmsgHandle))
        {
            /* A core has sent the kick, deassert the handshake. */
            APP_SRTM_SetRemoteReady(0);
            srtmState = APP_SRTM_StateLinkedUp;
            xSemaphoreGive(monSig);
        }
        else
        {
            /* Start timer to poll linkup status. */
            xTimerStart(linkupTimer, portMAX_DELAY);
        }
    }
}

static void APP_LinkupTimerCallback(TimerHandle_t xTimer)
{
    srtm_procedure_t proc = SRTM_Procedure_Create(APP_SRTM_PollLinkup, NULL, NULL);

    if (proc)
    {
        SRTM_Dispatcher_PostProc(disp, proc);
    }
}

static void APP_SRTM_NotifyPeerCoreReady(struct rpmsg_lite_instance *rpmsgHandle, bool ready)
{
    if (rpmsgMonitor)
    {
        rpmsgMonitor(rpmsgHandle, ready, rpmsgMonitorParam);
    }
}

static void APP_SRTM_Linkup(void)
{
    srtm_channel_t chan;
    srtm_rpmsg_endpoint_config_t rpmsgConfig;

    /* Create SRTM peer core */
    core = SRTM_PeerCore_Create(1U); /* Assign CortexA core ID to 1U */

    /* Set peer core state to activated */
    SRTM_PeerCore_SetState(core, SRTM_PeerCore_State_Activated);

    /* Common RPMsg channel config */
    rpmsgConfig.localAddr   = RL_ADDR_ANY;
    rpmsgConfig.peerAddr    = RL_ADDR_ANY;
    rpmsgConfig.rpmsgHandle = rpmsgHandle;

    /* Create and add SRTM AUTO channel to peer core */
    rpmsgConfig.epName = APP_SRTM_AUTO_CHANNEL_NAME;
    chan               = SRTM_RPMsgEndpoint_Create(&rpmsgConfig);
    SRTM_PeerCore_AddChannel(core, chan);

    /* Create and add SRTM I2C channel to peer core*/
    rpmsgConfig.epName = APP_SRTM_I2C_CHANNEL_NAME;
    chan               = SRTM_RPMsgEndpoint_Create(&rpmsgConfig);
    SRTM_PeerCore_AddChannel(core, chan);

    PRINTF("app_srtm: AUTO and I2C service registered\r\n");
    SRTM_Dispatcher_AddPeerCore(disp, core);
}

static void APP_SRTM_InitPeerCore(void)
{
    copyResourceTable();

    rpmsgHandle = rpmsg_lite_remote_init((void *)RPMSG_LITE_SRTM_SHMEM_BASE, RPMSG_LITE_SRTM_LINK_ID, RL_NO_FLAGS);
    assert(rpmsgHandle);

    /* SRTM will inform peer core that it is ready to receive the first kick. */
    APP_SRTM_SetRemoteReady(1);

    if (rpmsg_lite_is_link_up(rpmsgHandle))
    {
        /* If resume context has already linked up, don't need to announce channel again. */
        APP_SRTM_Linkup();
    }
    else
    {
        /* Start timer to poll linkup status. */
        xTimerStart(linkupTimer, portMAX_DELAY);
    }
}

static void APP_SRTM_ResetServices(void)
{
    SRTM_AutoService_Reset(autoService, core);
}

static void APP_SRTM_DeinitPeerCore(void)
{
    /* Stop linkupTimer if it's started. */
    xTimerStop(linkupTimer, portMAX_DELAY);

    if (core)
    {
        /* Notify application for the peer core disconnection. */
        APP_SRTM_NotifyPeerCoreReady(rpmsgHandle, false);
        /* Need to let services know peer core is now down. */
        APP_SRTM_ResetServices();

        SRTM_Dispatcher_RemovePeerCore(disp, core);
        SRTM_PeerCore_Destroy(core);
        core = NULL;
    }

    if (rpmsgHandle)
    {
        rpmsg_lite_deinit(rpmsgHandle);
        rpmsgHandle = NULL;
    }
}

static srtm_status_t APP_SRTM_Auto_RegisterEvent(srtm_auto_adapter_t adapter,
                                                 uint32_t clientId,
                                                 uint8_t *reqData,
                                                 uint32_t reqLen,
                                                 uint8_t *respData,
                                                 uint32_t respLen)
{
    srtm_status_t status;
    vehicle_state_t stateMsg;
    TaskHandle_t curTaskHandle;
    uint32_t ulNotifiedValue;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    if (appRegHandler)
        if (!appRegHandler(clientId, true, appRegParam, reqData, reqLen, respData, respLen))
            return SRTM_Status_Error;

    curTaskHandle = xTaskGetCurrentTaskHandle();
    stateMsg.type = STATE_TYPE_ANDROID;
    stateMsg.reg.enroll = 0x01;
    stateMsg.reg.handle = curTaskHandle;
    if (xQueueSend(xVStateQueue, (void *)&stateMsg, (TickType_t)0))
    {
        /* wait for response(current resource state) from other task*/
        xTaskNotifyWait( 0x00, 0xffffffff, &ulNotifiedValue, (TickType_t)10);
        respData[0] = (uint8_t)ulNotifiedValue;
        respData[1] = 0;
        PRINTF("Receive register command, partition=%d, respState=%d!\r\n", reqData[0], respData[0]);
        status = SRTM_Status_Success;
    }
    else
    {
        PRINTF("Receive register command, send to process queue failed!\r\n");
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_UnregisterEvent(srtm_auto_adapter_t adapter,
                                                   uint32_t clientId,
                                                   uint8_t *reqData,
                                                   uint32_t reqLen,
                                                   uint8_t *respData,
                                                   uint32_t respLen)
{
    vehicle_state_t stateMsg;
    srtm_status_t status;
    TaskHandle_t curTaskHandle;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    if (appRegHandler)
        if (!appRegHandler(clientId, false, appRegParam, reqData, reqLen, respData, respLen))
            return SRTM_Status_Error;

    curTaskHandle = xTaskGetCurrentTaskHandle();
    stateMsg.type = STATE_TYPE_ANDROID;
    stateMsg.reg.enroll = 0x00;
    stateMsg.reg.handle = curTaskHandle;
    if (xQueueSend(xVStateQueue, (void *)&stateMsg, (TickType_t)0))
    {
        status = SRTM_Status_Success;
        PRINTF("Unregister the client: %x with reason: %d successfully\r\n", clientId, reqData[0]);
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_Control(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen)
{
    int i;
    srtm_status_t status;
    control_unit_t ctrl;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    ctrl.type = (reqData[1] << 8) | reqData[0];
    ctrl.timeout = (reqData[5] << 24) | (reqData[4] << 16) | (reqData[3] << 8) | reqData[2];
    for (i=0; i<SRTM_USER_DATA_LENGTH - 6; i++)
        ctrl.value[i] = reqData[6+i];

    PRINTF("Receive control command, cmd=%x,to=%d,p[0]=%d,p[1]=%d,p[2]=%d,p[3]=%d,p[4]=%d\r\n",
           ctrl.type, ctrl.timeout, ctrl.value[0], ctrl.value[1],ctrl.value[2],
           ctrl.value[3],ctrl.value[4]);
    if (xQueueSend(xControlMsgQueue, (void *)&ctrl, (TickType_t)0))
    {
        status = SRTM_Status_Success;
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_PowerReport(srtm_auto_adapter_t adapter,
                                               uint32_t clientId,
                                               uint8_t *reqData,
                                               uint32_t reqLen,
                                               uint8_t *respData,
                                               uint32_t respLen)
{
    srtm_status_t status;
    ap_power_state_t ap_report;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    ap_report.powerState = (reqData[1] << 8) | reqData[0];
    ap_report.time_postpone = (reqData[3] << 24) | (reqData[2] << 16) | (reqData[1] << 8) | reqData[0];
    ap_report.is_request = 0;
    PRINTF("Receive power report, state=%x,time_postpone=%d\r\n", ap_report.powerState, ap_report.time_postpone);
    if (xQueueSend(xAPPowerQueue, (void *)&ap_report, (TickType_t)0))
    {
        status = SRTM_Status_Success;
        PRINTF("AP power report: %d\r\n", ap_report.powerState);
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_GetInfo(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen)
{
    uint16_t infoIndex;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    infoIndex = (reqData[1] << 8) | reqData[0];
    PRINTF("Receive get info, idx=%d\r\n", infoIndex);
    /* TODO: search the infoIndex data and fill the respData */
    if (infoIndex == VEHICLE_UNIQUE_ID)
    {
        respData[0] = reqData[0];
        respData[1] = reqData[1];
        respData[2] = 0x12;
        respData[3] = 0x34;
        respData[4] = 0x56;
        respData[5] = 0x78;
        respData[6] = 0xab;
        respData[7] = 0xcd;
        respData[8] = 0xef;
        respData[9] = 0xff;
    }

    return SRTM_Status_Success;
}

static void APP_SRTM_InitAutoService(void)
{
    /* Create and register service */
    autoService = SRTM_AutoService_Create(&autoAdapter);
    SRTM_Dispatcher_RegisterService(disp, autoService);
}

static void APP_SRTM_InitI2CDevice(void)
{
    lpi2c_master_config_t masterConfig;
    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Hz = APP_LPI2C_BAUDRATE;
    LPI2C_MasterInit(CM4_1__LPI2C, &masterConfig, CLOCK_GetIpFreq(kCLOCK_M4_1_Lpi2c));
}

static void APP_SRTM_DeinitI2CDevice(void)
{
    LPI2C_MasterDeinit(CM4_1__LPI2C);
}

static void APP_SRTM_InitI2CService(void)
{
    APP_SRTM_InitI2CDevice();
    i2cService = SRTM_I2CService_Create(&qm_i2c_adapter);
    SRTM_Dispatcher_RegisterService(disp, i2cService);
}

static void APP_SRTM_InitServices(void)
{
    APP_SRTM_InitI2CService();
    APP_SRTM_InitAutoService();
}

static void SRTM_MonitorTask(void *pvParameters)
{
    app_srtm_state_t state = APP_SRTM_StateShutdown;

    /* Initialize services and add to dispatcher */
    APP_SRTM_InitServices();

    /* Start SRTM dispatcher */
    SRTM_Dispatcher_Start(disp);

    /* Monitor peer core state change */
    while (true)
    {
        xSemaphoreTake(monSig, portMAX_DELAY);

        if (state == srtmState)
        {
            continue;
        }

        switch (srtmState)
        {
            case APP_SRTM_StateRun:
                assert(state == APP_SRTM_StateShutdown);
                SRTM_Dispatcher_Stop(disp);
                APP_SRTM_InitPeerCore();
                SRTM_Dispatcher_Start(disp);
                state = APP_SRTM_StateRun;
                break;

            case APP_SRTM_StateLinkedUp:
                if (state == APP_SRTM_StateRun)
                {
                    SRTM_Dispatcher_Stop(disp);

                    /* Need to announce channel as we just linked up. */
                    APP_SRTM_Linkup();
                    /* LinkedUp is still in Run state. Don't change state variable here. */

                    SRTM_Dispatcher_Start(disp);
                }
                break;

            case APP_SRTM_StateReboot:
                assert(state == APP_SRTM_StateRun);

                PRINTF("Handle Peer Core Reboot\r\n");
                SRTM_Dispatcher_Stop(disp);
                /* Remove peer core from dispatcher */
                APP_SRTM_DeinitPeerCore();

                /* Restore srtmState to Run. */
                srtmState = APP_SRTM_StateRun;

                /* Initialize peer core and add to dispatcher */
                APP_SRTM_InitPeerCore();
                SRTM_Dispatcher_Start(disp);

                /* Do not need to change state. It's still Run. */
                break;

            default:
                assert(false);
                break;
        }
    }
}

static void SRTM_DispatcherTask(void *pvParameters)
{
    SRTM_Dispatcher_Run(disp);
}

void APP_SRTM_Init(void)
{
    monSig = xSemaphoreCreateBinary();
    assert(monSig);

    linkupTimer =
        xTimerCreate("Linkup", APP_MS2TICK(APP_LINKUP_TIMER_PERIOD_MS), pdFALSE, NULL, APP_LinkupTimerCallback);
    assert(linkupTimer);

    /* Create SRTM dispatcher */
    disp = SRTM_Dispatcher_Create();

    xTaskCreate(SRTM_MonitorTask, "SRTM monitor", 256U, NULL, APP_SRTM_MONITOR_TASK_PRIO, NULL);
    xTaskCreate(SRTM_DispatcherTask, "SRTM dispatcher", 512U, NULL, APP_SRTM_DISPATCHER_TASK_PRIO, NULL);
}

void APP_SRTM_StartCommunication(void)
{
    srtmState = APP_SRTM_StateRun;
    xSemaphoreGive(monSig);
}

void APP_SRTM_Reboot(void)
{
    srtmState = APP_SRTM_StateReboot;
    xSemaphoreGive(monSig);
}

void APP_SRTM_SetAutoRegisterHandler(app_auto_register_handler_t handler, void *param)
{
    appRegHandler = handler;
    appRegParam   = param;
}

bool APP_SRTM_SendAutoCommand(uint32_t clientId,
                              srtm_auto_cmd_t cmd,
                              uint8_t *cmdParam,
                              uint32_t paramLen,
                              uint8_t *result,
                              uint32_t resultLen,
                              uint32_t timeout)
{
    assert(autoService);

    PRINTF("Send command, cmd=%x,p[0]=%d,p[1]=%d,p[2]=%d,p[3]=%d,p[4]=%d,p[5]=%d\r\n", cmd,
           cmdParam[0],cmdParam[1],cmdParam[2],cmdParam[3],cmdParam[4],cmdParam[5]);
    if (SRTM_AutoService_SendCommand(autoService, clientId, cmd, cmdParam, paramLen, result, resultLen, timeout) ==
        SRTM_Status_Success)
    {
        return true;
    }

    return false;
}

void APP_SRTM_PeerCoreRebootHandler(void)
{
    if (srtmState != APP_SRTM_StateShutdown)
    {
        srtmState = APP_SRTM_StateReboot;
        xSemaphoreGive(monSig);
    }
}

bool APP_SRTM_Suspend(void)
{
    APP_SRTM_DeinitI2CDevice();
    /* Here only simply return true. */
    return true;
}

void APP_SRTM_Resume(void)
{
    APP_SRTM_InitI2CDevice();
}

uint8_t APP_Read_I2C_Register(uint8_t busID, uint16_t slaveAddr, uint8_t regIndex)
{
    uint8_t value;
    SRTM_I2C_RequestBusWrite(i2cService, busID, slaveAddr, &regIndex, 1, 0);
    SRTM_I2C_RequestBusRead(i2cService, busID, slaveAddr, &value, 1);
    return value;
}

uint8_t APP_Write_I2C_Register(uint8_t busID, uint16_t slaveAddr, uint8_t regIndex, uint8_t value)
{
    uint8_t write_content[2];
    write_content[0] = regIndex;
    write_content[1] = value;
    SRTM_I2C_RequestBusWrite(i2cService, busID, slaveAddr, write_content, 2, 1);
    return value;
}
