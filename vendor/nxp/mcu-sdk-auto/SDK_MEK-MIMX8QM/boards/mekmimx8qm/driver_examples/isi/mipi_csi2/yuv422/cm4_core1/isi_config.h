/*
 * Copyright 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _ISI_CONFIG_H_
#define _ISI_CONFIG_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*${macro:start}*/
/* Whether use external convertor such as MIPI2HDMI convertor (ADV7535) */
#ifndef APP_DISPLAY_EXTERNAL_CONVERTOR
#define APP_DISPLAY_EXTERNAL_CONVERTOR 0
#endif

/* Use the MIPI DSI interface. */
#define DPU_EXAMPLE_DI DPU_DI_MIPI

#define CAMERA_DEVICE CAMERA_DEVICE_OV5640
/*${macro:end}*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*${prototype:start}*/
/*${prototype:end}*/
#endif /* _ISI_CONFIG_H_ */
