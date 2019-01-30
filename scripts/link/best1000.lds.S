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

#define HEAP_SECTION_SIZE			0x4000

#define STACK_SECTION_SIZE			0xD00

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#define OVERLAY_DATA_SECTION_SIZE	0xA000
#else
#define OVERLAY_DATA_SECTION_SIZE	0x8000
#endif

#if defined(CHIP_BEST2000)
#define FAST_XRAM_SECTION_SIZE		0xF000
#else
#if (RAM_SIZE >= 0x40000)
#if defined(BTUSB_AUDIO_MODE)
#define FAST_XRAM_SECTION_SIZE		0x18000
#else
#define FAST_XRAM_SECTION_SIZE		0x20000
#endif
#else
#define FAST_XRAM_SECTION_SIZE		0x8000
#endif
#endif

#ifndef FLASH_REGION_BASE
#define FLASH_REGION_BASE			FLASH_BASE
#endif
#ifndef FLASH_REGION_SIZE
#define FLASH_REGION_SIZE			(FLASH_SIZE - (FLASH_REGION_BASE - FLASH_BASE))
#endif
#define FLASH_NC_REGION_BASE		FLASH_C_TO_NC(FLASH_REGION_BASE)
#define FLASHX_REGION_BASE			FLASH_TO_FLASHX(FLASH_REGION_BASE)

#define RESERVED_SECTION_SIZE 0x1000

/* Linker script to configure memory regions. */
/* See plat_addr_map.h and common.mk for the macro definitions */
MEMORY
{
	ROM			(rx)	: ORIGIN = ROM_BASE,				LENGTH = ROM_SIZE
	FLASH 		(r)		: ORIGIN = FLASH_REGION_BASE,		LENGTH = FLASH_REGION_SIZE
	FLASH_NC	(r)		: ORIGIN = FLASH_NC_REGION_BASE,	LENGTH = FLASH_REGION_SIZE
	FLASHX		(rx)	: ORIGIN = FLASHX_REGION_BASE,		LENGTH = FLASH_REGION_SIZE
	RAM			(rwx)	: ORIGIN = RAM_BASE,				LENGTH = RAM_SIZE - FAST_XRAM_SECTION_SIZE
	RAMX		(rx)	: ORIGIN = RAMX_BASE,				LENGTH = RAM_SIZE - FAST_XRAM_SECTION_SIZE
	FRAMX		(rwx)	: ORIGIN = RAMX_BASE + RAM_SIZE - FAST_XRAM_SECTION_SIZE,	LENGTH = FAST_XRAM_SECTION_SIZE
#ifdef PSRAM_BASE
	PSRAM 		(rwx)	: ORIGIN = PSRAM_BASE,				LENGTH = PSRAM_SIZE
	PSRAM_NC	(rwx)	: ORIGIN = PSRAM_NC_BASE,			LENGTH = PSRAM_SIZE
	PSRAMX		(rx)	: ORIGIN = PSRAMX_BASE,				LENGTH = PSRAM_SIZE
#endif
}

/* Library configurations */
GROUP(libgcc.a libc.a libm.a libnosys.a)

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __export_fn_rom
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 *   __free_ram
 *   __factory_start
 *   __factory_end
 *   __calib_start
 *   __calib_end
 *   __flash_start
 *   __flash_end
 *   __free_flash
 *   __boot_sram_start_flash__
 *   __boot_sram_end_flash__
 *   __boot_sram_start__
 *   __boot_bss_sram_start__
 *   __boot_bss_sram_end__
 *   __sram_text_data_start_flash__
 *   __sram_text_data_end_flash__
 *   __fast_sram_text_data_start__
 *   __fast_sram_text_data_end__
 *   __fast_sram_text_data_start_flash__
 *   __fast_sram_text_data_end_flash__
 *   __sram_text_data_start__
 *   __sram_bss_start__
 *   __sram_bss_end__
 *   __overlay_text_start__
 *   __overlay_text_exec_start__
 *   __overlay_data_start__
 */
ENTRY(Boot_Loader)

