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
#ifndef _CODEC_SBC_H
#define _CODEC_SBC_H

#include "bluetooth.h"

#define BTIF_SBC_USE_FIXED_POINT_ENABLED


#ifdef BTIF_SBC_USE_FIXED_POINT_ENABLED
typedef S32 REAL;
#else
typedef float REAL;
#endif

typedef U8 btif_sbc_sample_freq_t;

#define BTIF_SBC_CHNL_SAMPLE_FREQ_16    0
#define BTIF_SBC_CHNL_SAMPLE_FREQ_32    1
#define BTIF_SBC_CHNL_SAMPLE_FREQ_44_1  2
#define BTIF_SBC_CHNL_SAMPLE_FREQ_48    3

typedef U8 btif_sbc_channel_mode_t;

#define BTIF_SBC_CHNL_MODE_MONO          0
#define BTIF_SBC_CHNL_MODE_DUAL_CHNL     1
#define BTIF_SBC_CHNL_MODE_STEREO        2
#define BTIF_SBC_CHNL_MODE_JOINT_STEREO  3

typedef U8 btif_sbc_alloc_method_t;

#define BTIF_SBC_ALLOC_METHOD_LOUDNESS   0
#define BTIF_SBC_ALLOC_METHOD_SNR        1

#define BTIF_SBC_MAX_NUM_BLK     16
#define BTIF_SBC_MAX_NUM_SB       8
#define BTIF_SBC_MAX_NUM_CHNL     2
#define BTIF_SBC_MAX_PCM_DATA  512

#define BTIF_SBC_DEC_ALL 0
#define BTIF_SBC_DEC_LEFT 1
#define BTIF_SBC_DEC_RIGHT 2

#define BTIF_SBC_SPLIT_LEFT 1
#define BTIF_SBC_SPLIT_RIGHT 2



#define BTIF_MSBC_BLOCKS	15

#define BTIF_A2DP_SBC_OCTET_NUMBER      (4)


typedef U8 btif_sbc_alloc_method_t;

#define BTIF_SBC_ALLOC_METHOD_LOUDNESS   0
#define BTIF_SBC_ALLOC_METHOD_SNR        1

