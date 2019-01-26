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
#include "plat_addr_map.h"
#include "string.h"
#include "hal_cmu.h"
#include "hal_timer.h"
#include "hal_dma.h"
#include "hal_iomux.h"
#include "hal_wdt.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "hal_trace.h"
#include "cmsis.h"
#include "hwtimer_list.h"
#include "pmu.h"
#include "analog.h"
#include "apps.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"
#include "hal_norflash.h"
#ifdef RTOS
#include "cmsis_os.h"
#include "rt_Time.h"
#include "app_factory.h"
#endif
#include "stdio.h"
#include "os_api.h"
#if defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
extern "C" {
#include "stdarg.h"
#include "patch.h"
}
#endif

#include "log_section.h"

#ifdef FIRMWARE_REV
#define SYS_STORE_FW_VER(x) \
      if(fw_rev_##x) { \
        *fw_rev_##x = fw.softwareRevByte##x; \
      }

typedef struct
{
	uint8_t softwareRevByte0;
	uint8_t softwareRevByte1;
	uint8_t softwareRevByte2;
	uint8_t softwareRevByte3;
} FIRMWARE_REV_INFO_T;

static FIRMWARE_REV_INFO_T fwRevInfoInFlash __attribute((section(".fw_rev"))) = {0, 0, 1, 0};
FIRMWARE_REV_INFO_T fwRevInfoInRam;

extern "C" void system_get_info(uint8_t *fw_rev_0, uint8_t *fw_rev_1,
    uint8_t *fw_rev_2, uint8_t *fw_rev_3)
{
  FIRMWARE_REV_INFO_T fw = fwRevInfoInFlash;

  SYS_STORE_FW_VER(0);
  SYS_STORE_FW_VER(1);
  SYS_STORE_FW_VER(2);
  SYS_STORE_FW_VER(3);
}
#endif

#define  system_shutdown_wdt_config(seconds) \
                        do{ \
                            hal_wdt_stop(HAL_WDT_ID_0); \
                            hal_wdt_set_irq_callback(HAL_WDT_ID_0, NULL); \
                            hal_wdt_set_timeout(HAL_WDT_ID_0, seconds); \
                            hal_wdt_start(HAL_WDT_ID_0); \
                            hal_sleep_set_deep_sleep_hook(HAL_DEEP_SLEEP_HOOK_USER_0, NULL); \
                        }while(0)

static osThreadId main_thread_tid = NULL;

