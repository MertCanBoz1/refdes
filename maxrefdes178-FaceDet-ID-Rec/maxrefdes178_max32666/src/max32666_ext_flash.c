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
#include <mxc_errors.h>
#include <mx25.h>
#include <spixf.h>
#include <stdio.h>

#include "max32666_ext_flash.h"
#include "max32666_debug.h"


//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define S_MODULE_NAME   "extfls"

#define MX25_EXP_ID             0x00C2953A


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Local function declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Function definitions
//-----------------------------------------------------------------------------
int ext_flash_init(void)
{
    int err;
    uint32_t id;

    // Initialize the SPIXFC registers and set the appropriate output pins
    if ((err = MX25_Init()) != E_NO_ERROR) {
        PR_ERROR("MX25_Init failed %d", err);
        return err;
    }

    if ((err = MX25_Reset()) != E_NO_ERROR) {
        PR_ERROR("MX25_Reset failed %d", err);
        return err;
    }

    // Get the ID of the MX25
    id = MX25_ID();
    if (id != MX25_EXP_ID) {
        PR_ERROR("invalid chip id 0x%x", id);
        return E_NOT_SUPPORTED;
    }

    // Enable Quad mode
    if ((err = MX25_Quad(1)) != E_NO_ERROR) {
        PR_ERROR("MX25_Quad failed %d", err);
        return err;
    }

//    uint8_t test_write[50];
//    uint8_t test_read[100];
//    for (int i = 0; i < sizeof(test_write); i++) {
//        test_write[i] = i & 0xff;
//    }
//
//    if ((err = MX25_Erase(0x00000, MX25_Erase_4K)) != E_NO_ERROR) {
//        PR_ERROR("MX25_Erase failed %d", err);
//        return err;
//    }
//
//    if ((err = MX25_Program_Page(0x00000, test_write, sizeof(test_write), MXC_SPIXF_WIDTH_4)) != E_NO_ERROR) {
//        PR_ERROR("MX25_Program_Page failed %d", err);
//        return err;
//    }
//
//    if ((err = MX25_Read(0x00000, test_read, sizeof(test_read), MXC_SPIXF_WIDTH_4)) != E_NO_ERROR) {
//        PR_ERROR("MX25_Read failed %d", err);
//        return err;
//    }
//
//    for (int i = 0; i < sizeof(test_read); i++) {
//        if (test_read[i] != (i & 0xff)) {
//           PR_ERROR("%d 0x%x 0x%x", i, test_read[i], (i & 0xff));
//        }
//    }

    return E_NO_ERROR;
}
