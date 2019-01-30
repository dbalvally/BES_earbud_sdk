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
#include "cmsis.h"
#include "bt_drv_reg_op.h"
#include "bt_drv_internal.h"
#include "bt_drv_interface.h"
#include "bt_drv.h"
#include "hal_chipid.h"
#include "hal_trace.h"
#include "hal_iomux.h"
#include <string.h>

//define for DEBUG
#define EM_BT_CLKOFF0_MASK   ((uint16_t)0x0000FFFF)
#define ASSERT_ERR(cond)                             { if (!(cond)) { LOG_PRINT_BT_DRIVER("line is %d file is %s", __LINE__, __FILE__); } }
#define abs(x)  (x>0?x:(-x))

#define BT_DRIVER_GET_U8_REG_VAL(regAddr)       (*(uint8_t *)(regAddr))
#define BT_DRIVER_GET_U16_REG_VAL(regAddr)      (*(uint16_t *)(regAddr))
#define BT_DRIVER_GET_U32_REG_VAL(regAddr)      (*(uint32_t *)(regAddr))

#define BT_DRIVER_PUT_U8_REG_VAL(regAddr, val)      *(uint8_t *)(regAddr) = (val)
#define BT_DRIVER_PUT_U16_REG_VAL(regAddr, val)     *(uint16_t *)(regAddr) = (val)
#define BT_DRIVER_PUT_U32_REG_VAL(regAddr, val)     *(uint32_t *)(regAddr) = (val)

void bt_drv_reg_op_rssi_set(uint16_t rssi)
{
}

void bt_drv_reg_op_scan_intv_set(uint32_t scan_intv)
{
}

void bt_drv_reg_op_encryptchange_errcode_reset(uint16_t hci_handle)
{

}

void bt_drv_reg_op_sco_sniffer_checker(void)
{
}

void bt_drv_reg_op_trigger_time_checker(void)
{
    BT_DRV_REG_OP_ENTER();
    LOG_PRINT_BT_DRIVER("0xd02201f0 = %x",*(volatile uint32_t *)0xd02201f0);
    BT_DRV_REG_OP_EXIT();
}

void bt_drv_reg_op_tws_output_power_fix_separate(uint16_t hci_handle, uint16_t pwr)
{
}

#define SNIFF_IN_SCO    2
///BD Address structure
struct bd_addr
{
    ///6-byte array address value
    uint8_t  addr[6];
};
///device info structure
struct ld_device_info
{
    struct bd_addr bd_addr;
    uint8_t link_id;
    uint8_t state;
};

bool bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_get(void)
{
    bool nRet = false;
    return nRet;
}

void bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_set(bool state)
{
}

int bt_drv_reg_op_currentfreeaclbuf_get(void)
{
    BT_DRV_REG_OP_ENTER();

    if(hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_0 || hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_1)
    {
        return (*(volatile uint16_t *)0xC0006338);
    }
    else 
    {
        return (*(volatile uint16_t *)0xC0006638);//hci_fc_env.curr_pkt_nb
    }
    
    BT_DRV_REG_OP_EXIT();
}
static uint16_t app_tws_original_mobile_airpath_info;
void bt_drv_reg_op_save_mobile_airpath_info(uint16_t hciHandle)
{
    BT_DRV_REG_OP_ENTER();

    uint8_t link_id = btdrv_conhdl_to_linkid(hciHandle);
    
    app_tws_original_mobile_airpath_info = BT_DRIVER_GET_U16_REG_VAL(0xd0211156 + 110*link_id);
    LOG_PRINT_BT_DRIVER("org mobile air info 0x%x ", app_tws_original_mobile_airpath_info);
    
    BT_DRV_REG_OP_EXIT();
}