#define CHIP_UNIQUE_INFO_LEN	12
uint8_t chip_unique_info[CHIP_UNIQUE_INFO_LEN];
void chip_unique_info_init(void)
{
	uint8_t index = 0;
	uint16_t value;
	pmu_get_efuse(PMU_EFUSE_PAGE_BATTER_LV, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
	index += sizeof(value);

	pmu_get_efuse(PMU_EFUSE_PAGE_BATTER_HV, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
	index += sizeof(value);

	pmu_get_efuse(PMU_EFUSE_PAGE_DCCALIB2_L, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
	index += sizeof(value);

	pmu_get_efuse(PMU_EFUSE_PAGE_DCCALIB2_R, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
	index += sizeof(value);

	pmu_get_efuse(PMU_EFUSE_PAGE_DCCALIB_L, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
	index += sizeof(value);

	pmu_get_efuse(PMU_EFUSE_PAGE_DCCALIB_R, &value);

	memcpy(&chip_unique_info[index], (uint8_t *)&value, sizeof(value));
}

extern "C" int chip_unique_get_word(void)
{
    int *p = (int *)chip_unique_info;
    return *p;
}
#define SYS_STORE_FW_VER(x) \
  if(fw_rev_##x) { \
    *fw_rev_##x = fw.softwareRevByte##x; \
  }


extern "C" int system_shutdown(void)
{
    osThreadSetPriority(main_thread_tid, osPriorityRealtime);
    osSignalSet(main_thread_tid, 0x4);
    return 0;
}

int system_reset(void)
{
    osThreadSetPriority(main_thread_tid, osPriorityRealtime);
    osSignalSet(main_thread_tid, 0x8);
    return 0;
}

int tgt_hardware_setup(void)
{
    unsigned short read_value;
    //reg_bt_tst_buf_sel_out = 0;
    //reg_bt_tst_buf_sel_in = 0;
    hal_analogif_reg_read(0xa2, &read_value);
    read_value = read_value & ~(0x1f<<10);
    hal_analogif_reg_write(0xa2, read_value);

    hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)cfg_hw_pinmux_pwl, sizeof(cfg_hw_pinmux_pwl)/sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP));
    if (app_battery_ext_charger_indicator_cfg.pin != HAL_IOMUX_PIN_NUM){
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&app_battery_ext_charger_indicator_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_battery_ext_charger_indicator_cfg.pin, HAL_GPIO_DIR_OUT, 0);
    }
    return 0;
}

#ifdef FPGA
uint32_t a2dp_audio_more_data(uint8_t *buf, uint32_t len);
uint32_t a2dp_audio_init(void);
extern "C" void app_audio_manager_open(void);
extern "C" void app_bt_init(void);
extern "C" uint32_t hal_iomux_init(struct HAL_IOMUX_PIN_FUNCTION_MAP *map, uint32_t count);
void app_overlay_open(void);

volatile uint32_t ddddd = 0;

#endif

#if defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
struct patch_func_item {
    void *old_func;
    void *new_func;
};
static uint32_t ALIGNED(0x100) remap_base[256];
char p_romsbc_trace_buffer[128];
int p_hal_trace_printf_without_crlf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(p_romsbc_trace_buffer,fmt,args);
    va_end(args);

    TRACE("%s\n", p_romsbc_trace_buffer);

    return 0;
}
void p_OS_MemSet(U8 *dest, U8 byte, U32 len)
{
	memset(dest, byte, len);
}
void p_OS_Assert(const char *expression, const char *file, U16 line)
{
    ASSERT(0,"OS_Assert exp: %s, func: %s, line %d\r\n", expression, file, line);
    while(1);
}
void* p_memcpy(void *dst,const void *src, size_t length)
{
	return memcpy(dst, src, length);
}
struct patch_func_item romsbc_patchs[] = {
    // OS_Memset
    {(void *)0x0003a54d, (void *)p_OS_MemSet},
    // OS_Assert
    {(void *)0x0003a571, (void *)p_OS_Assert},
    // hal_trace_printf_without_crlf
    {(void *)0x0003a601, (void *)p_hal_trace_printf_without_crlf},
    // memcpy
    {(void *)0x0003a635, (void *)p_memcpy},
};

void init_2200_romprofile_sbc_patch(void)
{
    uint32_t i = 0;
    patch_open((uint32_t)remap_base);
    for(i = 0; i < sizeof(romsbc_patchs)/sizeof(struct patch_func_item); ++i) {
        patch_enable(PATCH_TYPE_FUNC, (uint32_t)romsbc_patchs[i].old_func, (uint32_t)romsbc_patchs[i].new_func);
    }
}
#endif


#if defined(CHIP_BEST2000) && defined(__SBC_FUNC_IN_ROM_VBEST2000__) && !defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
extern "C" {
#include "extern_functions.h"
#include "osapi.h"
#include "math.h"
void rom1_entry(struct __extern_functions__ *efp);
void DDB_List_Saved_Flash(void);
void DDBlist_Erased_Flash(void);
void BESHCI_SendData(void *packet);
void BESHCI_BufferAvai(void *packet);
int btdrv_meinit_param_remain_size_get(void);
int btdrv_meinit_param_next_entry_get(uint32_t *addr, uint32_t *val);
int btdrv_meinit_param_init(void);
void speex_echo_get_residual(void *st, void *residual_echo, int len);
double __aeabi_i2d (int Value);
float __aeabi_d2f(double Value);
double __aeabi_f2d(float Value);
int __aeabi_d2iz(double Value);
double exp(double x);
double log(double x);
double floor(double x);
}
struct __extern_functions__ ef = {
    // bes_os
    .OS_Init = OS_Init,
    .OS_GetSystemTime = OS_GetSystemTime,
    .OS_Rand = OS_Rand,
    .OS_StopHardware = OS_StopHardware,
    .OS_ResumeHardware = OS_ResumeHardware,
    .OS_MemCopy = OS_MemCopy,
    .OS_MemCmp = OS_MemCmp,
    .OS_MemSet = OS_MemSet,
    .OS_StrCmp = OS_StrCmp,
    .OS_StrLen = OS_StrLen,
    .OS_Assert = NULL,
    .OS_LockStack = OS_LockStack,
    .OS_UnlockStack = OS_UnlockStack,
    .OS_NotifyEvm = OS_NotifyEvm,
    .OS_StartTimer = OS_StartTimer,
    .OS_CancelTimer = OS_CancelTimer,

