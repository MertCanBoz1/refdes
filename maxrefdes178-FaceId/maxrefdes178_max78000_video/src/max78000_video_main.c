/*******************************************************************************
 * Copyright (C) 2020-2021 Maxim Integrated Products, Inc., All rights Reserved.
 *
 * This software is protected by copyright laws of the United States and
 * of foreign countries. This material may also be protected by patent laws
 * and technology transfer regulations of the United States and of foreign
 * countries. This software is furnished under a license agreement and/or a
 * nondisclosure agreement and may only be used or reproduced in accordance
 * with the terms of those agreements. Dissemination of this information to
 * any party or parties not specified in the license agreement and/or
 * nondisclosure agreement is expressly prohibited.
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
 *******************************************************************************
 */

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <board.h>
#include <camera.h>
#include <gpio.h>
#include <icc.h>
#include <mxc.h>
#include <mxc_delay.h>
#include <rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "max78000_debug.h"
#include "max78000_qspi_slave.h"
#include "max78000_video_cnn.h"
#include "max78000_video_embedding_process.h"
#include "max78000_video_weights.h"
#include "maxrefdes178_definitions.h"
#include "maxrefdes178_utility.h"
#include "maxrefdes178_version.h"


//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define S_MODULE_NAME          "main"

//#define PRINT_TIME_CNN


//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
static const mxc_gpio_cfg_t gpio_flash     = MAX78000_VIDEO_FLASH_LED_PIN;
static const mxc_gpio_cfg_t gpio_camera    = MAX78000_VIDEO_CAMERA_PIN;
static const mxc_gpio_cfg_t gpio_sram_cs   = MAX78000_VIDEO_SRAM_CS_PIN;
static const mxc_gpio_cfg_t gpio_i2c       = MAX78000_VIDEO_I2C_PINS;
static const mxc_gpio_cfg_t gpio_debug_sel = MAX78000_VIDEO_DEBUG_SEL_PIN;
static const mxc_gpio_cfg_t gpio_exp_io    = MAX78000_VIDEO_EXPANDER_IO_PIN;
static const mxc_gpio_cfg_t gpio_exp_out   = MAX78000_VIDEO_EXPANDER_OUT_PIN;
static const mxc_gpio_cfg_t gpio_audio_int = MAX78000_VIDEO_AUDIO_INT_PIN;
static const mxc_gpio_cfg_t gpio_cnn_boost = MAX78000_VIDEO_CNN_BOOST_PIN;
static const mxc_gpio_cfg_t gpio_red       = MAX78000_VIDEO_LED_RED_PIN;
static const mxc_gpio_cfg_t gpio_green     = MAX78000_VIDEO_LED_GREEN_PIN;
static const mxc_gpio_cfg_t gpio_blue      = MAX78000_VIDEO_LED_BLUE_PIN;

