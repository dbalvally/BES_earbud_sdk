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
int talk_thru_monitor_init(void);
int talk_thru_monitor_run(short *pcm_buf_left, short *pcm_buf_right, int pcm_len);;
int talk_thru_monitor_deinit(void);
void bt_peak_detector_run(short *buf, uint32_t len);
void bt_peak_detector_sbc_init(void);