    // bes_ddb
#if 0
    .DDB_List_Saved_Flash = DDB_List_Saved_Flash,
    .DDBlist_Erased_Flash = DDBlist_Erased_Flash,
#else
    .DDB_List_Saved_Flash = NULL,
    .DDBlist_Erased_Flash = NULL,
#endif
    .DDB_Close = DDB_Close,
    .DDB_AddRecord = DDB_AddRecord,
    .DDB_Open = DDB_Open,
    .DDB_FindRecord = DDB_FindRecord,
    .DDB_DeleteRecord = DDB_DeleteRecord,
    .DDB_EnumDeviceRecords = DDB_EnumDeviceRecords,

    // misc
    .hal_trace_printf_without_crlf_fix_arg = hal_trace_printf_without_crlf_fix_arg,
    .vsprintf = vsprintf,
    .memcpy = memcpy,
    .BESHCI_SendData = BESHCI_SendData,
    .BESHCI_BufferAvai = BESHCI_BufferAvai,
#if 0
    .SPPOS_LockDev;
    .SPPOS_UnlockDev;
    .SPPOS_FlushRx;
    .SPPOS_FlushTx;
    .SPPOS_ReadBuf;
    .SPPOS_WriteBuf;
    .SPPOS_Ioctl;
    .SPPOS_RxBytes;
    .SPPOS_DevTx;
    .SPPOS_RxFree;
    .SPPOS_DevRx;
    .SPPOS_ReturnBuf;
#else
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#endif
    .btdrv_meinit_param_remain_size_get = btdrv_meinit_param_remain_size_get,
    .hal_get_chip_metal_id = hal_get_chip_metal_id,
    .btdrv_meinit_param_next_entry_get = btdrv_meinit_param_next_entry_get,
    .hal_trace_output = hal_trace_output,
    .btdrv_meinit_param_init = btdrv_meinit_param_init,
    .hal_trace_dump = hal_trace_dump,
    .speex_echo_get_residual = speex_echo_get_residual,
    .pow = pow,
    .sqrt = sqrt,
    .exit = exit,
    .memset = memset,
    .__aeabi_i2d = __aeabi_i2d,
    .__aeabi_d2f = __aeabi_d2f,
    .__aeabi_f2d = __aeabi_f2d,
    .__aeabi_d2iz = __aeabi_d2iz,
    .exp = exp,
    .log = log,
    .floor = floor,
};
#endif