static const uint8_t camera_settings[][2] = {
    {0x0e, 0x08}, // Sleep mode
    {0x69, 0x52}, // BLC window selection, BLC enable (default is 0x12)
    {0x1e, 0xb3}, // AddLT1F (default 0xb1)
    {0x48, 0x42},
    {0xff, 0x01}, // Select MIPI register bank
    {0xb5, 0x30},
    {0xff, 0x00}, // Select system control register bank
    {0x16, 0x03}, // (default)
    {0x62, 0x10}, // (default)
    {0x12, 0x01}, // Select Bayer RAW
    {0x17, 0x65}, // Horizontal Window Start Point Control (LSBs), default is 0x69
    {0x18, 0xa4}, // Horizontal sensor size (default)
    {0x19, 0x0c}, // Vertical Window Start Line Control (default)
    {0x1a, 0xf6}, // Vertical sensor size (default)
    {0x37, 0x04}, // PCLK is double system clock (default is 0x0c)
    {0x3e, 0x20}, // (default)
    {0x81, 0x3f}, // sde_en, uv_adj_en, scale_v_en, scale_h_en, uv_avg_en, cmx_en
    {0xcc, 0x02}, // High 2 bits of horizontal output size (default)
    {0xcd, 0x80}, // Low 8 bits of horizontal output size (default)
    {0xce, 0x01}, // Ninth bit of vertical output size (default)
    {0xcf, 0xe0}, // Low 8 bits of vertical output size (default)
    {0x82, 0x01}, // 01: Raw from CIP (default is 0x00)
    {0xc8, 0x02},
    {0xc9, 0x80},
    {0xca, 0x01},
    {0xcb, 0xe0},
    {0xd0, 0x28},
    {0x0e, 0x00}, // Normal mode (not sleep mode)
    {0x70, 0x00},
    {0x71, 0x34},
    {0x74, 0x28},
    {0x75, 0x98},
    {0x76, 0x00},
    {0x77, 0x64},
    {0x78, 0x01},
    {0x79, 0xc2},
    {0x7a, 0x4e},
    {0x7b, 0x1f},
    {0x7c, 0x00},
    {0x11, 0x01}, // CLKRC, Internal clock pre-scalar divide by 2 (default divide by 1)
    {0x20, 0x00}, // Banding filter (default)
    {0x21, 0x57}, // Banding filter (default is 0x44)
    {0x50, 0x4d},
    {0x51, 0x40}, // 60Hz Banding AEC 8 bits (default 0x80)
    {0x4c, 0x7d},
    {0x0e, 0x00},
    {0x80, 0x7f},
    {0x85, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x00},
    {0x89, 0x2a},
    {0x8a, 0x22},
    {0x8b, 0x20},
    {0xbb, 0xab},
    {0xbc, 0x84},
    {0xbd, 0x27},
    {0xbe, 0x0e},
    {0xbf, 0xb8},
    {0xc0, 0xc5},
    {0xc1, 0x1e},
    {0xb7, 0x05},
    {0xb8, 0x09},
    {0xb9, 0x00},
    {0xba, 0x18},
    {0x5a, 0x1f},
    {0x5b, 0x9f},
    {0x5c, 0x69},
    {0x5d, 0x42},
    {0x24, 0x78}, // AGC/AEC
    {0x25, 0x68}, // AGC/AEC
    {0x26, 0xb3}, // AGC/AEC
    {0xa3, 0x0b},
    {0xa4, 0x15},
    {0xa5, 0x29},
    {0xa6, 0x4a},
    {0xa7, 0x58},
    {0xa8, 0x65},
    {0xa9, 0x70},
    {0xaa, 0x7b},
    {0xab, 0x85},
    {0xac, 0x8e},
    {0xad, 0xa0},
    {0xae, 0xb0},
    {0xaf, 0xcb},
    {0xb0, 0xe1},
    {0xb1, 0xf1},
    {0xb2, 0x14},
    {0x8e, 0x92},
    {0x96, 0xff},
    {0x97, 0x00},
    {0x14, 0x2b},   // AGC value, manual, set banding (default is 0x30)
    {0x0e, 0x00},
    {0x0c, 0xd6},
    {0x82, 0x3},
    {0x11, 0x00},   // Set clock prescaler
    {0x12, 0x6},
    {0x61, 0x0},
    {0x64, 0x11},
    {0xc3, 0x80},
    {0x81, 0x3f},
    {0x16, 0x3},
    {0x37, 0xc},
    {0x3e, 0x20},
    {0x5e, 0x0},
    {0xc4, 0x1},
    {0xc5, 0x80},
    {0xc6, 0x1},
    {0xc7, 0x80},
    {0xc8, 0x2},
    {0xc9, 0x80},
    {0xca, 0x1},
    {0xcb, 0xe0},
    {0xcc, 0x0},
    {0xcd, 0x40},   // Default to 64 line width
    {0xce, 0x0},
    {0xcf, 0x40},   // Default to 64 lines high
    {0x1c, 0x7f},
    {0x1d, 0xa2},
    {0xee, 0xee}  // End of register list marker 0xee
};

// *****************************************************************************
static int8_t prev_decision = -2;
static int8_t decision = -2;
static uint32_t time_counter = 0;
static int8_t enable_cnn = 1;
static volatile int8_t button_pressed = 0;
static int8_t flash_led = 0;
static int8_t camera_vflip = 1;
static int8_t enable_video = 0;
static int8_t enable_sleep = 0;
static uint8_t qspi_payload_buffer[11280];
static version_t version = {S_VERSION_MAJOR, S_VERSION_MINOR, S_VERSION_BUILD};
static char demo_name[] = FACEID_DEMO_NAME;
static uint32_t camera_clock = 15 * 1000 * 1000;

#ifdef PRINT_TIME_CNN
#define PR_TIMER(fmt, args...) if((time_counter % 10) == 0) printf("T[%-5s:%4d] " fmt "\r\n", S_MODULE_NAME, __LINE__, ##args )
#endif


//-----------------------------------------------------------------------------
// Local function declarations
//-----------------------------------------------------------------------------
static void fail(void);
static void send_img(void);
static void run_cnn(int x_offset, int y_offset);
static void run_demo(void);


//-----------------------------------------------------------------------------
// Function definitions
//-----------------------------------------------------------------------------
void button_int(void *cbdata)
{
    button_pressed = 1;
}

void sleep_defer_int(void)
{
    // Clear interrupt
    MXC_TMR_ClearFlags(MAX78000_VIDEO_SLEEP_DEFER_TMR);

    enable_sleep = 1;
}