SECTIONS
{
	__export_fn_rom = (ORIGIN(ROM) + LENGTH(ROM) - ROM_BUILD_INFO_SECTION_SIZE - ROM_EXPORT_FN_SECTION_SIZE);

#if defined(__SBC_FUNC_IN_ROM_VBEST2000__)
    rom1_entry = 0x0000c000;
    set__impure_ptr = 0x0003a4e4;
#endif

#if defined(__SBC_FUNC_IN_ROM_VBEST2000__) || defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
    SBC_FrameLen = 0x000329b1;
    SBC_InitDecoder = 0x00032a65;
    SBC_DecodeFrames = 0x000348c9;
    SBC_DecodeFrames_Out_SBSamples = 0x00034dc5;
    SBC_EncodeFrames = 0x0003538d;
    SBC_InitEncoder = 0x00035381;
    SBC_EncodeFrames_With_SBSamples = 0x000377bd;
#endif

	.boot_struct (ORIGIN(FLASH)) :
	{
		__flash_start = .;
		KEEP(*(.boot_struct))
		. = ALIGN(4);
	} > FLASH

	. = FLASH_TO_FLASHX(.);

	.boot_text_flash (.) : AT (FLASHX_TO_FLASH(ADDR(.boot_text_flash)))
	{
		*(.boot_loader)
		*(.boot_text_flash*)
		*(.boot_rodata*)
		. = ALIGN(4);
	} > FLASHX
	
		.firmware_rev (ALIGN(64)) :
	{
		*(.fw_rev)
		. = ALIGN(4);
		
	} > FLASHX
	
	.ota_upgrade_log (ALIGN(4096)):
	{
		*(.ota_upgrade_log)
		. = (0x1000);
	} > FLASHX
	
	.vector_table (ORIGIN(RAM)) (NOLOAD) :
	{
		KEEP(*(.vector_table))
		. = VECTOR_SECTION_SIZE;
		. = ALIGN(4);
	} > RAM

	.reboot_param (.) (NOLOAD) :
	{
		KEEP(*(.reboot_param))
		. = REBOOT_PARAM_SECTION_SIZE;
		. = ALIGN(4);
	} > RAM

	.userdata_pool (.) (NOLOAD) :
	{
		*(.userdata_pool)
		. = ALIGN(4);
		__userdata_pool_end__ = .;
	} > RAM

	__boot_sram_start_flash__ = LOADADDR(.boot_text_flash) + SIZEOF(.boot_text_flash)+ 0x1000+ SIZEOF(.ota_upgrade_log);
	__boot_sram_start__ = __userdata_pool_end__;
	.boot_text_sram (RAM_TO_RAMX(__boot_sram_start__)) : AT (__boot_sram_start_flash__)
	{
		/* memcpy.o or libc_nano.a(lib_a-memcpy.o/lib_a-memcpy-stub.o) */
		*:memcpy.o(.text*)
		*:lib_a-memcpy*.o(.text*)

		/* memset.o or libc_nano.a(lib_a-memset.o) */
		*:memset.o(.text*)
		*:lib_a-memset*.o(.text*)

		*:hal_norflash*.o(.text*)
		*:norflash_*.o(.text*)

		*(.boot_text_sram*)
		. = ALIGN(4);
	} > RAMX

	.boot_data_sram (RAMX_TO_RAM(ADDR(.boot_text_sram) + SIZEOF(.boot_text_sram))) :
					AT (__boot_sram_start_flash__ + SIZEOF(.boot_text_sram))
	{
		*:hal_norflash*.o(.data* .rodata*)
		*:norflash_*.o(.data* .rodata*)
		*:hal_psram.o(.data* .rodata*)
		*(.boot_data*)
		. = ALIGN(4);
	} > RAM

	__boot_sram_end_flash__ = __boot_sram_start_flash__ +
								SIZEOF(.boot_text_sram) + SIZEOF(.boot_data_sram);

	.boot_bss_sram (.) (NOLOAD) :
	{
		__boot_bss_sram_start__ = .;
		*:hal_norflash*.o(.bss*)
		*:norflash_*.o(.bss*)
		*:hal_psram.o(.bss*)
		*(.boot_bss*)
		. = ALIGN(4);
		__boot_bss_sram_end__ = .;
	} > RAM

	.mempool_bss_sram (.) (NOLOAD) :
	{
		__mempool_bss_sram_start__ = .;
		*:app_audio.o(.bss.app_audio_buff)
		*:app_tws_if.o(.bss.tws_big_buff)
		*(.mempool_bss*)
		. = ALIGN(4);
		__mempool_bss_sram_end__ = .;
	} > RAM

	__sram_text_data_start_flash__ = __boot_sram_end_flash__;
	__sram_text_data_start__ = __mempool_bss_sram_end__;
	.sram_text (RAM_TO_RAMX(__sram_text_data_start__)) :
				AT (__sram_text_data_start_flash__)
	{
		*(.sram_text*)
		. = ALIGN(4);
	} > RAMX

	.sram_data (RAMX_TO_RAM(ADDR(.sram_text) + SIZEOF(.sram_text))) :
				AT (__sram_text_data_start_flash__ + SIZEOF(.sram_text))
	{
#if defined(BTUSB_AUDIO_MODE)
		*:hal_usb.o(.rodata*)
#endif
		*:utils.o(.data* .rodata*)
		*:cqueue.o(.data* .rodata*)
		*:iir_process.o(.data* .rodata*)
#if defined(HFP_1_6_ENABLE)
		*:sbcplc.o(.data* .rodata*)
		*:sbc.o(.data* .rodata*)
		*:sbc_math.o(.data* .rodata*)
#endif
#if defined(A2DP_AAC_ON)
		*:fixpoint_math.o(.data* .rodata*)
		*:FDK_bitbuffer.o(.data* .rodata*)
#endif
		*:HAL_CM4.o(.data* .rodata*)
		*:rt_CMSIS.o(.data* .rodata*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
		*:rt_Event.o(.data* .rodata*)
		*:rt_List.o(.data* .rodata*)
		*:rt_System.o(.data* .rodata*)
		*:rt_Time.o(.data* .rodata*)
		*:HAL_CM.o(.data* .rodata*)
		*:RTX_Conf_CM.o(.data* .rodata*)
		*:rt_Event.o(.data* .rodata*)
		*:rt_List.o(.data* .rodata*)
		*:rt_Mailbox.o(.data* .rodata*)
		*:rt_MemBox.o(.data* .rodata*)
		*:rt_Mutex.o(.data* .rodata*)
		*:rt_Robin.o(.data* .rodata*)
		*:rt_Semaphore.o(.data* .rodata*)
		*:rt_System.o(.data* .rodata*)
		*:rt_Task.o(.data* .rodata*)
		*:rt_Time.o(.data* .rodata*)
		*:a2dp_sm.o(.data* .rodata*)
		*:avsigmgr.o(.data* .rodata*)
		*:avdtpcon.o(.data* .rodata*)
		*:l2cap_if.o(.data* .rodata*)
		*:me.o(.data* .rodata*)
		*:eventmgr.o(.data* .rodata*)
#endif
		*:hci_proc.o(.data* .rodata*)
		*:rxbuff.o(.data* .rodata*)
		*:a2dp.o(.data* .rodata*)
		*:app_tws_role_switch.o(.data* .rodata*)
		*:libthird_party_CMSIS_twiddle_tables_cm4_la.o(.data* .rodata*)
		*(.sram_data*)
		. = ALIGN(4);
	} > RAM

	__sram_text_data_end_flash__ = __sram_text_data_start_flash__ +
								SIZEOF(.sram_text) + SIZEOF(.sram_data);

	.sram_bss (.) (NOLOAD) :
	{
		__sram_bss_start__ = .;
		*:utils.o(.bss*)
		*(.sram_bss*)
		. = ALIGN(4);
		__sram_bss_end__ = .;
	} > RAM

	__fast_sram_text_data_start_flash__ = __sram_text_data_end_flash__;
	.fast_text_sram (ORIGIN(FRAMX)) : AT (__fast_sram_text_data_start_flash__)
	{
		__fast_sram_text_data_start__ = .;

		*:HAL_CM4.o(.text*)
		*:rt_CMSIS.o(.text*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
		*:rt_Event.o(.text*)
		*:rt_List.o(.text*)
		*:rt_System.o(.text*)
		*:rt_Time.o(.text*)
		*:HAL_CM.o(.text*)
		*:RTX_Conf_CM.o(.text*)
		*:rt_Event.o(.text*)
		*:rt_List.o(.text*)
		*:rt_Mailbox.o(.text*)
		*:rt_MemBox.o(.text*)
		*:rt_Mutex.o(.text*)
		*:rt_Robin.o(.text*)
		*:rt_Semaphore.o(.text*)
		*:rt_System.o(.text*)
		*:rt_Task.o(.text*)
		*:rt_Time.o(.text*)
		*:a2dp_sm.o(.text.A2dpAvdtpCallback)
		*:avsigmgr.o(.text.AvdtpStrmConnCallback)
		*:avdtpcon.o(.text.AvdtpConnConnected)
		*:me.o(.text.ME_Callback)
		*:eventmgr.o(.text.EVM_Process)
#endif
		*:hci_proc.o(.text*)
		*:rxbuff.o(.text*RXBUFF_Alloc)
		*:rxbuff.o(.text*RXBUFF_Free)
		*:a2dp.o(.text.*StreaTransferAacPacket)
		*:app_tws_role_switch.o(.text.*hci_rx_data_parser*)

#if defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
        *:main.o(.text.*p_OS_MemSet*);
        *:main.o(.text.*p_OS_Assert*);
        *:main.o(.text.*p_hal_trace_printf_without_crlf*);
        *:main.o(.text.*p_memcpy*);
#endif

#if defined(BTUSB_AUDIO_MODE)
		*:hal_dma.o(.text*)
#endif

#if defined(BTUSB_AUDIO_MODE)
		*:hal_usb.o(.text*)

		*:usb_audio.o(.text*)
		*:usb_audio_sync.o(.text*)
		*:usb_audio_cdc.o(.text*)

		*:usbaudio_entry.o(.text*)
		*:usb_audio_app.o(.text*)
#endif

		/* memmove.o or libc_nano.a(lib_a-memmove.o) */
		*:memmove.o(.text*)
		*:lib_a-memmove.o(.text*)
		*:utils.o(.text*)
		*:cqueue.o(.text*)
#if (MIX_MIC_DURING_MUSIC)
                *:app_bt_stream.o(.text*)
#endif
#if defined(HFP_1_6_ENABLE)
#if defined(__SBC_FUNC_IN_ROM__) || defined(__SBC_FUNC_IN_ROM_VBEST2000__)
		*:sbc.o(.text*)
#else
		*:sbc.o(.text.SbcIsValidStreamInfo)
		*:sbc.o(.text.SBC_FrameLen)
		*:sbc.o(.text.SbcCrcSum4)
		*:sbc.o(.text.SbcMonoBitAlloc)
		*:sbc.o(.text.SbcParseHeader)
		*:sbc.o(.text.SbcParseScaleFactors)
		*:sbc.o(.text.SbcUnpackSample)
		*:sbc.o(.text.SbcBuildSubbandSamples)
		*:sbc.o(.text.SbcJointProcessing)
		*:sbc.o(.text.SbcSynthesisFilter8)
		*:sbc.o(.text.SbcResetDecoderState)
		*:sbc.o(.text.SbcMuteFrame)
		*:sbc.o(.text.SBC_DecodeFrames)
		*:sbc.o(.text.SbcPrepareHeader)
		*:sbc.o(.text.SbcQuantizeSamples)
#endif
		*:sbc_math.o(.text*)
#endif
#if defined(A2DP_AAC_ON)
		*:fixpoint_math.o(.text*)
		*:FDK_bitbuffer.o(.text*)
		*:FDK_tools_rom.o(.text*FDKgetWindowSlope*)
		*:FDK_tools_rom.o(.text*getBitstreamElementList*)
		*:dct.o(.text*dct_IV*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
		*:fft.o(.text._Z3fftiPlPi)
		*:fft_rad2.o(.text*dit_fft*)
		*:dct.o(.text*getTables*)
#endif
#endif
		*:libthird_party_CMSIS_twiddle_tables_cm4_la.o(.text*)

		*(.fast_text_sram*)
		. = ALIGN(4);
		__fast_sram_text_data_end__ = .;
	} > FRAMX
	__fast_sram_text_data_end_flash__ = __fast_sram_text_data_start_flash__ + SIZEOF(.fast_text_sram);

	__overlay_text_start__ = RAMX_TO_RAM(__fast_sram_text_data_end__);
	__overlay_text_exec_start__ = RAM_TO_RAMX(__overlay_text_start__);
	OVERLAY (__overlay_text_exec_start__) : NOCROSSREFS AT (__fast_sram_text_data_end_flash__)
	{
#ifndef NO_OVERLAY
		.overlay_text0
		{
			LONG(0);
			/* common */
			*:ae_common.o(.text*)
			*:ae_rand.o(.text*)
			*:cmsis_fftwrap.o(.text*)
			*:fastmath.o(.text*)
			*:integer_delay.o(.text*)
			*:iirfilt.o(.text*)
			*:ringbuf.o(.text*)
			*:scale_int16.o(.text*)
			*:speech_conv.o(.text*)
			*:speech_trans_buf.o(.text*)
			*:speech_win.o(.text*)

			*:preprocess.o(.text*)
			*:filterbank.o(.text*)
			*:kiss_fft.o(.text*)
			*:kiss_fftr.o(.text*)
			*:fftwrap.o(.text*)
			*:mdf.o(.text*)
			*:plc_8000.o(.text*)
			*:iir_process.o(.text.iir_run_mono_15bits .text.iir_run_mono_16bits .text.iir_run_mono_algorithmgain)
#if defined(HFP_1_6_ENABLE)
			*:plc_16000.o(.text*)
#endif
#if defined(_CVSD_BYPASS_)
			*:Pcm8k_Cvsd.o(.text*)
#endif
#if defined(SPEECH_TX_2MIC_NS)
			*:webrtc_vad.o(.text.WebRtcVad_Process)
			*:webrtc_vad.o(.text.WebRtcVad_ValidRateAndFrameLength)
			*:webrtc_vad.o(.text.WebRtcVad_CalcVad16khz)
			*:webrtc_vad.o(.text.WebRtcVad_Downsampling)
			*:webrtc_vad.o(.text.WebRtcVad_CalcVad8khz)
			*:vad_filterbank.o(.text.*)
			*:energy.o(.text.*)
			*:get_scaling_square.o(.text.*)
			*:dual_mic_denoise.o(.text*)
			*:cross_correlation.o(.text*)
			*:min_max_operations.o(.text*)
			*:resample_by_2_internal.o(.text*)
			*:division_operations.o(.text*)
			*:downsample_fast.o(.text*)
			*:resample_fractional.o(.text*)
			*:resample_48khz.o(.text*)
			*:vad_sp.o(.text*)
			*:vad_core.o(.text*)
			*:webrtc_vad.o(.text*)
			*:vad_gmm.o(.text*)
#endif
#if defined(SPEECH_TX_2MIC_NS2)
			*:coherent_denoise.o(.text*)
#endif
#if defined(SPEECH_TX_NS2) || defined(SPEECH_RX_NS2)
			*:lc_mmse_ns.o(.text*)
#endif
#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT)
			*:lc_mmse_ns_float.o(.text*)
#endif
#if defined(SPEECH_TX_NS3) || defined(SPEECH_RX_NS3)
			*:ns3.o(.text*)
			*:noise_suppression_x.o(.text*)
			*:nsx_core.o(.text*)
			*:nsx_core_c.o(.text*)
			*:copy_set_operations.o(.text*)
			*:cross_correlation.o(.text*)
			*:division_operations.o(.text*)
			*:dot_product_with_scale.o(.text*)
			*:downsample_fast.o(.text*)
			*:energy.o(.text*)
			*:get_scaling_square.o(.text*)
			*:min_max_operations.o(.text*)
			*:real_fft.o(.text*)
			*:resample_by_2.o(.text*)
			*:resample_by_2_internal.o(.text*)
			*:spl_init.o(.text*)
			*:spl_sqrt.o(.text*)
			*:spl_sqrt_floor.o(.text*)
			*:vector_scaling_operations.o(.text*)
#endif
#if defined(SPEECH_TX_AEC2FLOAT)
			*:coherence.o(.text*)
			*:echo_canceller.o(.text*)
			*:echo_suppression.o(.text*)
			*:lcmmse.o(.text*)
			*:logmmse.o(.text*)
			*:mdf2.o(.text*)
			*:noise_generator.o(.text*)
			*:pink_noise_gen.o(.text*)
			*:white_noise_gen.o(.text*)
#endif
#if defined(MSBC_8K_SAMPLE_RATE)
			*:iir_resample.o(.text*)
#endif
#if defined(SPEECH_TX_COMPEXP)
			*:compexp.o(.text*)
#endif
#if defined(SPEECH_TX_EQ) || defined(SPEECH_RX_EQ) || defined(MSBC_8K_SAMPLE_RATE)
			*:speech_eq.o(.text*)
			*:speech_arm_eq.o(.text*)
			*:arm_biquad_cascade_df1_f32.o(.text*)
#endif
#if defined(SPEECH_TX_POST_GAIN) || defined(SPEECH_RX_POST_GAIN)
			*:speech_gain.o(.text*)
#endif
#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT) || defined(SPEECH_TX_AEC2FLOAT)
			*:arm_rfft_fast_f32.o(.text*)
			*:arm_cfft_f32.o(.text*)
#endif
			*(.overlay_text0)
			. = ALIGN(4);
		}
		.overlay_text1
		{
			LONG(0);
#if defined(SBC_TRANS_TO_AAC)
			*:adj_thr.o(.text.*FDKaacEnc_AdjThrClose*)
			*:bit_cnt.o(.text*)
			*:quantize.o(.text*FDKaacEnc_quantizeLines*)
			*:adj_thr.o(.text*CalcRedValPower*)
			*:tpenc_lib.o(.text.*transportEnc_GetStaticBits*)
			*:tpenc_latm.o(.text.*transportEnc_LatmCountTotalBitDemandHeader*)
			*:tpenc_lib.o(.text.*transportEnc_GetBitstream*)
			*:line_pe.o(.text.*FDKaacEnc_calcSfbPe*)
			*:adj_thr.o(.text.*FDKaacEnc_calcPe*)
			*:spreading.o(.text.*FDKaacEnc_SpreadingMax*)
			*:bitenc.o(.text.*FDKaacEnc_writeExtensionData*)
			*:bitenc.o(.text.*FDKaacEnc_ChannelElementWrite*)
			*:qc_main.o(.text.*FDKaacEnc_getTotalConsumedBits*)
			*:adj_thr.o(.text.*FDKaacEnc_reduceThresholdsCBR*)
			*:band_nrg.o(.text.*FDKaacEnc_CalcSfbMaxScaleSpec*)
			*:aacenc_tns.o(.text.*FDKaacEnc_AutoToParcor*)
			*:aacenc_tns.o(.text.*FDKaacEnc_Parcor2Index*)
			*:aacenc_tns.o(.text.*FDKaacEnc_TnsDetect*)
			*:aacenc_tns.o(.text.*FDKaacEnc_TnsEncode*)
			*:pre_echo_control.o(.text.*FDKaacEnc_PreEchoControl*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
			*:transform.o(.text.*FDKaacEnc_Transform_Real*)
			*:band_nrg.o(.text.*FDKaacEnc_CheckBandEnergyOptim*)
			*:tpenc_latm.o(.text.*AdvanceAudioMuxElement*)
			*:aacenc.o(.text.*FDKaacEnc_EncodeFrame*)
			*:aacenc_pns.o(.text.*FDKaacEnc_PnsDetect*)
			*:aacenc_tns.o(.text.*FDKaacEnc_MergedAutoCorrelation*)
			*:qc_main.o(.text.*FDKaacEnc_QCMain*)
			*:dyn_bits.o(.text.*FDKaacEnc_dynBitCount*)
			*:tpenc_lib.o(.text.*transportEnc_GetFrame*)
			*:line_pe.o(.text.*FDKaacEnc_prepareSfbPe*)
			*:qc_main.o(.text.*FDKaacEnc_updateBitres*)
			*:tpenc_lib.o(.text.*transportEnc_CrcEndReg*)
			*:tpenc_latm.o(.text.*transportEnc_LatmWrite*)
			*:qc_main.o(.text.*FDKaacEnc_AdjustBitrate*)
			*:qc_main.o(.text.*FDKaacEnc_QCMainPrepare*)
			*:adj_thr.o(.text.*FDKaacEnc_peCalculation*)
			*:block_switch.o(.text.*FDKaacEnc_BlockSwitching*)
			*:sf_estim.o(.text.*FDKaacEnc_CalcFormFactor*)
			*:aacenc_pns.o(.text.*FDKaacEnc_CodePnsChannel*)
			*:adj_thr.o(.text.*FDKaacEnc_DistributeBits*)
			*:bitenc.o(.text.*FDKaacEnc_WriteBitstream*)
			*:qc_main.o(.text.*FDKaacEnc_updateFillBits*)
			*:tpenc_lib.o(.text.*transportEnc_CrcStartReg*)
			*:tpenc_latm.o(.text.*transportEnc_LatmGetFrame*)
			*:adj_thr.o(.text.*FDKaacEnc_AdjustThresholds*)
			*:quantize.o(.text.*FDKaacEnc_QuantizeSpectrum*)
			*:tpenc_lib.o(.text.*transportEnc_EndAccessUnit*)
			*:block_switch.o(.text.*FDKaacEnc_SyncBlockSwitching*)
			*:tpenc_lib.o(.text.*transportEnc_WriteAccessUnit*)
			*:sf_estim.o(.text.*FDKaacEnc_EstimateScaleFactors*)
			*:qc_main.o(.text.*FDKaacEnc_FinalizeBitConsumption*)
			*:tpenc_latm.o(.text.*transportEnc_LatmAdjustSubframeBits*)
			*:sf_estim.o(.text.*FDKaacEnc_FDKaacEnc_EstimateScaleFactorsChannel*)
			*:adj_thr.o(.text.*FDKaacEnc_adaptMinSnr*)
			*:adj_thr.o(.text.*FDKaacEnc_adaptThresholdsToPe*)
			*:tonality.o(.text.*FDKaacEnc_CalculateFullTonality*)
			*:bit_cnt.o(.text.*FDKaacEnc_count1_2_3_4_5_6_7_8_9_10_11*)
			*:tpenc_latm.o(.text.*CreateStreamMuxConfig*)
			*:tpenc_asc.o(.text.*transportEnc_writeASC*)
			*:tpenc_asc.o(.text.*writeSampleRate*)
			*:tpenc_asc.o(.text.*writeAot*)
			*:FDK_trigFcts.o(.text.*fixp_atan*)
			*:band_nrg.o(.text.*FDKaacEnc_CalcBandEnergyOptimLong*)
			*:fixpoint_math.o(.text.*fPow*)
			*:psy_configuration.o(.text.*getMaskFactor*)
			*:psy_configuration.o(.text.*FDKaacEnc_BarcLineValue*)
			*:genericStds.o(.text.*FDKabs*)
			*:aacenc_pns.o(.text.*FDKaacEnc_PostProcessPnsChannelPair*)
			*:adj_thr.o(.text.*FDKaacEnc_reduceMinSnr*)
			*:grp_data.o(.text.*FDKaacEnc_groupShortData*)
			*:band_nrg.o(.text.*FDKaacEnc_CalcBandEnergyOptimShort*)
			*:aacEnc_ram.o(.text.*FreeRam_aacEnc_QCelement*)
#endif
#else
#if !defined(TWS_RBCODEC_PLAYER) && !defined(TWS_LINEIN_PLAYER)
#if !defined(HFP_1_6_ENABLE)
			*:sbc.o(.text*)
			*:sbc_math.o(.text*)
#endif
			*:fixedpoint.o(.text*)
			*:replaygain.o(.text*)
			*:dsp_core.o(.text*)
			*:dsp_misc.o(.text*)
			*:dsp_filter.o(.text*)
			*:dsp_sample_input.o(.text*)
			*:dsp_sample_output.o(.text*)
			*:eq.o(.text*)
			*:pga.o(.text*)
#if !defined(A2DP_AAC_ON) && !defined(A2DP_SCALABLE_ON)
			*:fir_process.o(.text*)
			*:iir_process.o(.text.iir_run .text.*iir_run_per_*)
#endif
#endif
#endif
			*(.overlay_text1)
			. = ALIGN(4);
		}
		.overlay_text2
		{
			LONG(0);
			*:fmdec.o(.text* .data* .rodata* .bss*)
			*(.overlay_text2)
			. = ALIGN(4);
		}
		.overlay_text3
		{
			LONG(0);
			*:fft128dot.o(.text*)
			*(.overlay_text3)
			. = ALIGN(4);
		}
		.overlay_text4
		{
			LONG(0);
#if defined(A2DP_AAC_ON)
			*:block.o(.text.*CBlock_GetEscape*)
			*:tpdec_lib.o(.text.*transportDec_GetBitstream*)
			*:tpdec_lib.o(.text.*transportDec_GetAuBitsRemaining*)
			*:tpdec_latm.o(.text.*CLatmDemux_GetValue*)
			*:tpdec_latm.o(.text.*CLatmDemux_Read*)
			*:tpdec_lib.o(.text.*transportDec_FillData*)
			*:tpdec_lib.o(.text.*transportDec_ReadAccessUnit*)
			*:tpdec_lib.o(.text.*transportDec_readHeader*)
			*:block.o(.text.*ApplyTools*)
			*:aacdec_pns.o(.text.*CPns_Apply*)
			*:aacdec_tns.o(.text.*CTns_Apply*)
			*:aacdec_tns.o(.text.*CTns_Reset*)
			*:mdct.o(.text.*imdct_gain*)
			*:mdct.o(.text.*imdct_block*)
			*:aacdec_pns.o(.text.*CPns_ResetData*)
			*:pulsedata.o(.text.*CPulseData_Read*)
			*:channel.o(.text.*CChannelElement_Read*)
			*:tpdec_asc.o(.text.*CProgramConfig_Reset*)
			*:tpdec_lib.o(.text.*transportDec_CrcCheck*)
			*:block.o(.text.*CBlock_FrequencyToTime*)
			*:block.o(.text.*CBlock_ReadSectionData*)
			*:channel.o(.text.*CChannelElement_Decode*)
			*:tpdec_lib.o(.text.*transportDec_CrcEndReg*)
			*:aacdecoder.o(.text.*CAacDecoder_DecodeFrame*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
			*:block.o(.text.*CBlock_ReadSpectralData*)
			*:block.o(.text.*CBlock_ScaleSpectralData*)
			*:aacdec_tns.o(.text.*CTns_ReadDataPresentFlag*)
			*:tpdec_lib.o(.text.*transportDec_CrcStartReg*)
			*:block.o(.text.*CBlock_ReadScaleFactorData*)
			*:tpdec_lib.o(.text.*transportDec_EndAccessUnit*)
			*:tpdec_latm.o(.text.*CLatmDemux_GetNrOfSubFrames*)
			*:tpdec_lib.o(.text.*transportDec_GetAuBitsTotal*)
			*:tpdec_latm.o(.text.*CLatmDemux_GetFrameLengthInBits*)
			*:tpdec_latm.o(.text.*CLatmDemux_ReadPayloadLengthInfo*)
			*:block.o(.text.*CBlock_InverseQuantizeSpectralData*)
			*:channelinfo.o(.text.*IcsRead*)
			*:tpdec_asc.o(.text.*CProgramConfig_LookupElement*)
			*:aacdec_tns.o(.text.*CTns_Read*)
			*:tpdec_lib.o(.text.*transportDec_AdjustEndOfAccessUnit*)
			*:pulsedata.o(.text.*CPulseData_Apply*)
			*:tpdec_asc.o(.text.*AudioSpecificConfig_Init*)
			*:tpdec_asc.o(.text.*AudioSpecificConfig_Parse*)
			*:tpdec_latm.o(.text.*CLatmDemux_ReadStreamMuxConfig*)
			*:tpdec_asc.o(.text.*getSampleRate*)
			*:tpdec_asc.o(.text.*getAOT*)
			*:aacdec_drc.o(.text*)
			*:aacdec_hcr_bit.o(.text*)
			*:aacdec_hcr.o(.text*)
			*:aacdec_hcrs.o(.text*)
			*:aacdecoder_lib.o(.text*)
			*:aacdecoder.o(.text*)
			*:aacdec_pns.o(.text*)
			*:aacdec_tns.o(.text*)
			*:aac_ram.o(.text*)
			*:aac_rom.o(.text*)
			*:block.o(.text*)
			*:channelinfo.o(.text*)
			*:channel.o(.text*)
			*:conceal.o(.text*)
			*:ldfiltbank.o(.text*)
			*:pulsedata.o(.text*)
			*:rvlcbit.o(.text*)
			*:rvlcconceal.o(.text*)
			*:rvlc.o(.text*)
			*:stereo.o(.text*)
#endif
#endif
			*(.overlay_text4)
			. = ALIGN(4);
		}
		.overlay_text5
		{
			LONG(0);
			*:mpa.o(.text*)
			*:bit.o(.text*)
			*:frame.o(.text*)
			*:huffman.o(.text*)
			*:layer12.o(.text*)
			*:layer3.o(.text*)
			*:stream.o(.text*)
			*:synth.o(.text*)
			*(.overlay_text5)
			. = ALIGN(4);
		}
		.overlay_text6
		{
			LONG(0);
#if defined(A2DP_LHDC_ON)
			*:lhdcUtil.o(.text*)
			*:dec.o(.text*)
			*:cirbuf.o(.text*)
#endif
			*(.overlay_text6)
			. = ALIGN(4);
		}
		.overlay_text7
		{
			LONG(0);
			*(.overlay_text7)
			. = ALIGN(4);
		}
#endif
		.overlay_text_end
		{
			LONG(0);
			. = ALIGN(4);
		}
	} > FRAMX

	.dummy_overlay_text (.) (NOLOAD) :
	{
		__dummy_overlay_text_end__ = .;
	} > FRAMX

	ASSERT(__dummy_overlay_text_end__ - __fast_sram_text_data_start__ <= FAST_XRAM_SECTION_SIZE, "fast xram sections too large")
	__free_fram = FAST_XRAM_SECTION_SIZE - (__dummy_overlay_text_end__ - __fast_sram_text_data_start__);

	__overlay_data_start__ = __sram_bss_end__;
	OVERLAY (__overlay_data_start__) : NOCROSSREFS AT (__load_stop_overlay_text_end)
	{
#ifndef NO_OVERLAY
		.overlay_data0
		{
			/* Explicitly place 4 bytes at section start to avoid "section type changed to PROGBITS" warnings */
			LONG(0);
			/* common */
			*:fastmath_const.o(.data* .rodata*)
			*:speech_win.o(.data* .rodata*)

			*:preprocess.o(.data* .rodata*)
			*:filterbank.o(.data* .rodata*)
			*:kiss_fft.o(.data* .rodata*)
			*:kiss_fftr.o(.data* .rodata*)
			*:fftwrap.o(.data* .rodata*)
			*:mdf.o(.data* .rodata*)
			*:plc_8000.o(.data* .rodata*)
#if defined(HFP_1_6_ENABLE)
			*:plc_16000.o(.data* .rodata*)
#endif			
#if defined(_CVSD_BYPASS_)
			*:Pcm8k_Cvsd.o(.data* .rodata* .bss*)
#endif
#if defined(SPEECH_TX_2MIC_NS)
			*:dual_mic_denoise.o(.data* .rodata*)
			*:vad_filterbank.o(.data* .rodata*)
			*:energy.o(.data* .rodata*)
			*:get_scaling_square.o(.data* .rodata*)

			*:cross_correlation.o(.data* .rodata*)
			*:min_max_operations.o(.data* .rodata*)
			*:resample_by_2_internal.o(.data* .rodata*)
			*:division_operations.o(.data* .rodata*)
			*:downsample_fast.o(.data* .rodata*)
			*:resample_fractional.o(.data* .rodata*)
			*:resample_48khz.o(.data* .rodata*)
			*:vad_sp.o(.data* .rodata*)
			*:vad_core.o(.data* .rodata*)
			*:webrtc_vad.o(.data* .rodata*)
			*:vad_gmm.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_2MIC_NS2)
			*:coherent_denoise.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_NS2) || defined(SPEECH_RX_NS2)
			*:lc_mmse_ns.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_NS3) || defined(SPEECH_RX_NS3)
			*:nsx_core.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT)
			*:lc_mmse_ns_float.o(.data* .rodata*)
#endif
#if defined(MSBC_8K_SAMPLE_RATE)
			*:iir_resample.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT) || defined(SPEECH_TX_AEC2FLOAT)
			*:arm_common_tables.o(.rodata.armBitRevIndexTable256)
			*:arm_common_tables.o(.rodata.twiddleCoef_256)
			*:arm_common_tables.o(.rodata.twiddleCoef_rfft_512)
			*:arm_common_tables.o(.rodata.armBitRevIndexTable128)
			*:arm_common_tables.o(.rodata.twiddleCoef_128)
			*:arm_common_tables.o(.rodata.twiddleCoef_rfft_256)
#endif
			*(.overlay_data0 .overlay_rodata0)
			. = ALIGN(4);
		}
		.overlay_data1
		{
			LONG(0);
#if defined(SBC_TRANS_TO_AAC)
			*:aacenc_lib.o(.data* .rodata*)
			*:aacenc.o(.data* .rodata*)
			*:adj_thr.o(.data* .rodata*)
			*:bit_cnt.o(.data* .rodata*)
			*:quantize.o(.data* .rodata*)
			*:tpenc_lib.o(.data* .rodata*)
			*:tpenc_latm.o(.data* .rodata*)
			*:line_pe.o(.data* .rodata*)
			*:spreading.o(.data* .rodata*)
			*:bitenc.o(.data* .rodata*)
			*:qc_main.o(.data* .rodata*)
			*:band_nrg.o(.data* .rodata*)
			*:aacenc_tns.o(.data* .rodata*)
			*:pre_echo_control.o(.data* .rodata*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
			*:transform.o(.data* .rodata*)
			*:dyn_bits.o(.data* .rodata*)
			*:aacenc_pns.o(.data* .rodata*)
			*:aacEnc_ram.o(.data* .rodata*)
			*:aacEnc_rom.o(.data* .rodata*)
			*:bandwidth.o(.data* .rodata*)
			*:block_switch.o(.data* .rodata*)
			*:channel_map.o(.data* .rodata*)
			*:chaosmeasure.o(.data* .rodata*)
			*:grp_data.o(.data* .rodata*)
			*:intensity.o(.data* .rodata*)
			*:metadata_compressor.o(.data* .rodata*)
			*:metadata_main.o(.data* .rodata*)
			*:ms_stereo.o(.data* .rodata*)
			*:noisedet.o(.data* .rodata*)
			*:pnsparam.o(.data* .rodata*)
			*:psy_configuration.o(.data* .rodata*)
			*:psy_main.o(.data* .rodata*)
			*:sf_estim.o(.data* .rodata*)
			*:tonality.o(.data* .rodata*)
#endif
#else
#if !defined(TWS_RBCODEC_PLAYER) && !defined(TWS_LINEIN_PLAYER)
#if !defined(HFP_1_6_ENABLE)
			*:sbc.o(.data* .rodata*)
			*:sbc_math.o(.data* .rodata*)
#endif
			*:fixedpoint.o(.data* .rodata*)
			*:replaygain.o(.data* .rodata*)
			*:dsp_core.o(.data* .rodata*)
			*:dsp_misc.o(.data* .rodata*)
			*:dsp_filter.o(.data* .rodata*)
			*:dsp_sample_input.o(.data* .rodata*)
			*:dsp_sample_output.o(.data* .rodata*)
			*:eq.o(.data* .rodata*)
			*:pga.o(.data* .rodata*)
#if !defined(A2DP_AAC_ON) && !defined(A2DP_SCALABLE_ON)
			*:fir_process.o(.data* .rodata*)
#endif
#endif
#endif
			*(.overlay_data1 .overlay_rodata1)
			. = ALIGN(4);
		}
		.overlay_data2
		{
			LONG(0);
			*(.overlay_data2 .overlay_rodata2)
			. = ALIGN(4);
		}
		.overlay_data3
		{
			LONG(0);
			*:fft128dot.o(.data* .rodata*)
			*(.overlay_data3 .overlay_rodata3)
			. = ALIGN(4);
		}
		.overlay_data4
		{
			LONG(0);
#if defined(A2DP_AAC_ON)
			*:block.o(.data* .rodata*)
			*:tpdec_lib.o(.data* .rodata*)
			*:tpdec_latm.o(.data* .rodata*)
			*:aacdec_pns.o(.data* .rodata*)
			*:aacdec_tns.o(.data* .rodata*)
			*:mdct.o(.data* .rodata*)
			*:pulsedata.o(.data* .rodata*)
			*:channel.o(.data* .rodata*)
			*:tpdec_asc.o(.data* .rodata*)
			*:aacdecoder.o(.data* .rodata*)
#if !defined(__SBC_FUNC_IN_ROM_VBEST2000__)
			*:aacdecoder_lib.o(.data* .rodata*)
			*:channelinfo.o(.data* .rodata*)
			*:stereo.o(.data* .rodata*)
			*:aacdec_drc.o(.data* .rodata*)
			*:aacdec_hcr_bit.o(.data* .rodata*)
			*:aacdec_hcr.o(.data* .rodata*)
			*:aacdec_hcrs.o(.data* .rodata*)
			*:aac_ram.o(.data* .rodata*)
			*:aac_rom.o(.data* .rodata*)
			*:conceal.o(.data* .rodata*)
			*:ldfiltbank.o(.data* .rodata*)
			*:rvlcbit.o(.data* .rodata*)
			*:rvlcconceal.o(.data* .rodata*)
			*:rvlc.o(.data* .rodata*)
#endif
#endif
			*(.overlay_data4 .overlay_rodata4)
			. = ALIGN(4);
		}
		.overlay_data5
		{
			LONG(0);
			*:mpa.o(.data* .bss*)
			*:bit.o(.data* .bss*)
			*:frame.o(.data* .bss*)
			*:huffman.o(.data* .bss*)
			*:layer12.o(.data* .bss*)
			*:layer3.o(.data* .bss*)
			*:stream.o(.data* .bss*)
			*:synth.o(.data* .bss*)
			*(.overlay_data5 .overlay_rodata5)
			. = ALIGN(4);
		}
		.overlay_data6
		{
			LONG(0);
#if defined(A2DP_LHDC_ON)
			*:lhdcUtil.o(.data* .rodata*)
    		        *:dec.o(.data* .rodata*)
			*:cirbuf.o(.data* .rodata*)
#endif
			*(.overlay_data6 .overlay_rodata6)
			. = ALIGN(4);
		}
		.overlay_data7
		{
			LONG(0);
			*(.overlay_data7 .overlay_rodata7)
			. = ALIGN(4);
		}
#endif
		.overlay_data_end
		{
			LONG(0);
			. = ALIGN(4);
		}
	} > RAM

	.dummy_overlay_data (.) (NOLOAD) :
	{
		__dummy_overlay_data_end__ = .;
	} > RAM

	ASSERT(__dummy_overlay_data_end__ - __overlay_data_start__ <= OVERLAY_DATA_SECTION_SIZE, "overlay sections too large")

	.text (FLASH_TO_FLASHX(__load_stop_overlay_data_end)) : AT (FLASHX_TO_FLASH(ADDR(.text)))
	{
		*(.fast_text_sram*)
		*(.text*)
		*(.flash_text*)
		KEEP(*(.init))
		KEEP(*(.fini))

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
		*crtbegin.o(.dtors)
		*crtbegin?.o(.dtors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
		*(SORT(.dtors.*))
		*(.dtors)
	} > FLASHX

	.ARM.extab (.) : AT (FLASHX_TO_FLASH(ADDR(.ARM.extab)))
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASHX

	__exidx_start = .;
	.ARM.exidx (.) : AT (FLASHX_TO_FLASH(ADDR(.ARM.exidx)))
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > FLASHX
	__exidx_end = .;

	. = FLASHX_TO_FLASH(.);

	.rodata (.) :
	{
		*(.rodata*)
		*(.flash_rodata*)
		. = ALIGN(4);
		__cust_cmd_hldr_tbl_start = .;
		KEEP(*(.cust_cmd_hldr_tbl))
		__cust_cmd_hldr_tbl_end = .;
		*(.note.gnu.build-id)
		. = ALIGN(4);
		KEEP(*(.eh_frame*))
	} > FLASH
        .tota.cmd.table (.) :
	{
		. = ALIGN(4);
		__tota_handler_table_start = .;
		KEEP(*(.tota_handler_table))		
		__tota_handler_table_end = .;	
	} > FLASH

	.custom.cmd.table (.) :
	{
		. = ALIGN(4);
		__custom_handler_table_start = .;
		KEEP(*(.custom_handler_table))
		__custom_handler_table_end = .;
	} > FLASH
	
	.tws.cmd.table (.) :
	{
		. = ALIGN(4);
		__tws_custom_handler_table_start = .;
		KEEP(*(.tws_custom_handler_table))
		__tws_custom_handler_table_end = .;
	} > FLASH

	/* To copy multiple FLASH to RAM sections,
	 * uncomment .copy.table section and,
	 * define __STARTUP_COPY_MULTIPLE in startup_ARMCMx.S */
	/*
	.copy.table (.) :
	{
		. = ALIGN(4);
		__copy_table_start__ = .;
		LONG (__etext)
		LONG (__data_start__)
		LONG (__data_end__ - __data_start__)
		LONG (__etext2)
		LONG (__data2_start__)
		LONG (__data2_end__ - __data2_start__)
		__copy_table_end__ = .;
	} > FLASH
	*/

	/* To clear multiple BSS sections,
	 * uncomment .zero.table section and,
	 * define __STARTUP_CLEAR_BSS_MULTIPLE in startup_ARMCMx.S */
	/*
	.zero.table (.) :
	{
		. = ALIGN(4);
		__zero_table_start__ = .;
		LONG (__bss_start__)
		LONG (__bss_end__ - __bss_start__)
		LONG (__bss2_start__)
		LONG (__bss2_end__ - __bss2_start__)
		__zero_table_end__ = .;
	} > FLASH
	*/

	__etext = .;

	.data : AT (__etext)
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);

		. = ALIGN(4);
		/* finit data */
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);

		KEEP(*(.jcr*))
		. = ALIGN(4);
		/* All data end */
		__data_end__ = .;

	} > RAM

	.bss (.) (NOLOAD) :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > RAM

	.heap (.) (NOLOAD) :
	{
		. = ALIGN(8);
		__HeapBase = .;
		__end__ = .;
		end = __end__;
		. += HEAP_SECTION_SIZE;
		. = ALIGN(8);
		__HeapLimit = .;
	} > RAM

	/* .stack_dummy section doesn't contains any symbols. It is only
	 * used for linker to calculate size of stack sections, and assign
	 * values to stack symbols later */
	.stack_dummy (.) (COPY) :
	{
		. = STACK_SECTION_SIZE;
		. = ALIGN(8);
	} > RAM

	/* Set stack top to end of RAM, and stack limit move down by
	 * size of stack_dummy section */
	__StackTop = ORIGIN(RAM) + LENGTH(RAM);
	__StackLimit = __StackTop - SIZEOF(.stack_dummy);
	PROVIDE(__stack = __StackTop);

	/* Check if data + heap + stack exceeds RAM limit */
	ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
	__free_ram = __StackLimit - __HeapLimit;

	. = __etext + SIZEOF(.data);
	.build_info (.) :
	{
		KEEP(*(.build_info))
		. = ALIGN(4);
	} > FLASH = 0x00000000

	/* The following section be the last loaded section */
	.code_start_addr (.) :
	{
		LONG(BUILD_INFO_MAGIC);
		LONG(ABSOLUTE(__flash_start));
	} > FLASH

	__flash_end = .;
	
	.debug_log_section (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - RESERVED_SECTION_SIZE- USERDATA_SECTION_SIZE - AUD_SECTION_SIZE - DEBUG_LOG_SECTION_SIZE - USER_STATISTICS_SECTION_SIZE -CUSTOM_PARAMETER_SECTION_SIZE) (NOLOAD) :
	{
		__debug_log_start = .;
		. = DEBUG_LOG_SECTION_SIZE;
		__debug_log_end = .;
	} > FLASH_NC
	
	.custom_parameter (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - RESERVED_SECTION_SIZE- USERDATA_SECTION_SIZE - AUD_SECTION_SIZE - USER_STATISTICS_SECTION_SIZE - CUSTOM_PARAMETER_SECTION_SIZE) (NOLOAD) :
	{
		__custom_parameter_start = .;
		. = CUSTOM_PARAMETER_SECTION_SIZE;
		__custom_parameter_end = .;
	} > FLASH_NC

	.user_statistics_section (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - RESERVED_SECTION_SIZE- USERDATA_SECTION_SIZE - AUD_SECTION_SIZE - USER_STATISTICS_SECTION_SIZE) (NOLOAD) :
	{
		__user_statistics_start = .;
		. = USER_STATISTICS_SECTION_SIZE;
		__user_statistics_end = .;
	} > FLASH_NC	
	
	.userdata (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - RESERVED_SECTION_SIZE - AUD_SECTION_SIZE - USERDATA_SECTION_SIZE) (NOLOAD) :
	{
		__userdata_start = .;
		. = (.)+USERDATA_PARTITION_SIZE1;
		__userdata_end = .;
		__userdata_start_bak = .;
		. = (.)+USERDATA_PARTITION_SIZE2;
		__userdata_end_bak = .;		
	} > FLASH_NC
	
	.audio (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - RESERVED_SECTION_SIZE - AUD_SECTION_SIZE) (NOLOAD) :
	{
		__aud_start = .;
		. = AUD_SECTION_SIZE;
		__aud_end = .;
	} > FLASH_NC
	
	.factory (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE) (NOLOAD) :
	{
		__factory_start = .;
		. = FACTORY_SECTION_SIZE;
		__factory_end = .;
	} > FLASH_NC

	ASSERT(FLASH_NC_TO_C(__debug_log_start) >= __flash_end, "region FLASH overflowed")
	__free_flash = FLASH_NC_TO_C(__debug_log_start) - __flash_end;
}