void bt_drv_reg_op_block_xfer_with_mobile(uint16_t hciHandle)
{
    BT_DRV_REG_OP_ENTER();

    uint8_t link_id = btdrv_conhdl_to_linkid(hciHandle);
    
    BT_DRIVER_PUT_U16_REG_VAL((0xd0211156 + 110*link_id), app_tws_original_mobile_airpath_info^0x8000);

    LOG_PRINT_BT_DRIVER("block_xfer_with_mobile,hci handle 0x%x as 0x%x", hciHandle, app_tws_original_mobile_airpath_info^0x8000);

    BT_DRV_REG_OP_EXIT();    
}

void bt_drv_reg_op_resume_xfer_with_mobile(uint16_t hciHandle)
{
    BT_DRV_REG_OP_ENTER();
    uint8_t link_id = btdrv_conhdl_to_linkid(hciHandle);

    BT_DRIVER_PUT_U16_REG_VAL((0xd0211156 + 110*link_id), app_tws_original_mobile_airpath_info);
    LOG_PRINT_BT_DRIVER("resume xfer with mobile, hci handle 0x%x as 0x%x", hciHandle, app_tws_original_mobile_airpath_info);

    BT_DRV_REG_OP_EXIT();
}

int bt_drv_reg_op_packet_type_checker(uint16_t hciHandle)
{
    return 0;
}

void bt_drv_reg_op_max_slot_setting_checker(uint16_t hciHandle)
{}

void bt_drv_reg_op_force_task_dbg_idle(void)
{
}

void bt_drv_reg_op_afh_follow_mobile_mobileidx_set(uint16_t hciHandle)
{}


void bt_drv_reg_op_afh_follow_mobile_twsidx_set(uint16_t hciHandle)
{}

void bt_drv_reg_op_afh_bak_reset(void)
{}

void bt_drv_reg_op_afh_bak_save(uint8_t role, uint16_t mobileHciHandle)
{}

void bt_drv_reg_op_connection_checker(void)
{}

void bt_drv_reg_op_sco_status_store(void)
{}

void bt_drv_reg_op_sco_status_restore(void)
{}

void bt_drv_reg_op_afh_set_default(void)
{
}

void bt_drv_reg_op_ld_sniffer_master_addr_set(uint8_t * addr)
{
}

uint8_t msbc_find_tx_sync(uint8_t *buff)
{
    BT_DRV_REG_OP_ENTER();

    uint8_t i;
    for(i=0;i<60;i++)
    {
        if(buff[i]==0x1 && buff[(i+2)%60] == 0xad)
        {
        //    LOG_PRINT_BT_DRIVER("MSBC tx sync find =%d",i);
            return i;
        }
    }
    LOG_PRINT_BT_DRIVER("TX No pACKET");

    BT_DRV_REG_OP_EXIT();    
    return 0;
}

bool bt_drv_reg_op_sco_tx_buf_restore(uint8_t *trigger_test)
{
    bool nRet = false;
    return nRet;
}

extern "C" uint32_t hci_current_left_tx_packets_left(void);
extern "C" uint32_t hci_current_left_rx_packets_left(void);
extern "C" uint32_t hci_current_rx_packet_complete(void);
extern "C" uint8_t hci_current_rx_aclfreelist_cnt(void);
void bt_drv_reg_op_bt_packet_checker(void)
{
}

void bt_drv_reg_op_bt_info_checker(void)
{
}

uint32_t *rxbuff;
uint16_t *send_count;
uint16_t *free_count;
uint16_t *bt_send_count;

void bt_drv_reg_op_ble_buffer_cleanup(void)
{
}

struct ke_timer
{
    /// next ke timer
    struct ke_timer *next;
    /// message identifier
    uint16_t     id;
    /// task identifier
    uint16_t    task;
    /// time value
    uint32_t    time;
};

struct co_list_hdr
{
    /// Pointer to next co_list_hdr
    struct co_list_hdr *next;
};

/// structure of a list
struct co_list_con
{
    /// pointer to first element of the list
    struct co_list_hdr *first;
    /// pointer to the last element
    struct co_list_hdr *last;

    /// number of element in the list
    uint32_t cnt;
    /// max number of element in the list
    uint32_t maxcnt;
    /// min number of element in the list
    uint32_t mincnt;
};