int main(void)
{
    MXC_Delay(MXC_DELAY_MSEC(500)); // Wait supply to be ready

    int ret = 0;
    int slaveAddress;
    int id;

    // Enable camera
    GPIO_CLR(gpio_camera);
    MXC_GPIO_Config(&gpio_camera);

    // Configure flash pin
    GPIO_SET(gpio_flash);
    MXC_GPIO_Config(&gpio_flash);
    MXC_GPIO0->ds0 |= gpio_flash.mask;
    MXC_GPIO0->ds1 |= gpio_flash.mask;
    GPIO_SET(gpio_flash);

    // Configure CNN boost
    GPIO_CLR(gpio_cnn_boost);
    MXC_GPIO_Config(&gpio_cnn_boost);

    // Configure LEDs
    GPIO_CLR(gpio_red);
    MXC_GPIO_Config(&gpio_red);

    GPIO_CLR(gpio_green);
    MXC_GPIO_Config(&gpio_green);

    GPIO_CLR(gpio_blue);
    MXC_GPIO_Config(&gpio_blue);

    // Configure unused pins as high-z
    MXC_GPIO_Config(&gpio_sram_cs);
    MXC_GPIO_Config(&gpio_i2c);
    MXC_GPIO_Config(&gpio_debug_sel);
    MXC_GPIO_Config(&gpio_exp_io);
    MXC_GPIO_Config(&gpio_exp_out);
    MXC_GPIO_Config(&gpio_audio_int);

    /* Enable cache */
    MXC_ICC_Enable(MXC_ICC0);

    /* Set system clock to 100 MHz */
    MXC_SYS_Clock_Select(MXC_SYS_CLOCK_IPO);
    SystemCoreClockUpdate();

    PR_INFO("maxrefdes178_max78000_video %s v%d.%d.%d [%s]",
            demo_name, version.major, version.minor, version.build, S_BUILD_TIMESTAMP);

    // Enable peripheral, enable CNN interrupt, turn on CNN clock
    // CNN clock: 50 MHz div 1
    ret  = cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
    ret &= cnn_init(); // Bring CNN state machine into consistent state
    ret &= cnn_load_weights(); // Load CNN kernels
    ret &= cnn_configure(); // Configure CNN state machine
    if (ret != CNN_OK) {
        PR_ERROR("Could not initialize the CNN accelerator %d", ret);
        fail();
    }

    ret = init_database();
    if (ret != E_NO_ERROR) {
        PR_ERROR("Could not initialize the database, %d", ret);
        fail();
    }

    /* Initialize RTC */
    ret = MXC_RTC_Init(0, 0);
    if (ret != E_NO_ERROR) {
        PR_ERROR("Could not initialize rtc, %d", ret);
        fail();
    }

    ret = MXC_RTC_Start();
    if (ret != E_NO_ERROR) {
        PR_ERROR("Could not start rtc, %d", ret);
        fail();
    }

    ret = MXC_DMA_Init();
    if (ret != E_NO_ERROR) {
        PR_ERROR("DMA INIT ERROR %d", ret);
        fail();
    }

    ret = qspi_slave_init();
    if (ret != E_NO_ERROR) {
        PR_ERROR("qspi_dma_slave_init fail %d", ret);
        fail();
    }

    // Initialize the camera driver.
    ret = camera_init(camera_clock);
    if (ret != E_NO_ERROR) {
        PR_ERROR("Camera init failed! %d", ret);
        fail();
    }

    // Obtain the I2C slave address of the camera.
    slaveAddress = camera_get_slave_address();
    PR_INFO("Camera I2C slave address is 0x%02hhX", slaveAddress);

    // Obtain the product ID of the camera.
    ret = camera_get_product_id(&id);
    if (ret != STATUS_OK) {
        PR_ERROR("Error returned from reading camera product id. Error : %d", ret);
        fail();
    }
    PR_INFO("Camera Product ID is 0x%04hhX", id);

    // Obtain the manufacture ID of the camera.
    ret = camera_get_manufacture_id(&id);
    if (ret != STATUS_OK) {
        PR_ERROR("Error returned from reading camera manufacture id. Error : %d", ret);
        return -1;
    }
    PR_INFO("Camera Manufacture ID is 0x%04hhX", id);

    // set camera registers with default values
    for (int i = 0; (camera_settings[i][0] != 0xee); i++) {
        camera_write_reg(camera_settings[i][0], camera_settings[i][1]);
    }

    // Setup the camera image dimensions, pixel format and data acquiring details.
    ret = camera_setup(CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FORMAT, FIFO_FOUR_BYTE, USE_DMA, MAX78000_VIDEO_CAMERA_DMA_CHANNEL);
    if (ret != STATUS_OK) {
        PR_ERROR("Error returned from setting up camera. Error : %d", ret);
        fail();
    }

    // Init button interrupt
    PB_RegisterCallback(0, (pb_callback) button_int);

    // Init sleep defer timer interrupt
    mxc_tmr_cfg_t tmr;
    MXC_TMR_Shutdown(MAX78000_VIDEO_SLEEP_DEFER_TMR);
    tmr.pres = TMR_PRES_128;
    tmr.mode = TMR_MODE_ONESHOT;
    tmr.bitMode = TMR_BIT_MODE_32;
    tmr.clock = MXC_TMR_APB_CLK;
    tmr.cmp_cnt = MAX78000_SLEEP_DEFER_DURATION * PeripheralClock / 128;
    tmr.pol = 0;
    MXC_NVIC_SetVector(MXC_TMR_GET_IRQ(MXC_TMR_GET_IDX(MAX78000_VIDEO_SLEEP_DEFER_TMR)), sleep_defer_int);
    NVIC_EnableIRQ(MXC_TMR_GET_IRQ(MXC_TMR_GET_IDX(MAX78000_VIDEO_SLEEP_DEFER_TMR)));
    MXC_TMR_Init(MAX78000_VIDEO_SLEEP_DEFER_TMR, &tmr, false);
    MXC_TMR_EnableInt(MAX78000_VIDEO_SLEEP_DEFER_TMR);
    MXC_TMR_Start(MAX78000_VIDEO_SLEEP_DEFER_TMR);

    // Use camera interface buffer for QSPI payload buffer
    /*
    {
        uint32_t  imgLen;
        uint32_t  w, h;
        camera_get_image(&qspi_payload_buffer, &imgLen, &w, &h);
    }
*/
    // Successfully initialize the program
    PR_INFO("Program initialized successfully");

    run_demo();

    return 0;
}

