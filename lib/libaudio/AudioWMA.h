/*
 * AudioWMA.h
 * This library is used to load audio from Windows Meadia Audio files(*.wma)
 *
 * WMUSAUDIO Version 0 by mowglisanu
 *
 * Copyright (C) 2010 Musa Mitchell. mowglisanu@gmail.com
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
#ifndef __WMUSAUDIO_H__
#define __WMUSAUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Audio.h"
#include <pspaudiocodec.h>
#include <pspasfparser.h>

typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID dec_thid;
        int eof;
        int working;
        int start;
        int fill;
        int frequency;
        unsigned long* wma_codec_buffer;
        SceAsfParser* parser;
        short *wma_frame_buffer;
        short *wma_output_buffer;     
        int wma_mix_decode;//where to start decoding
        int wma_mix_top;//the cap
        int wma_mix_output;
        SceUID out_thid;
}wmaExtra;

#define WMA_SAMPLES 64
#define WMA_MAX_OUTPUT 12032
#define WMA_MAX_SAMPLES 3008

/**
 * Load a wma file
 *
 * @param filename - Path of the file to load.
 *
 * @returns pointer to Audio struct on success, NULL on failure,
 */
PAudio loadWma(const char *file);
/**
 * Sets the path of the me boot start prx
 *
 * @param filename - Path of the file.
 *
 */
void setBootStartWma(char *location);
/**
 * Retrives the path of the me boot start prx
 *
 * @return - Path of the file.
 *
 */
char *getBootStartWma();

#ifdef __cplusplus
};
#endif

#endif
