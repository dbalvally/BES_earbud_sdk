/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#ifndef __HAL_CMU_BEST1400_H__
#define __HAL_CMU_BEST1400_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_CMU_VALID_CRYSTAL_FREQ          { 26000000, 26000000, 26000000, 26000000, }

#define HAL_CMU_DEFAULT_CRYSTAL_FREQ        26000000

enum HAL_CMU_MOD_ID_T {
    // HCLK/HRST
    HAL_CMU_MOD_H_MCU = 0,      // 0
    HAL_CMU_MOD_H_ROM,          // 1
    HAL_CMU_MOD_H_RAM0,         // 2
    HAL_CMU_MOD_H_RAM1,         // 3
    HAL_CMU_MOD_H_AHB0,         // 4
    HAL_CMU_MOD_H_AHB1,         // 5
    HAL_CMU_MOD_H_GDMA,         // 6
    HAL_CMU_MOD_H_EXTMEM,       // 7
    HAL_CMU_MOD_H_FLASH,        // 8
    HAL_CMU_MOD_H_USBC,         // 9
    HAL_CMU_MOD_H_CODEC,        // 10
    HAL_CMU_MOD_H_I2C_SLAVE,    // 11
    HAL_CMU_MOD_H_USBH,         // 12
    HAL_CMU_MOD_H_AH2H_BT,      // 13
    HAL_CMU_MOD_H_BT_DUMP,      // 14
    // PCLK/PRST
    HAL_CMU_MOD_P_CMU,          // 15
    HAL_CMU_MOD_P_GPIO,         // 16
    HAL_CMU_MOD_P_GPIO_INT,     // 17
    HAL_CMU_MOD_P_WDT,          // 18
    HAL_CMU_MOD_P_PWM,          // 19
    HAL_CMU_MOD_P_TIMER0,       // 20
    HAL_CMU_MOD_P_TIMER1,       // 21
    HAL_CMU_MOD_P_I2C0,         // 22
    HAL_CMU_MOD_P_SPI,          // 23
    HAL_CMU_MOD_P_SPI_ITN,      // 24
    HAL_CMU_MOD_P_UART0,        // 25
    HAL_CMU_MOD_P_UART1,        // 26
    HAL_CMU_MOD_P_I2S,          // 27
    HAL_CMU_MOD_P_SPDIF0,       // 28
    HAL_CMU_MOD_P_IOMUX,        // 29
    HAL_CMU_MOD_P_PCM,          // 30
    HAL_CMU_MOD_P_GLOBAL,       // 31
    // OCLK/ORST
    HAL_CMU_MOD_O_SLEEP,        // 32
    HAL_CMU_MOD_O_FLASH,        // 33
    HAL_CMU_MOD_O_USB,          // 34
    HAL_CMU_MOD_O_GPIO,         // 35
    HAL_CMU_MOD_O_WDT,          // 36
    HAL_CMU_MOD_O_TIMER0,       // 37
    HAL_CMU_MOD_O_TIMER1,       // 38
    HAL_CMU_MOD_O_I2C0,         // 39
    HAL_CMU_MOD_O_SPI,          // 40
    HAL_CMU_MOD_O_SPI_ITN,      // 41
    HAL_CMU_MOD_O_UART0,        // 42
    HAL_CMU_MOD_O_UART1,        // 43
    HAL_CMU_MOD_O_I2S,          // 44
    HAL_CMU_MOD_O_SPDIF0,       // 45
    HAL_CMU_MOD_O_USB32K,       // 46
    HAL_CMU_MOD_O_IOMUX,        // 47
    HAL_CMU_MOD_O_PWM0,         // 48
    HAL_CMU_MOD_O_PWM1,         // 49
    HAL_CMU_MOD_O_PWM2,         // 50
    HAL_CMU_MOD_O_PWM3,         // 51
    HAL_CMU_MOD_O_CODEC,        // 52
    HAL_CMU_MOD_O_CODECIIR,     // 53
    HAL_CMU_MOD_O_CODECRS,      // 54
    HAL_CMU_MOD_O_CODECADC,     // 55
    HAL_CMU_MOD_O_CODECAD0,     // 56
    HAL_CMU_MOD_O_CODECAD1,     // 57
    HAL_CMU_MOD_O_CODECAD2,     // 58
    HAL_CMU_MOD_O_CODECDA0,     // 59
    HAL_CMU_MOD_O_CODECDA1,     // 60
    HAL_CMU_MOD_O_PCM,          // 61
    HAL_CMU_MOD_O_BT_ALL,       // 62

    HAL_CMU_MOD_QTY,

    HAL_CMU_MOD_GLOBAL = HAL_CMU_MOD_P_GLOBAL,
};

enum HAL_CMU_CLOCK_OUT_ID_T {

    HAL_CMU_CLOCK_OUT_QTY,
};

enum HAL_CMU_I2S_MCLK_ID_T {

    HAL_CMU_I2S_MCLK_QTY,
};

void hal_cmu_codec_reset_set(void);

void hal_cmu_codec_reset_clear(void);

void hal_cmu_codec_iir_enable(uint32_t speed);

void hal_cmu_codec_iir_disable(void);

int hal_cmu_codec_iir_set_div(uint32_t div);

int hal_cmu_i2s_mclk_enable(enum HAL_CMU_I2S_MCLK_ID_T id);

void hal_cmu_i2s_mclk_disable(void);

void hal_cmu_codec_vad_clock_enable(void);

void hal_cmu_codec_vad_clock_disable(void);

#ifdef __cplusplus
}
#endif

#endif

