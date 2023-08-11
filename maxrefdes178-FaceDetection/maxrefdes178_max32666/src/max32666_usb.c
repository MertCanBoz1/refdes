/*******************************************************************************
 * Copyright (C) 2020 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 ******************************************************************************/


//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "max32666_usb_descriptors.h"
#include <stdint.h>
#include <cdc_acm.h>
#include <enumerate.h>
#include <gcr_regs.h>
#include <mxc_delay.h>
#include <mxc_errors.h>
#include <stddef.h>
#include <stdio.h>
#include <usb.h>
#include <usb_event.h>

#include "max32666_debug.h"
#include "max32666_usb.h"


//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define S_MODULE_NAME   "usb"

#define EVENT_ENUM_COMP     MAXUSB_NUM_EVENTS
#define EVENT_REMOTE_WAKE   (EVENT_ENUM_COMP + 1)

#define BUFFER_SIZE  64


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
volatile int configured;
volatile int suspended;
volatile unsigned int event_flags;
int remote_wake_en;

/* This EP assignment must match the Configuration Descriptor */
static const acm_cfg_t acm_cfg = {
    1,                  /* EP OUT */
    MXC_USBHS_MAX_PACKET, /* OUT max packet size */
    2,                  /* EP IN */
    MXC_USBHS_MAX_PACKET, /* IN max packet size */
    3,                  /* EP Notify */
    MXC_USBHS_MAX_PACKET, /* Notify max packet size */
};

static volatile int usb_read_complete;


//-----------------------------------------------------------------------------
// Local function declarations
//-----------------------------------------------------------------------------
int usbStartupCallback();
int usbShutdownCallback();
static void usbAppSleep(void);
static void usbAppWakeup(void);
static int setconfigCallback(MXC_USB_SetupPkt* sud, void* cbdata);
static int setfeatureCallback(MXC_USB_SetupPkt* sud, void* cbdata);
static int clrfeatureCallback(MXC_USB_SetupPkt* sud, void* cbdata);
static int eventCallback(maxusb_event_t evt, void* data);
static int usbReadCallback(void);
static void echoUSB(void);
static void delay_us(unsigned int usec);


//-----------------------------------------------------------------------------
// Function definitions
//-----------------------------------------------------------------------------
void USB_IRQHandler(void)
{
    MXC_USB_EventHandler();
}

int usb_init(void)
{
    int ret;
    maxusb_cfg_options_t usb_opts;

    /* Initialize state */
    configured = 0;
    suspended = 0;
    event_flags = 0;
    remote_wake_en = 0;

    /* Start out in full speed */
    usb_opts.enable_hs = 0;
    usb_opts.delay_us = delay_us; /* Function which will be used for delays */
    usb_opts.init_callback = usbStartupCallback;
    usb_opts.shutdown_callback = usbShutdownCallback;

    /* Initialize the usb module */
    ret = MXC_USB_Init(&usb_opts);
    if (ret != E_NO_ERROR) {
        PR_ERROR("MXC_USB_Init failed %d", ret);
        return ret;
    }

    /* Initialize the enumeration module */
    ret = enum_init();
    if (ret != E_NO_ERROR) {
        PR_ERROR("enum_init failed %d", ret);
        return ret;
    }

    /* Register enumeration data */
    enum_register_descriptor(ENUM_DESC_DEVICE, (uint8_t*) &device_descriptor, 0);
    enum_register_descriptor(ENUM_DESC_CONFIG, (uint8_t*) &config_descriptor, 0);
    enum_register_descriptor(ENUM_DESC_STRING, lang_id_desc, 0);
    enum_register_descriptor(ENUM_DESC_STRING, mfg_id_desc, 1);
    enum_register_descriptor(ENUM_DESC_STRING, prod_id_desc, 2);

    /* Handle configuration */
    enum_register_callback(ENUM_SETCONFIG, setconfigCallback, NULL);

    /* Handle feature set/clear */
    enum_register_callback(ENUM_SETFEATURE, setfeatureCallback, NULL);
    enum_register_callback(ENUM_CLRFEATURE, clrfeatureCallback, NULL);

    /* Initialize the class driver */
    ret = acm_init(&config_descriptor.comm_interface_descriptor);
    if (ret != E_NO_ERROR) {
        printf("acm_init failed %d", ret);
        return ret;
    }

    /* Register callbacks */
    MXC_USB_EventEnable(MAXUSB_EVENT_NOVBUS, eventCallback, NULL);
    MXC_USB_EventEnable(MAXUSB_EVENT_VBUS, eventCallback, NULL);
    acm_register_callback(ACM_CB_READ_READY, usbReadCallback);
    usb_read_complete = 0;

    /* Start with USB in low power mode */
    usbAppSleep();
    NVIC_EnableIRQ(USB_IRQn);

    return E_NO_ERROR;
}