static void fail(void)
{
    PR_ERROR("fail");

    GPIO_SET(gpio_red);
    GPIO_CLR(gpio_green);
    GPIO_CLR(gpio_blue);

    while(1);
}

static void run_demo(void)
{
    uint32_t capture_started_time = GET_RTC_MS();
    uint32_t cnn_completed_time = 0;
    uint32_t qspi_completed_time = 0;
    uint32_t capture_completed_time = 0;
    max78000_statistics_t max78000_statistics = {0};
    qspi_packet_header_t qspi_rx_header;
    qspi_state_e qspi_rx_state;

    PR_INFO("Embeddings subject names:");
    for (int i = 0; i < get_subject_count(); i++) {
          PR_INFO("  %s", get_subject_name(i));
    }

    camera_start_capture_image();

    while (1) { //Capture image and run CNN

        /* Check if QSPI RX has data */
        qspi_rx_state = qspi_slave_get_rx_state();
        if (qspi_rx_state == QSPI_STATE_CS_DEASSERTED_HEADER) {
            qspi_rx_header = qspi_slave_get_rx_header();

            // Use camera interface buffer for QSPI payload
            MXC_PCIF_Stop();

            qspi_slave_set_rx_data(qspi_payload_buffer, qspi_rx_header.info.packet_size);
            qspi_slave_trigger();
            qspi_slave_wait_rx();

            // Check payload crc again
            if (qspi_rx_header.payload_crc16 != crc16_sw(qspi_payload_buffer, qspi_rx_header.info.packet_size)) {
                PR_ERROR("Invalid payload crc %x", qspi_rx_header.payload_crc16);
                qspi_slave_set_rx_state(QSPI_STATE_IDLE);
                camera_start_capture_image();
                continue;
            }

            // QSPI commands with payload
            switch(qspi_rx_header.info.packet_type) {
                case QSPI_PACKET_TYPE_VIDEO_FACEID_EMBED_UPDATE_CMD:
                    PR_INFO("facied embeddings received %d", qspi_rx_header.info.packet_size);

                    uninit_database();

                    uint8_t faceid_embed_update_status;
                    if (update_database(qspi_payload_buffer, qspi_rx_header.info.packet_size) != E_NO_ERROR) {
                        PR_ERROR("Could not update the database");
                        faceid_embed_update_status = FACEID_EMBED_UPDATE_STATUS_ERROR_UNKNOWN;
                    } else if (init_database() != E_NO_ERROR) {
                        PR_ERROR("Could not initialize the database");
                        faceid_embed_update_status = FACEID_EMBED_UPDATE_STATUS_ERROR_UNKNOWN;
                    } else {
                        PR_INFO("Embeddings update completed");
                        for (int i = 0; i < get_subject_count(); i++) {
                            PR_INFO("  %s", get_subject_name(i));
                        }
                        faceid_embed_update_status = FACEID_EMBED_UPDATE_STATUS_SUCCESS;
                    }
                    qspi_slave_set_rx_state(QSPI_STATE_IDLE);
                    qspi_slave_send_packet(&faceid_embed_update_status, 1, QSPI_PACKET_TYPE_VIDEO_FACEID_EMBED_UPDATE_RES);

                    break;

                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS0:
                    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
                    cnn_init(); // Bring state machine into consistent state
                    // cnn_load_weights_faceid(); // No need to reload kernels
                    capture_started_time = GET_RTC_MS();
                    *((volatile uint8_t *) 0x50180001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50180000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50184001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50184000,(uint32_t *) &qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50188001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50188000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x5018c081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5018c000,(uint32_t *) &qspi_payload_buffer[8460], 633);    
                      break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS1:
                    *((volatile uint8_t *) 0x50190001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50190000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50194001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50194000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50198001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50198000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x5019c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5019c000, (uint32_t *)&qspi_payload_buffer[8460], 705);   
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS2:
                    *((volatile uint8_t *) 0x501a0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501a0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x501a4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501a4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x501a8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501a8000,(uint32_t *) &qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x501ac001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501ac000, (uint32_t *)&qspi_payload_buffer[8460], 705);   
                    break;                     
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS3:
                    *((volatile uint8_t *) 0x501b0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501b0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x501b4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501b4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x501b8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501b8000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x501bc001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x501bc000, (uint32_t *)&qspi_payload_buffer[8460], 705); 

                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS4:
                    *((volatile uint8_t *) 0x50580001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50580000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50584001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50584000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50588001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50588000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x5058c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5058c000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS5:
                    *((volatile uint8_t *) 0x50590081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50590000, (uint32_t *)&qspi_payload_buffer[0], 633);    
                    *((volatile uint8_t *) 0x50594081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50594000, (uint32_t *)&qspi_payload_buffer[2532], 633);
                    *((volatile uint8_t *) 0x50598081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50598000, (uint32_t *)&qspi_payload_buffer[5064], 633);
                    *((volatile uint8_t *) 0x5059c081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5059c000, (uint32_t *)&qspi_payload_buffer[7596], 633); 
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS6:
                    *((volatile uint8_t *) 0x505a0081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505a0000, (uint32_t *)&qspi_payload_buffer[0], 633);    
                    *((volatile uint8_t *) 0x505a4081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505a4000, (uint32_t *)&qspi_payload_buffer[2532], 633);
                    *((volatile uint8_t *) 0x505a8081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505a8000, (uint32_t *)&qspi_payload_buffer[5064], 633);
                    *((volatile uint8_t *) 0x505ac081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505ac000, (uint32_t *)&qspi_payload_buffer[7596], 633); 

                    break;
                    
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS7:
                    *((volatile uint8_t *) 0x505b0081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505b0000, (uint32_t *)&qspi_payload_buffer[0], 633);    
                    *((volatile uint8_t *) 0x505b4081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505b4000, (uint32_t *)&qspi_payload_buffer[2532], 633);
                    *((volatile uint8_t *) 0x505b8081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505b8000,(uint32_t *) &qspi_payload_buffer[5064], 633);
                    *((volatile uint8_t *) 0x505bc081) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x505bc000,(uint32_t *) &qspi_payload_buffer[7596], 633); 

                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS8:
                    *((volatile uint8_t *) 0x50980001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50980000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50984001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50984000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50988001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50988000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x5098c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5098c000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS9:
                    *((volatile uint8_t *) 0x50990001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50990000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50994001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50994000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50998001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50998000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x5099c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x5099c000, (uint32_t *)&qspi_payload_buffer[8460], 705); 

                    break; 
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS10:
                    *((volatile uint8_t *) 0x509a0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509a0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x509a4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509a4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x509a8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509a8000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x509ac001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509ac000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS11:
                    *((volatile uint8_t *) 0x509b0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509b0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x509b4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509b4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x509b8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509b8000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x509bc001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x509bc000, (uint32_t *)&qspi_payload_buffer[8460], 705); 

                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS12:
                    *((volatile uint8_t *) 0x50d80001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d80000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50d84001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d84000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50d88001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d88000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x50d8c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d8c000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    break;
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS13:
                    *((volatile uint8_t *) 0x50d90001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d90000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50d94001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d94000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50d98001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d98000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x50d9c001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50d9c000, (uint32_t *)&qspi_payload_buffer[8460], 705); 

                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS14:
                    *((volatile uint8_t *) 0x50da0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50da0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50da4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50da4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50da8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50da8000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x50dac001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50dac000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    break;
                    
                case QSPI_PACKET_TYPE_WEIGHTS_KERNELS15:
                    *((volatile uint8_t *) 0x50db0001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50db0000, (uint32_t *)&qspi_payload_buffer[0], 705);    
                    *((volatile uint8_t *) 0x50db4001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50db4000, (uint32_t *)&qspi_payload_buffer[2820], 705);
                    *((volatile uint8_t *) 0x50db8001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50db8000, (uint32_t *)&qspi_payload_buffer[5640], 705);
                    *((volatile uint8_t *) 0x50dbc001) = 0x01; // Set address
                    memcpy32((uint32_t *) 0x50dbc000, (uint32_t *)&qspi_payload_buffer[8460], 705); 
                    faceokay = 0;
                    weights = 1;
                    capture_completed_time = GET_RTC_MS();
                    PR_INFO("%d",(capture_completed_time - capture_started_time) * 1000);  
                    break;
                    
                case QSPI_PACKET_TYPE_TEST:
                    PR_INFO("test data received %d", qspi_rx_header.info.packet_size);
                    for (int i = 0; i < qspi_rx_header.info.packet_size; i++) {
                        printf("%02hhX ", qspi_payload_buffer[i]);
                    }
                    printf("\n");
                    break;
                default:
                    PR_ERROR("Invalid packet %d", qspi_rx_header.info.packet_type);
                    break;
            }

            qspi_slave_set_rx_state(QSPI_STATE_IDLE);
            camera_start_capture_image();

        } else if (qspi_rx_state == QSPI_STATE_COMPLETED) {
            qspi_rx_header = qspi_slave_get_rx_header();

            // QSPI commands without payload
            switch(qspi_rx_header.info.packet_type) {
            case QSPI_PACKET_TYPE_VIDEO_VERSION_CMD:
                qspi_slave_set_rx_state(QSPI_STATE_IDLE);
                qspi_slave_send_packet((uint8_t *) &version, sizeof(version),
                        QSPI_PACKET_TYPE_VIDEO_VERSION_RES);
                break;
            case QSPI_PACKET_TYPE_VIDEO_DEMO_NAME_CMD:
                qspi_slave_set_rx_state(QSPI_STATE_IDLE);
                qspi_slave_send_packet((uint8_t *) demo_name, strlen(demo_name),
                        QSPI_PACKET_TYPE_VIDEO_DEMO_NAME_RES);
                break;
            case QSPI_PACKET_TYPE_VIDEO_SERIAL_CMD:
                // TODO
                break;
            case QSPI_PACKET_TYPE_VIDEO_ENABLE_CMD:
                PR_INFO("enable video");
                enable_video = 1;
                // Enable camera
                GPIO_CLR(gpio_camera);
                camera_start_capture_image();
                break;
            case QSPI_PACKET_TYPE_VIDEO_DISABLE_CMD:
                PR_INFO("disable video");
                enable_video = 0;
                MXC_PCIF_Stop();
                // Disable camera
                GPIO_SET(gpio_camera);
                GPIO_CLR(gpio_red);
                GPIO_CLR(gpio_green);
                break;
            case QSPI_PACKET_TYPE_VIDEO_ENABLE_CNN_CMD:
                PR_INFO("enable cnn");
                enable_cnn = 1;
                break;
            case QSPI_PACKET_TYPE_VIDEO_DISABLE_CNN_CMD:
                PR_INFO("disable cnn");
                enable_cnn = 0;
                GPIO_CLR(gpio_red);
                GPIO_CLR(gpio_green);
                break;
            case QSPI_PACKET_TYPE_VIDEO_FACEID_SUBJECTS_CMD:
                // Use camera interface buffer for FaceID embeddings subject names
                MXC_PCIF_Stop();

                memcpy(qspi_payload_buffer, get_subject_name(0), get_subject_names_len());
                qspi_slave_set_rx_state(QSPI_STATE_IDLE);
                qspi_slave_send_packet(qspi_payload_buffer, get_subject_names_len(), QSPI_PACKET_TYPE_VIDEO_FACEID_SUBJECTS_RES);

                camera_start_capture_image();
                break;
            case QSPI_PACKET_TYPE_VIDEO_ENABLE_FLASH_LED_CMD:
                PR_INFO("enable flash");
                GPIO_CLR(gpio_flash);
                flash_led = 1;
                break;
            case QSPI_PACKET_TYPE_VIDEO_DISABLE_FLASH_LED_CMD:
                PR_INFO("disable flash");
                GPIO_SET(gpio_flash);
                flash_led = 0;
                break;
            case QSPI_PACKET_TYPE_VIDEO_CAMERA_CLOCK_5_MHZ_CMD:
                PR_INFO("camera clock 5mhz");
                camera_clock = 5 * 1000 * 1000;
                MXC_PT_Stop(MXC_F_PTG_ENABLE_PT0);
                MXC_PT_SqrWaveConfig(0, camera_clock);
                MXC_PT_Start(MXC_F_PTG_ENABLE_PT0);
                break;
            case QSPI_PACKET_TYPE_VIDEO_CAMERA_CLOCK_10_MHZ_CMD:
                PR_INFO("camera clock 10mhz");
                camera_clock = 10 * 1000 * 1000;
                MXC_PT_Stop(MXC_F_PTG_ENABLE_PT0);
                MXC_PT_SqrWaveConfig(0, camera_clock);
                MXC_PT_Start(MXC_F_PTG_ENABLE_PT0);
                break;
            case QSPI_PACKET_TYPE_VIDEO_CAMERA_CLOCK_15_MHZ_CMD:
                PR_INFO("camera clock 15mhz");
                camera_clock = 15 * 1000 * 1000;
                MXC_PT_Stop(MXC_F_PTG_ENABLE_PT0);
                MXC_PT_SqrWaveConfig(0, camera_clock);
                MXC_PT_Start(MXC_F_PTG_ENABLE_PT0);
                break;
            case QSPI_PACKET_TYPE_VIDEO_ENABLE_VFLIP_CMD:
                PR_INFO("flip enable");
                camera_vflip = 1;
                camera_set_vflip(camera_vflip);
                break;
            case QSPI_PACKET_TYPE_VIDEO_DISABLE_VFLIP_CMD:
                PR_INFO("flip disable");
                camera_vflip = 0;
                camera_set_vflip(camera_vflip);
                break;
            case QSPI_PACKET_TYPE_VIDEO_DEBUG_CMD:
                PR_INFO("dont sleep for %ds to let debugger connection", MAX78000_SLEEP_DEFER_DURATION);
                enable_sleep = 0;
                MXC_TMR_Start(MAX78000_VIDEO_SLEEP_DEFER_TMR);
                break;
            default:
                PR_ERROR("Invalid packet %d", qspi_rx_header.info.packet_type);
                break;
            }

            qspi_slave_set_rx_state(QSPI_STATE_IDLE);
        }

        if (button_pressed) {
            button_pressed = 0;
            PR_INFO("button A pressed");

            flash_led ^= 1;
            if (flash_led) {
                GPIO_CLR(gpio_flash);
            } else {
                GPIO_SET(gpio_flash);
            }

            qspi_slave_send_packet(NULL, 0, QSPI_PACKET_TYPE_VIDEO_BUTTON_PRESS_RES);
        }

        if (!enable_video) {
            if (enable_sleep) {
//                MXC_LP_EnterSleepMode();
                __WFI();
            }
            continue;
        }

        if (camera_is_image_rcv()) { // Check whether image is ready
            capture_completed_time = GET_RTC_MS();

            send_img();

            qspi_completed_time = GET_RTC_MS();

            if (enable_cnn) {
                if(!faceokay){
                    run_cnn(0, 0);
                }
            }

            cnn_completed_time = GET_RTC_MS();

            if (time_counter % 10 == 0) {
                max78000_statistics.capture_duration_us = (capture_completed_time - capture_started_time) * 1000;
                max78000_statistics.communication_duration_us = (qspi_completed_time - capture_completed_time) * 1000;
                max78000_statistics.cnn_duration_us = (cnn_completed_time - qspi_completed_time) * 1000;
                max78000_statistics.total_duration_us = (cnn_completed_time - capture_started_time) * 1000;

                PR_DEBUG("Capture : %lu", max78000_statistics.capture_duration_us);
                PR_DEBUG("CNN     : %lu", max78000_statistics.cnn_duration_us);
                PR_DEBUG("QSPI    : %lu", max78000_statistics.communication_duration_us);
                PR_DEBUG("Total   : %lu\n\n", max78000_statistics.total_duration_us);

                qspi_slave_send_packet((uint8_t *) &max78000_statistics, sizeof(max78000_statistics),
                        QSPI_PACKET_TYPE_VIDEO_STATISTICS_RES);
            }

            time_counter++;

            camera_start_capture_image();
            capture_started_time = GET_RTC_MS();

        }
    }
}

static void send_img(void)
{
    uint8_t   *raw;
    uint32_t  imgLen;
    uint32_t  w, h;

    // Get the details of the image from the camera driver.
    camera_get_image(&raw, &imgLen, &w, &h);

    qspi_slave_send_packet(raw, imgLen, QSPI_PACKET_TYPE_VIDEO_DATA_RES);
//    MXC_Delay(MXC_DELAY_MSEC(3)); // Yield SPI DMA RAM read
}

static void run_cnn(int x_offset, int y_offset)
{
    uint8_t *data;
    uint8_t *raw;
    uint8_t ur, ug, ub;
    int8_t r, g, b;
    uint32_t number;
    uint32_t w, h;
    static uint32_t noface_count = 0;

    // Get the details of the image from the camera driver.
    camera_get_image(&raw, &number, &w, &h);

#ifdef PRINT_TIME_CNN
    uint32_t pass_time = GET_RTC_MS();
#endif

    // Enable CNN clock
    MXC_SYS_ClockEnable(MXC_SYS_PERIPH_CLOCK_CNN);

   // cnn_init(); // Bring state machine into consistent state
    //cnn_load_weights(); // No need to reload kernels
    cnn_configure(); // Configure state machine

    cnn_start();

#ifdef PRINT_TIME_CNN
    PR_TIMER("CNN init : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif

    for (int i = y_offset; i < FACEID_HEIGHT + y_offset; i++) {
        data = raw + (((LCD_HEIGHT - FACEID_HEIGHT) / 2) + i) * LCD_WIDTH * LCD_BYTE_PER_PIXEL;  // down
        data += ((LCD_WIDTH - FACEID_WIDTH) / 2) * LCD_BYTE_PER_PIXEL;  // right

        for(int j = x_offset; j < FACEID_WIDTH + x_offset; j++) {
            // RGB565, |RRRRRGGG|GGGBBBBB|
            ub = (data[j * LCD_BYTE_PER_PIXEL + 1] << 3);
            ug = ((data[j * LCD_BYTE_PER_PIXEL] << 5) | ((data[j * LCD_BYTE_PER_PIXEL + 1] & 0xE0) >> 3));
            ur = (data[j * LCD_BYTE_PER_PIXEL] & 0xF8);

            b = ub - 128;
            g = ug - 128;
            r = ur - 128;

            // Loading data into the CNN fifo
            while (((*((volatile uint32_t *) 0x50000004) & 1)) != 0); // Wait for FIFO 0

            number = 0x00FFFFFF & ((((uint8_t)b) << 16) | (((uint8_t)g) << 8) | ((uint8_t) r));

            *((volatile uint32_t *) 0x50000008) = number; // Write FIFO 0
        }
    }

#ifdef PRINT_TIME_CNN
    PR_TIMER("CNN load : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif

    while (cnn_time == 0)
        __WFI(); // Wait for CNN done

#ifdef PRINT_TIME_CNN
    PR_TIMER("CNN wait : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif

    cnn_unload((uint32_t*)(raw));
    
    cnn_stop();
    // Disable CNN clock to save power
    MXC_SYS_ClockDisable(MXC_SYS_PERIPH_CLOCK_CNN);
    cnn_disable();
#ifdef PRINT_TIME_CNN
    PR_TIMER("CNN unload : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif

    int pResult = calculate_minDistance((uint8_t*)(raw));

#ifdef PRINT_TIME_CNN
    PR_TIMER("Embedding calc : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif

    if (pResult == 0) {
        classification_result_t classification_result = {0};
        uint8_t *counter;
        uint8_t counter_len;
        get_min_dist_counter(&counter, &counter_len);
        classification_result.classification = CLASSIFICATION_NOTHING;
        GPIO_CLR(gpio_red);
        GPIO_CLR(gpio_green);

        prev_decision = decision;
        decision = -5;

        for(uint8_t id = 0; id < counter_len; ++id){
            if (counter[id] >= (uint8_t)(closest_sub_buffer_size * 0.8)){   // >80% detection
                strncpy(classification_result.result, get_subject_name(id), sizeof(classification_result.result) - 1);
                decision = id;
                classification_result.classification = CLASSIFICATION_DETECTED;
                noface_count = 0;
                GPIO_CLR(gpio_red);
                GPIO_SET(gpio_green);
                break;
            } else if (counter[id] >= (uint8_t)(closest_sub_buffer_size * 0.4)){  // >%40 adjust
                strncpy(classification_result.result, "Adjust Face", sizeof(classification_result.result) - 1);
                decision = -2;
                classification_result.classification = CLASSIFICATION_LOW_CONFIDENCE;
                noface_count = 0;
                GPIO_SET(gpio_red);
                GPIO_SET(gpio_green);
                break;
            } else if (counter[id] > closest_sub_buffer_size * 0.2){   // >20% unknown
                strncpy(classification_result.result, "Unknown", sizeof(classification_result.result) - 1);
                decision = -1;
                classification_result.classification = CLASSIFICATION_UNKNOWN;
                noface_count = 0;
                GPIO_SET(gpio_red);
                GPIO_CLR(gpio_green);
                break;
            }
            else if (counter[id] > closest_sub_buffer_size * 0.1){   // >10% transition
                strncpy(classification_result.result, "Transition", sizeof(classification_result.result) - 1);
                decision = -3;
                classification_result.classification = CLASSIFICATION_NOTHING;
                noface_count = 0;
            }
            else
            {
                noface_count ++;
                if (noface_count > 10)
                {
                    strncpy(classification_result.result, "No Face", sizeof(classification_result.result) - 1);
                    decision = -4;
                    classification_result.classification = CLASSIFICATION_NOTHING;
                    noface_count --;
                }
            }
        }

        if(decision != prev_decision){
            qspi_slave_send_packet((uint8_t *) &classification_result, sizeof(classification_result),
                    QSPI_PACKET_TYPE_VIDEO_CLASSIFICATION_RES);
            PR_DEBUG("Result : %s\n", classification_result.result);
        }
    }

#ifdef PRINT_TIME_CNN
    PR_TIMER("Embedding result : %d", GET_RTC_MS() - pass_time);
    pass_time = GET_RTC_MS();
#endif
}
