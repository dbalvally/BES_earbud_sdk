/**************************************************************************//**
 * @file     best1400.h
 * @brief    CMSIS Core Peripheral Access Layer Header File for
 *           ARMCM4 Device Series
 * @version  V2.02
 * @date     10. September 2014
 *
 * @note     configured for CM4 with FPU
 *
 ******************************************************************************/
/* Copyright (c) 2011 - 2014 ARM LIMITED

   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   - Neither the name of ARM nor the names of its contributors may be used
     to endorse or promote products derived from this software without
     specific prior written permission.
   *
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
   ---------------------------------------------------------------------------*/


#ifndef __BEST1400_H__
#define __BEST1400_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__
/* -------------------------  Interrupt Number Definition  ------------------------ */

typedef enum IRQn
{
/* -------------------  Cortex-M4 Processor Exceptions Numbers  ------------------- */
  NonMaskableInt_IRQn           = -14,      /*!<  2 Non Maskable Interrupt          */
  HardFault_IRQn                = -13,      /*!<  3 HardFault Interrupt             */
  MemoryManagement_IRQn         = -12,      /*!<  4 Memory Management Interrupt     */
  BusFault_IRQn                 = -11,      /*!<  5 Bus Fault Interrupt             */
  UsageFault_IRQn               = -10,      /*!<  6 Usage Fault Interrupt           */
  SVCall_IRQn                   =  -5,      /*!< 11 SV Call Interrupt               */
  DebugMonitor_IRQn             =  -4,      /*!< 12 Debug Monitor Interrupt         */
  PendSV_IRQn                   =  -2,      /*!< 14 Pend SV Interrupt               */
  SysTick_IRQn                  =  -1,      /*!< 15 System Tick Interrupt           */

/* ----------------------  BEST1400 Specific Interrupt Numbers  --------------------- */
  FPU_IRQn                      =   0,      /*!< FPU Interrupt                      */
  RESERVED01_IRQn               =   1,      /*!< Reserved Interrupt                 */
  RESERVED02_IRQn               =   2,      /*!< Reserved Interrupt                 */
  RESERVED03_IRQn               =   3,      /*!< Reserved Interrupt                 */
  AUDMA_IRQn                    =   4,      /*!< General Purpose DMA Interrupt      */
  MCU_TIMER1_IRQ2n              =   5,      /*!< MCU Timer1 Interrupt1              */
  MCU_TIMER1_IRQ1n              =   6,      /*!< MCU Timer1 Interrupt2              */
  USB_IRQn                      =   7,      /*!< USB Interrupt                      */
  WAKEUP_IRQn                   =   8,      /*!< Wakeup Interrupt                   */
  GPIO_IRQn                     =   9,      /*!< GPIO Interrupt                     */
  WDT_IRQn                      =  10,      /*!< Watchdog Timer Interrupt           */
  RTC_IRQn                      =  11,      /*!< RTC Interrupt                      */
  MCU_TIMER0_IRQ1n              =  12,      /*!< MCU Timer0 Interrupt               */
  MCU_TIMER0_IRQ2n              =  13,      /*!< MCU Timer0 Interrupt               */
  I2C0_IRQn                     =  14,      /*!< I2C0 Interrupt                     */
  SPI0_IRQn                     =  15,      /*!< SPI0 Interrupt                     */
  UART2_IRQn                    =  16,      /*!< Reserved Interrupt                 */
  UART0_IRQn                    =  17,      /*!< UART0 Interrupt                    */
  UART1_IRQn                    =  18,      /*!< UART1 Interrupt                    */
  CODEC_IRQn                    =  19,      /*!< CODEC Interrupt                    */
  PCM_IRQn                      =  20,      /*!< PCM Interrupt                      */
  I2S_IRQn                      =  21,      /*!< I2S Interrupt                      */
  RESERVED22_IRQn               =  22,      /*!< SPDIF0 Interrupt                   */
  SPI_ITN_IRQn                  =  23,      /*!< Internal SPI Interrupt             */
  RESERVED24_IRQn               =  24,      /*!< Reserved Interrupt                 */
  GPADC_IRQn                    =  25,      /*!< GPADC Interrupt                    */
  RESERVED26_IRQn               =  26,      /*!< Reserved Interrupt                 */
  USB_PIN_IRQn                  =  27,      /*!< PMU USB Interrupt                  */
  RESERVED28_IRQn               =  28,      /*!< Reserved Interrupt                 */
  RESERVED29_IRQn               =  29,      /*!< Reserved Interrupt                 */
  USB_CALIB_IRQn                =  30,      /*!< USB CALIB Interrupt                */
  USB_SOF_IRQn                  =  31,      /*!< USB SOF Interrupt                  */
  CHARGER_IRQn                  =  32,      /*!< Charger Interrupt                  */
  PWRKEY_IRQn                   =  33,      /*!< POWER KEY Interrupt                */
  DUMP_IRQn                     =  34,      /*!< DUMP Interrupt                     */
  BT2MCU_IRQn                   =  35,      /*!< BT2MCU Interrupt                   */
  ISDONE_IRQn                   =  36,      /*!< MCU2BT Data0 Done Interrupt        */
  ISDONE1_IRQn                  =  37,      /*!< MCU2BT Data1 Done Interrupt        */
  ISDATA_IRQn                   =  38,      /*!< BT2MCU Data0 Ind Interrupt         */
  ISDATA1_IRQn                  =  39,      /*!< BT2MCU Data1 Ind Interrupt         */
  RESERVED40_IRQn               =  40,      /*!< Reserved Interrupt                 */
  RESERVED41_IRQn               =  41,      /*!< Reserved Interrupt                 */
  RESERVED42_IRQn               =  42,      /*!< Reserved Interrupt                 */
  RESERVED43_IRQn               =  43,      /*!< Reserved Interrupt                 */
  RESERVED44_IRQn               =  44,      /*!< Reserved Interrupt                 */
  RESERVED45_IRQn               =  45,      /*!< Reserved Interrupt                 */
  RESERVED46_IRQn               =  46,      /*!< Reserved Interrupt                 */
  RESERVED47_IRQn               =  47,      /*!< Reserved Interrupt                 */
  USER_IRQn_QTY,
  INVALID_IRQn                  = USER_IRQn_QTY,
} IRQn_Type;