int usb_worker(void)
{
    echoUSB();

    if (event_flags) {
        /* Display events */
        if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_NOVBUS)) {
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_NOVBUS);
            PR_INFO("VBUS Disconnect");
        }
        else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_VBUS)) {
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_VBUS);
            PR_INFO("VBUS Connect");
        }
        else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_BRST)) {
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_BRST);
            PR_INFO("Bus Reset");
        }
        else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_SUSP)) {
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_SUSP);
            PR_INFO("Suspended");
        }
        else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_DPACT)) {
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_DPACT);
            PR_INFO("Resume");
        }
        else if (MXC_GETBIT(&event_flags, EVENT_ENUM_COMP)) {
            MXC_CLRBIT(&event_flags, EVENT_ENUM_COMP);
            PR_INFO("Enumeration complete. Waiting for characters...");
        }
        else if (MXC_GETBIT(&event_flags, EVENT_REMOTE_WAKE)) {
            MXC_CLRBIT(&event_flags, EVENT_REMOTE_WAKE);
            PR_INFO("Remote Wakeup");
        }
    }

    return E_NO_ERROR;
}

/* This callback is used to allow the driver to call part specific initialization functions. */
int usbStartupCallback()
{
    // Startup the HIRC96M clock if it's not on already
    if (!(MXC_GCR->clkcn & MXC_F_GCR_CLKCN_HIRC96M_EN)) {
        MXC_GCR->clkcn |= MXC_F_GCR_CLKCN_HIRC96M_EN;

        if (MXC_SYS_Clock_Timeout(MXC_F_GCR_CLKCN_HIRC96M_RDY) != E_NO_ERROR) {
            return E_TIME_OUT;
        }
    }

    MXC_SYS_ClockEnable(MXC_SYS_PERIPH_CLOCK_USB);

    return E_NO_ERROR;
}

int usbShutdownCallback()
{
    MXC_SYS_ClockDisable(MXC_SYS_PERIPH_CLOCK_USB);

    return E_NO_ERROR;
}

static void usbAppSleep(void)
{
    /* TODO: Place low-power code here */
    suspended = 1;
}

static void usbAppWakeup(void)
{
    /* TODO: Place low-power code here */
    suspended = 0;
}

static void echoUSB(void)
{
    int chars;
    uint8_t buffer[BUFFER_SIZE];

    if ((chars = acm_canread()) > 0) {

        if (chars > BUFFER_SIZE) {
            chars = BUFFER_SIZE;
        }

        // Read the data from USB
        if (acm_read(buffer, chars) != chars) {
            PR_ERROR("acm_read failed");
            return;
        }

        // Echo it back
        if (acm_present()) {
            if (acm_write(buffer, chars) != chars) {
                PR_ERROR("acm_write failed");
            }
        }
    }
}

static int setconfigCallback(MXC_USB_SetupPkt* sud, void* cbdata)
{
    /* Confirm the configuration value */
    if (sud->wValue == config_descriptor.config_descriptor.bConfigurationValue) {
        configured = 1;
        MXC_SETBIT(&event_flags, EVENT_ENUM_COMP);
        return acm_configure(&acm_cfg);  /* Configure the device class */
    }
    else if (sud->wValue == 0) {
        configured = 0;
        return acm_deconfigure();
    }

    return -1;
}

static int setfeatureCallback(MXC_USB_SetupPkt* sud, void* cbdata)
{
    if (sud->wValue == FEAT_REMOTE_WAKE) {
        remote_wake_en = 1;
    }
    else {
        // Unknown callback
        return -1;
    }

    return 0;
}

static int clrfeatureCallback(MXC_USB_SetupPkt* sud, void* cbdata)
{
    if (sud->wValue == FEAT_REMOTE_WAKE) {
        remote_wake_en = 0;
    }
    else {
        // Unknown callback
        return -1;
    }

    return 0;
}

static int eventCallback(maxusb_event_t evt, void* data)
{
    /* Set event flag */
    MXC_SETBIT(&event_flags, evt);

    switch (evt) {
    case MAXUSB_EVENT_NOVBUS:
        MXC_USB_EventDisable(MAXUSB_EVENT_BRST);
        MXC_USB_EventDisable(MAXUSB_EVENT_SUSP);
        MXC_USB_EventDisable(MAXUSB_EVENT_DPACT);
        MXC_USB_Disconnect();
        configured = 0;
        enum_clearconfig();
        acm_deconfigure();
        usbAppSleep();
        break;

    case MAXUSB_EVENT_VBUS:
        MXC_USB_EventClear(MAXUSB_EVENT_BRST);
        MXC_USB_EventEnable(MAXUSB_EVENT_BRST, eventCallback, NULL);
        MXC_USB_EventClear(MAXUSB_EVENT_SUSP);
        MXC_USB_EventEnable(MAXUSB_EVENT_SUSP, eventCallback, NULL);
        MXC_USB_Connect();
        usbAppSleep();
        break;

    case MAXUSB_EVENT_BRST:
        usbAppWakeup();
        enum_clearconfig();
        acm_deconfigure();
        configured = 0;
        suspended = 0;
        break;

    case MAXUSB_EVENT_SUSP:
        usbAppSleep();
        break;

    case MAXUSB_EVENT_DPACT:
        usbAppWakeup();
        break;

    default:
        break;
    }

    return 0;
}

static int usbReadCallback(void)
{
    usb_read_complete = 1;
    return 0;
}

static void delay_us(unsigned int usec)
{
    /* mxc_delay() takes unsigned long, so can't use it directly */
    MXC_Delay(usec);
}