typedef struct {

    U8 bitPool;

    /* Sampling frequency of the stream */
    btif_sbc_sample_freq_t sampleFreq;

    /* The channel (Mono, Stereo, etc.) */
    btif_sbc_channel_mode_t channelMode;

    /* The allocation method for the stream */
    btif_sbc_alloc_method_t allocMethod;

    /* Number of blocks used to encode the stream (4, 8, 12, or 16) */
    U8 numBlocks;

    /* The number of subbands in the stream (4 or 8) */
    U8 numSubBands;

    /* The number of channels in the stream (calculated from channelMode) */
    U8 numChannels;

    /* The flag of msbc */
    U8 mSbcFlag;

    /* === Internal use only === */
    U16 bitOffset;
    U8 crc;
    U8 fcs;
    U8 join[BTIF_SBC_MAX_NUM_SB];
    U8 scale_factors[BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    S32 scaleFactors[BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    U16 levels[BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    S8 bitNeed0[BTIF_SBC_MAX_NUM_SB];
    S8 bitNeed1[BTIF_SBC_MAX_NUM_SB];
    U8 bits[BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    REAL sbSample[BTIF_SBC_MAX_NUM_BLK][BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    U8 fcs_bak;
} btif_sbc_stream_info_t;
typedef struct {

    U8 bitPool;

    uint8_t sampleFreq;

    uint8_t channelMode;

    uint8_t allocMethod;

    U8 numBlocks;

    U8 numSubBands;

    U8 numChannels;

    U8 mSbcFlag;

} btif_sbc_stream_info_short_t;

#if BTIF_SBC_ENCODER == BTIF_ENABLED

typedef struct {

    btif_sbc_stream_info_t streamInfo;
    U8 sFactorsJoint[BTIF_SBC_MAX_NUM_CHNL][BTIF_SBC_MAX_NUM_SB];
    REAL sbJoint[BTIF_SBC_MAX_NUM_BLK][BTIF_SBC_MAX_NUM_CHNL];
    S16 X0[80 * 2];
    S16 X1[80 * 2];
    U16 X0pos, X1pos;
    REAL Y[16];
} btif_sbc_encoder_t;

#endif


typedef struct {

    btif_sbc_stream_info_t streamInfo;

    U16 maxPcmLen;
    REAL V0[160];
    REAL V1[160];

    struct {
        U8 stageBuff[BTIF_SBC_MAX_PCM_DATA];    /* Staging buffer            */
        U16 stageLen;           /* Length of staged data     */
        U16 curStageOff;        /* Offset into staged data   */
        U8 *rxBuff;             /* The Received buffer       */
        U16 rxSize;             /* Remaining rx buff size    */
        U8 rxState;             /* Parser state              */
    } parser;

} btif_sbc_decoder_t;

#define BTIF_BT_PACKET_HEADER_LEN 25

typedef struct {
    list_entry_t node;
    uint8_t *data;              /* Points to a buffer of user data.  */
    uint16_t dataLen;           /* Indicates the length of "data" in bytes. */
    uint16_t flags;             /* Must be initialized to BTP_FLAG_NONE by
                                 * applications running on top of L2CAP.
                                 */

#if   BTIF_L2CAP_PRIORITY == BTIF_ENABLED
    BtPriority priority;
#endif

    /* Group: The following fields are for internal use only by the stack. */
    void *ulpContext;
    uint8_t *tail;
    uint16_t tailLen;

#ifdef  BTIF_XA_STATISTICS
    U32 rfc_timer;
    U32 hci_timer;
    U32 l2cap_timer;

#endif
    uint16_t llpContext;
    uint16_t remoteCid;

#if  BTIF_L2CAP_NUM_ENHANCED_CHANNELS > 0
    uint8_t segStart;
    uint16_t segNum;
    uint16_t segCount;
    uint8_t fcs[2];

#endif
    uint8_t hciPackets;
    uint8_t headerLen;
    uint8_t header[BTIF_BT_PACKET_HEADER_LEN];
} btif_bt_packet_t;

typedef struct {
    list_entry_t node;
    uint8_t *data;              /* transmit data ponter */
    uint16_t dataLen;           /*length of transmit data  */
    uint16_t frameSize;         /* size of an  frame     */

    btif_bt_packet_t packet;
    uint16_t dataSent;
    uint16_t frmDataSent;
} btif_a2dp_sbc_packet_t;


typedef struct  {

    /* Sampling frequency of the stream */
    btif_sbc_sample_freq_t  sampleFreq;

    /* The number of channels in the stream  */
    uint8_t             numChannels;

    /* The length of decode PCM audio data (in bytes) */
    uint16_t            dataLen;

    /* Decoded 16 bit PCM data, memory
     * must be allocated by the application
     */
    uint8_t            *data;

} btif_sbc_pcm_data_t;


#ifdef __cplusplus
extern "C" {
#endif                          /*  */

uint16_t btif_sbc_frame_len(btif_sbc_stream_info_t *StreamInfo);

#if BTIF_SBC_DECODER == BTIF_ENABLED


void btif_sbc_init_decoder(btif_sbc_decoder_t *Decoder);


bt_status_t btif_sbc_decode_frames(btif_sbc_decoder_t *Decoder,
                          uint8_t         *Buff,
                          uint16_t         Len,
                          uint16_t        *BytesParsed,
                          btif_sbc_pcm_data_t *PcmData,
                          uint16_t         MaxPcmData,
                          float*       gains);



bt_status_t btif_sbc_decode_frames_out_sbsamples(btif_sbc_decoder_t *Decoder,
        uint8_t		 *Buff,
        uint16_t		  Len,
        uint16_t		 *BytesDecoded,
        btif_sbc_pcm_data_t *PcmData,
        uint16_t		  MaxPcmData,
        float* gains,
        uint8_t     ChooseDecChannel,
        REAL       *SBSamplesBuf,
        uint32_t        SBSamplesBufLen,
        uint32_t        *SBSamplesBufUsed,
        uint8_t     ChooseSplitChannel);

#endif /* SBC_DECODER == XA_ENABLED */

#if BTIF_SBC_ENCODER == BTIF_ENABLED


void btif_sbc_init_encoder(btif_sbc_encoder_t *Encoder);


bt_status_t btif_sbc_encode_frames(btif_sbc_encoder_t *Encoder,
                          btif_sbc_pcm_data_t *PcmData,
                          uint16_t        *BytesEncoded,
                          uint8_t         *Buff,
                          uint16_t        *Len,
                          uint16_t         MaxSbcData);

bt_status_t  btif_sbc_encode_frames_with_sbsamples(btif_sbc_encoder_t *Encoder,
						  REAL       *SBSamplesBuf,
						  uint32_t		 SBSamplesBufLen_bytes,
						  uint32_t		 *SBSamplesBufUsed_bytes,
						  uint8_t		 *Buff,
						  uint16_t		 *Len,
						  uint16_t		  MaxSbcData,
						  uint8_t      *number_freame_encoded);


#endif  /* SBC_ENCODER == XA_ENABLED */



#ifdef __cplusplus
}
#endif

#endif

