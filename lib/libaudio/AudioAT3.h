/*
 * AudioAT3.h
 * This library is used to load atrac3 audio files
 *
 * MUSAT3 Version 1 by mowglisanu
 *
 * Copyright (C) Musa Mitchell. mowglisanu@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __MUSAT3_H__
#define __MUSAT3_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <pspkerneltypes.h>
#include <pspaudio.h>
#include <pspaudiocodec.h>
#include <psputility_avmodules.h>
#include <pspatrac3.h>
#include "Audio.h"

/** Identify the CODEC */
typedef enum _PspAtracCodecTypes {
	_PSP_ATRAC_AT3PLUS 	= 0x00001000,
	_PSP_ATRAC_AT3 		= 0x00001001
} _PspAtracCodecTypes;

/** ATRAC3 Error Codes */
enum _PspAtract3ErrorCodes {
	_PSP_ATRAC_SUCCESS						= 0,
	_PSP_ATRAC_ERROR_PARAM_FAIL				= 0x80630001,
	_PSP_ATRAC_ERROR_API_FAIL				= 0x80630002,
	_PSP_ATRAC_ERROR_NO_ATRACID				= 0x80630003,
	_PSP_ATRAC_ERROR_BAD_CODECTYPE			= 0x80630004,
	_PSP_ATRAC_ERROR_BAD_ATRACID			= 0x80630005,
	_PSP_ATRAC_ERROR_UNKNOWN_FORMAT			= 0x80630006,
	_PSP_ATRAC_ERROR_UNMATCH_FORMAT			= 0x80630007,
	_PSP_ATRAC_ERROR_BAD_DATA				= 0x80630008,
	_PSP_ATRAC_ERROR_ALLDATA_IS_ONMEMORY	= 0x80630009,
	_PSP_ATRAC_ERROR_UNSET_DATA				= 0x80630010,
	_PSP_ATRAC_ERROR_READSIZE_IS_TOO_SMALL	= 0x80630011,
	_PSP_ATRAC_ERROR_NEED_SECOND_BUFFER		= 0x80630012,
	_PSP_ATRAC_ERROR_READSIZE_OVER_BUFFER	= 0x80630013,
	_PSP_ATRAC_ERROR_NOT_4BYTE_ALIGNMENT	= 0x80630014,
	_PSP_ATRAC_ERROR_BAD_SAMPLE				= 0x80630015,
	_PSP_ATRAC_ERROR_WRITEBYTE_FIRST_BUFFER	= 0x80630016,
	_PSP_ATRAC_ERROR_WRITEBYTE_SECOND_BUFFER= 0x80630017,
	_PSP_ATRAC_ERROR_ADD_DATA_IS_TOO_BIG	= 0x80630018,
	_PSP_ATRAC_ERROR_UNSET_PARAM			= 0x80630021,
	_PSP_ATRAC_ERROR_NONEED_SECOND_BUFFER	= 0x80630022,
	_PSP_ATRAC_ERROR_NODATA_IN_BUFFER		= 0x80630023,
	_PSP_ATRAC_ERROR_ALLDATA_WAS_DECODED	= 0x80630024
};

/** Remain Frame typical Status */
enum _PspAtract3RemainFrameStatus {
	_PSP_ATRAC_ALLDATA_IS_ON_MEMORY				= -1,
	_PSP_ATRAC_NONLOOP_STREAM_DATA_IS_ON_MEMORY	= -2,
	_PSP_ATRAC_LOOP_STREAM_DATA_IS_ON_MEMORY		= -3
};

typedef struct {
	SceUChar8 *pucWritePositionFirstBuf;
	SceUInt32 uiWritableByteFirstBuf;
	SceUInt32 uiMinWriteByteFirstBuf;
	SceUInt32 uiReadPositionFirstBuf;

	SceUChar8 *pucWritePositionSecondBuf;
	SceUInt32 uiWritableByteSecondBuf;
	SceUInt32 uiMinWriteByteSecondBuf;
	SceUInt32 uiReadPositionSecondBuf;
} _PspBufferInfo;

#define SceAtrac3plusCodec 0x1000
#define SceAtrac3Codec 0x1001

typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID thid;
        int eof;
        int working;
        int ID;
        int fill;
        int frequency;
        void *data;
}at3_extra;
typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID thid;
        int eof;
        int working;
        int start;
        int fill;
        int frequency;
        u8 *data;
        int dataSize;
        unsigned long* aa3_codec_buffer; 
        int position;
        int aa3_data_align;
        int aa3_channel_mode;
}aa3_extra;

/**
 * Load a at3/+ audio from a RIFF    WAVEfmt file
 *
 * @param filename - Path of the file to load.
 *
 * @param stream -  STREAM_FROM_FILE to read from file and STREAM_NO_STREAM to decompress load to memory and STREAM_NO_STREAM to load to memory.
 *
 * @returns pointer to Audio struct on success, NULL on failure,
 */
PAudio loadAt3(const char *file, u32 stream);

/**
 * Load a at3/+ audio from an aa3 file
 *
 * @param filename - Path of the file to load.
 *
 * @returns pointer to Audio struct on success, NULL on failure,
 */
PAudio loadAa3(const char *file);

int At3ToWav(const char *at3file, const char *wavfile);
int Aa3ToWav(const char *aa3file, const char *wavfile);

#ifdef __cplusplus
}
#endif

#endif
