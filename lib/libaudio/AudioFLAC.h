/*
 * AudioFLAC.h
 * This library is used to load audio from flac files
 *
 * MUSAUDIOFLAC Version 1 by mowglisanu
 *
 * Copyright (C) 2009 Musa Mitchell. mowglisanu@gmail.com
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
#ifndef NO_FLAC
#ifndef __MUSAUDIOFLAC_H__
#define __MUSAUDIOFLAC_H__

#include <pspkernel.h> 
#include <pspsdk.h> 
#include <pspaudio.h>
#include <FLAC/all.h>

#include "Audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define fLaC 0x2000

typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID thid;
        int eof;
        int working;
        int start;
        int fill;
        int frequency;
        void *data;
        int dataSize;
        int position;
        int index;
        FLAC__StreamDecoder *decoder;
}flacExtra;

#define FLAC_FLAG_OGG    0x01000000
#define FLAC_SAMPLES 1024
PAudio loadFlac(const char *filename, int stream);
PAudio loadFlacAdvance(int dataSrc, int stream, int size, u32 type, u32 flags);
FLAC__StreamDecoder* getDecoderFlac(const PAudio flac);
#ifdef __cplusplus
}
#endif
#endif
#endif