int main(int argc, char *argv[])
{
    uint8_t sys_case = 0;
    int ret = 0;
#ifdef __FACTORY_MODE_SUPPORT__
    uint32_t bootmode = hal_sw_bootmode_get();
#endif

#if defined(__SBC_FUNC_IN_ROM_VBEST2000__) && !defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
    set__impure_ptr(_impure_ptr);
    rom1_entry(&ef);
#endif

#if defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
    init_2200_romprofile_sbc_patch();
#endif

    tgt_hardware_setup();

    main_thread_tid = osThreadGetId();

    hwtimer_init();

    hal_dma_set_delay_func((HAL_DMA_DELAY_FUNC)osDelay);
    hal_audma_open();
    hal_gpdma_open();

#ifdef DEBUG
#if (DEBUG_PORT == 1)
    hal_iomux_set_uart0();
#ifdef __FACTORY_MODE_SUPPORT__
    if (!(bootmode & HAL_SW_BOOTMODE_FACTORY))
#endif
    {
        hal_trace_open(HAL_TRACE_TRANSPORT_UART0);

    }
#endif

#if (DEBUG_PORT == 2)
#ifdef __FACTORY_MODE_SUPPORT__
    if (!(bootmode & HAL_SW_BOOTMODE_FACTORY))
#endif
    {
        hal_iomux_set_analog_i2c();
    }
    hal_iomux_set_uart1();
    hal_trace_open(HAL_TRACE_TRANSPORT_UART1);
#endif
    hal_sleep_start_stats(10000, 10000);
#else
#if (DEBUG_PORT == 1)
#ifdef __FACTORY_MODE_SUPPORT__
    if (!(bootmode & HAL_SW_BOOTMODE_FACTORY))
#endif
    {
        hal_trace_open(HAL_TRACE_TRANSPORT_UART0);
    }
#endif

#if (DEBUG_PORT == 2)
    hal_trace_open(HAL_TRACE_TRANSPORT_UART1);
#endif

#ifndef FPGA
    uint8_t flash_id[HAL_NORFLASH_DEVICE_ID_LEN];
    hal_norflash_get_id(HAL_NORFLASH_ID_0, flash_id, ARRAY_SIZE(flash_id));
    TRACE("FLASH_ID: %02X-%02X-%02X", flash_id[0], flash_id[1], flash_id[2]);
    ASSERT(hal_norflash_opened(HAL_NORFLASH_ID_0), "Failed to init flash: %d", hal_norflash_get_open_state(HAL_NORFLASH_ID_0));
#endif
#endif

    hal_iomux_ispi_access_init();

    pmu_open();

    analog_open();
    TRACE("_debug: BT_LOCAL_NAME = %s",BT_LOCAL_NAME);
// with OTA support
#if defined(FLASH_REGION_BASE) && FLASH_REGION_BASE > FLASH_BASE
	// as OTA bin will load the factory data and user data from the bottom TWO sectors from the flash,
	// the FLASH_SIZE defined is the common.mk must be the actual chip flash size, otherwise the ota will load the
	// wrong information
	uint32_t actualFlashSize = hal_norflash_get_flash_total_size(HAL_NORFLASH_ID_0);
	if (FLASH_SIZE != actualFlashSize)
	{
		TRACE("Wrong FLASH_SIZE defined in common.mk!");
		TRACE("FLASH_SIZE is defined as 0x%x while the actual chip flash size is 0x%x!", FLASH_SIZE, actualFlashSize);
		TRACE("Please change the FLASH_SIZE in common.mk to 0x%x to enable the OTA feature.", actualFlashSize);
		ASSERT(false, "%s,%d.",__func__,__LINE__);
	}
#endif

#ifdef FPGA

    TRACE("\n[best of best of best...]\n");
    TRACE("\n[ps: w4 0x%x,2]", &ddddd);

    ddddd = 1;
    while (ddddd == 1);
    TRACE("bt start");

    list_init();

    a2dp_audio_init();
    IabtInit();
    app_os_init();

    af_open();
    app_audio_open();
    app_audio_manager_open();
    app_overlay_open();

    app_bt_init();

    SAFE_PROGRAM_STOP();

#else // !FPGA

#if DEBUG_LOG_ENABLED||USER_STATISTIC_DATA_LOG_ENABLED
    log_section_init();
#endif

#ifdef __FACTORY_MODE_SUPPORT__
    if (bootmode & HAL_SW_BOOTMODE_FACTORY){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_FACTORY);
        ret = app_factorymode_init(bootmode);

    }else if(bootmode & HAL_SW_BOOTMODE_CALIB){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_CALIB);
        app_factorymode_calib_only();
    }else
#endif
    {
#ifdef FIRMWARE_REV
        fwRevInfoInRam = fwRevInfoInFlash;
        TRACE("The Firmware rev is %d.%d.%d.%d", 
        fwRevInfoInRam.softwareRevByte0,
        fwRevInfoInRam.softwareRevByte1,
        fwRevInfoInRam.softwareRevByte2,
        fwRevInfoInRam.softwareRevByte3);
#endif
        ret = app_init();
    }

    if (!ret){
#if defined(_AUTO_TEST_)
        AUTO_TEST_SEND("BT Init ok.");
#endif
        while(1)
        {
            osEvent evt;
#ifndef __POWERKEY_CTRL_ONOFF_ONLY__
            osSignalClear (main_thread_tid, 0x0f);
#endif
            //wait any signal
            evt = osSignalWait(0x0, osWaitForever);

            //get role from signal value
            if(evt.status == osEventSignal)
            {
                if(evt.value.signals & 0x04)
                {
                    sys_case = 1;
                    break;
                }
                else if(evt.value.signals & 0x08)
                {
                    sys_case = 2;
                    break;
                }
            }else{
                sys_case = 1;
                break;
            }
         }
    }
#ifdef __WATCHER_DOG_RESET__
    system_shutdown_wdt_config(10);
#endif
    app_deinit(ret);
    TRACE("byebye~~~ %d\n", sys_case);
    if ((sys_case == 1)||(sys_case == 0)){
        TRACE("shutdown\n");
#if defined(_AUTO_TEST_)
        AUTO_TEST_SEND("System shutdown.");
        osDelay(50);
#endif
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
        pmu_shutdown();
    }else if (sys_case == 2){
        TRACE("reset\n");
#if defined(_AUTO_TEST_)
        AUTO_TEST_SEND("System reset.");
        osDelay(50);
#endif
        hal_cmu_sys_reboot();
    }

#endif // !FPGA
    return 0;
}