#define TIMER1_IRQn             MCU_TIMER0_IRQ1n
#define TIMER2_IRQn             MCU_TIMER0_IRQ2n

#endif

/* ================================================================================ */
/* ================      Processor and Core Peripheral Section     ================ */
/* ================================================================================ */

/* --------  Configuration of the Cortex-M4 Processor and Core Peripherals  ------- */
#define __CM4_REV                 0x0001    /*!< Core revision r0p1                              */
#define __MPU_PRESENT             1         /*!< MPU present or not                              */
#define __NVIC_PRIO_BITS          3         /*!< Number of Bits used for Priority Levels         */
#define __Vendor_SysTickConfig    0         /*!< Set to 1 if different SysTick Config is used    */
#define __FPU_PRESENT             1         /*!< FPU present                                     */
#define __NUM_CODE_PATCH          6
#define __NUM_LIT_PATCH           2

#define ARM_MATH_CM4              1

#include "core_cm4.h"                       /* Processor and core peripherals                    */

#ifndef __ASSEMBLER__

#include "system_ARMCM4.h"                  /* System Header                                     */

#endif

/* ================================================================================ */
/* ================       Device Specific Peripheral Section       ================ */
/* ================================================================================ */

/* -------------------  Start of section using anonymous unions  ------------------ */
#if   defined (__CC_ARM)
  #pragma push
  #pragma anon_unions
#elif defined (__ICCARM__)
  #pragma language=extended
#elif defined (__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined (__TMS470__)
  /* anonymous unions are enabled by default */
#elif defined (__TASKING__)
  #pragma warning 586
#elif defined (__CSMC__)
  /* anonymous unions are enabled by default */
#else
  #warning Not supported compiler type
#endif

/* --------------------  End of section using anonymous unions  ------------------- */
#if   defined (__CC_ARM)
  #pragma pop
#elif defined (__ICCARM__)
  /* leave anonymous unions enabled */
#elif defined (__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined (__TMS470__)
  /* anonymous unions are enabled by default */
#elif defined (__TASKING__)
  #pragma warning restore
#elif defined (__CSMC__)
  /* anonymous unions are enabled by default */
#else
  #warning Not supported compiler type
#endif

#ifdef __cplusplus
}
#endif

#endif