struct mblock_free
{
    /// Next free block pointer
    struct mblock_free* next;
    /// Previous free block pointer
    struct mblock_free* previous;
    /// Size of the current free block (including delimiter)
    uint16_t free_size;
    /// Used to check if memory block has been corrupted or not
    uint16_t corrupt_check;
};

void bt_drv_reg_op_crash_dump(void)
{
}

uint8_t bt_drv_reg_op_get_tx_pwr(uint16_t connHandle)
{   
    return 0;
}

void bt_drv_reg_op_set_tx_pwr(uint16_t connHandle, uint8_t txpwr)
{
}

void bt_drv_reg_op_fix_tx_pwr(uint16_t connHandle)
{
}

void bt_drv_reg_op_lsto_hack(uint8_t link_id)
{
}

void bt_drv_reg_op_set_emsack_cs(uint16_t connHandle, uint8_t master)
{
}

void bt_drv_reg_op_enable_emsack_mode(uint16_t connHandle, uint8_t master, uint8_t enable)
{
}

void bt_drv_reg_op_set_accessible_mode(uint8_t mode)
{
}



void bt_drv_reg_op_set_swagc_mode(uint8_t mode)
{
}

void bt_drv_reg_op_set_reboot_pairing_mode(uint8_t mode)
{
}

void bt_drv_reg_op_force_retrans(bool enalbe)
{
    return;

    BT_DRV_REG_OP_ENTER();

    if (enalbe){
        BTDIGITAL_REG_SET_FIELD(0xd0220468,3,23,3);
    }else{
        BTDIGITAL_REG_SET_FIELD(0xd0220468,3,23,0);
    }
    BT_DRV_REG_OP_EXIT();
}

void bt_drv_reg_enable_pcm_tx_hw_cal(void)
{
}

void bt_drv_reg_monitor_clk(void)
{
}

int8_t bt_drv_read_rssi_in_dbm(uint16_t connHandle)
{
    int8_t rssi = 0;
    return rssi;
}


void bt_drv_reg_op_acl_silence(uint16_t connHandle, uint8_t silence)
{
}

 ///sniffer connect information environment structure
struct ld_sniffer_connect_env
{
    /// 
    uint16_t bitoff;
    ///
    uint32_t clk_offset;
    ///
    uint8_t map[10];    
    ///
    uint32_t afh_instant;
    ///
    uint8_t afh_mode;
    ///
    uint8_t enc_mode;
    ///
    uint8_t  ltk[16];
    ///
    uint8_t  role;
};

void bt_drv_reg_op_call_monitor(uint16_t connHandle, uint8_t tws_role)
{
}

void bt_drv_reg_op_lock_sniffer_sco_resync(void)
{
}

void bt_drv_reg_op_unlock_sniffer_sco_resync(void)
{
}

uint8_t bt_drv_reg_op_retx_att_nb_manager(uint8_t mode)
{
    return 0;
}



void bt_drv_reg_op_update_sniffer_bitoffset(uint16_t mobile_conhdl,uint16_t master_conhdl)
{

}

void bt_drv_reg_op_modify_bitoff_timer(uint16_t time_out)
{

}

#ifdef __LARGE_SYNC_WIN__
void bt_drv_reg_op_enlarge_sync_winsize(uint16_t conhdl, bool on)
{
}
#endif

void bt_drv_reg_cs_monitor(void)
{
}

void bt_drv_reg_op_ble_llm_substate_hacker(void)
{
}

bool  bt_drv_reg_op_check_esco_acl_sniff_conflict(uint16_t hciHandle)
{
    return false;
}

void bt_drv_reg_op_esco_acl_sniff_delay_cal(uint16_t hciHandle,bool enable)
{
    
}

void bt_drv_reg_op_set_tpoll(uint8_t linkid,uint16_t poll_interval)
{
}

int8_t  bt_drv_rssi_correction(int8_t rssi)
{
    return 0;
}