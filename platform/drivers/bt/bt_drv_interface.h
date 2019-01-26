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
#ifndef __BT_DRV_INTERFACE_H__
#define  __BT_DRV_INTERFACE_H__

#include "stdint.h"
#include "stdbool.h"

// CARE: round-off error
#define BT_CLK_UNIT 312.5       //us
#define BT_CLK_UNIT_2X 625       //us
#define BT_CLK_UNIT_10X 3125       //us
#define US_TO_BTCLKS(us)    ((uint64_t)(us) * 2 / BT_CLK_UNIT_2X)
#define BTCLKS_TO_US(n)     ((uint64_t)(n) * BT_CLK_UNIT_2X / 2)

#define DUT_CONNECT_STATUS_CONNECTED (1)
#define DUT_CONNECT_STATUS_DISCONNECTED (2)

void btdrv_hciopen(void);
void btdrv_hcioff(void);
void btdrv_start_bt(void);
void bt_drv_extra_config_after_init(void);
void btdrv_rf_init_ext(void);

void btdrv_stop_bt(void);
void btdrv_testmode_start(void);
void btdrv_enable_dut(void);
void btdrv_hci_reset(void);
void btdrv_enable_nonsig_tx(uint8_t index);
void btdrv_enable_nonsig_rx(uint8_t index);
void btdrv_vco_test_start(uint8_t chnl);
void btdrv_vco_test_stop(void);
void bt_drv_calib_open(void);
void bt_drv_check_calib(void);
void bt_drv_calib_close(void);
int bt_drv_calib_result_porc(uint32_t *capval);
void bt_drv_calib_rxonly_porc(void);
void btdrv_write_localinfo(char *name, uint8_t len, uint8_t *addr);

#ifdef __cplusplus
extern "C" {
#endif

void bt_drv_rf_reset(void);
void btdrv_sleep_config(uint8_t sleep_en);
void btdrv_feature_default(void);
void btdrv_test_mode_addr_set(void);

int btdrv_meinit_param_init(void);
int btdrv_meinit_param_remain_size_get(void);
int btdrv_meinit_param_next_entry_get(uint32_t *addr, uint32_t *val);

void btdrv_store_device_role(bool slave);
bool btdrv_device_role_is_slave(void);

uint32_t btdrv_rf_get_max_xtal_tune_ppb(void);
uint32_t btdrv_rf_get_xtal_tune_factor(void);
void btdrv_rf_init_xtal_fcap(uint32_t fcap);
uint32_t btdrv_rf_get_init_xtal_fcap(void);
uint32_t btdrv_rf_get_xtal_fcap(void);
void btdrv_rf_set_xtal_fcap(uint32_t fcap , uint8_t is_direct);
int btdrv_rf_xtal_fcap_busy(uint8_t is_direct);

void btdrv_rf_bit_offset_track_enable(bool enable);
void btdrv_rf_bit_offset_track_force_disable(void);
uint32_t btdrv_rf_bit_offset_get(void);
void btdrv_uart_bridge_loop(void);
void btdrv_testmode_data_overide(void);

void btdrv_set_bt_pcm_triggler_delay(uint8_t delay);
void btdrv_set_bt_pcm_en(uint8_t  en);

uint32_t btdrv_rf_bitoffset_get(uint16_t conidx);
uint16_t bt_drv_reg_op_get_bitoffset(uint16_t conhandle);

void  btdrv_syn_clr_trigger(void);

uint32_t btdrv_rf_bit_offset_get(void);
uint32_t btdrv_syn_get_curr_ticks(void);
uint32_t bt_syn_get_curr_ticks(uint16_t conhdl);
void bt_syn_set_tg_ticks(uint32_t val,uint16_t conhdl);
void btdrv_syn_trigger_codec_en(uint32_t v);
uint32_t btdrv_get_syn_trigger_codec_en(void);
uint32_t btdrv_get_trigger_ticks(void);


void btdrv_rf_trig_patch_enable(bool enable);
void btdrv_tws_trig_role(uint8_t role);
void btdrv_rf_set_conidx(uint32_t conidx);



#define ACL_TRIGGLE_MODE       1
#define SCO_TRIGGLE_MODE      2
void btdrv_enable_playback_triggler(uint8_t triggle_mode);
void btdrv_set_bt_pcm_triggler_en(uint8_t  en);
void btdrv_set_bt_pcm_triggler_delay(uint8_t  delay);
void btdrv_set_bt_pcm_triggler_delay_reset(uint8_t  delay);
void btdrv_set_pcm_data_ignore_crc(void);


uint8_t btdrv_conhdl_to_linkid(uint16_t connect_hdl);
void btdrv_set_tws_role_triggler(uint8_t tws_mode);
void btdrv_ins_patch_test_init(void);
void btdrv_dynamic_patch_moble_disconnect_reason_hacker(uint16_t hciHandle);
#define BT_TX_1M  1
#define BT_TX_2M  2
#define BT_TX_3M  4
void btdrv_current_packet_type_hacker(uint16_t hciHandle,uint8_t rate);
void btdrv_dynamic_patch_sco_status_clear(void);
uint32_t btdrv_dut_get_connect_status(void);

void btdrv_seq_bak_mode(uint8_t mode,uint8_t linkid);

#ifdef CHIP_BEST2300
void btdrv_linear_format_16bit_set(void);
void btdrv_pcm_enable(void);
void btdrv_pcm_disable(void);
void btdrv_spi_trig_data_change(uint8_t spi_sel, uint8_t index, uint32_t value);
void btdrv_spi_trig_data_set(uint8_t spi_sel);
uint8_t btdrv_get_spi_trig_enable(void);
void btdrv_set_spi_trig_enable(void);
void btdrv_clear_spi_trig_enable(void);
void btdrv_spi_trig_init(void);
void btdrv_spi_trig_select(uint8_t link_id, bool spi1_set);
void bt_drv_reg_cs_monitor(void);
#endif// CHIP_BEST2300
bool btdrv_rf_rx_gain_adjust_getstatus(void);
bool btdrv_rf_blerx_gain_adjust_getstatus(void);
void btdrv_rf_blerx_gain_adjust_default(void);
void btdrv_rf_blerx_gain_adjust_lowgain(void);

#define BTCLK_STATUS_PRINT()    do{ \
    LOG_PRINT_TWS("[%s] %d: curr bt time= %d", __func__, __LINE__, bt_syn_get_curr_ticks(app_tws_get_tws_conhdl())); \
                                }while(0)

#define IS_ENABLE_BT_DRIVER_REG_DEBUG_READING	0

#define BT_DRIVER_GET_U8_REG_VAL(regAddr)		(*(uint8_t *)(regAddr))
#define BT_DRIVER_GET_U16_REG_VAL(regAddr)		(*(uint16_t *)(regAddr))
#define BT_DRIVER_GET_U32_REG_VAL(regAddr)		(*(uint32_t *)(regAddr))

#define BT_DRIVER_PUT_U8_REG_VAL(regAddr, val)		*(uint8_t *)(regAddr) = (val)
#define BT_DRIVER_PUT_U16_REG_VAL(regAddr, val)		*(uint16_t *)(regAddr) = (val)
#define BT_DRIVER_PUT_U32_REG_VAL(regAddr, val)		*(uint32_t *)(regAddr) = (val)

#ifdef __cplusplus
}
#endif

#endif

