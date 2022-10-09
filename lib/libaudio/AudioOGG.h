/*
 * AudioOGG.h
 *
 * MUSAUDIOGG Version 0 by mowglisanu
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
#ifndef NO_OGG
#ifndef __MUSAUDIOGG_H__
#define __MUSAUDIOGG_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_TREMOR
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
//#include <vorbis/ivorbiscodec.h>
//#include <vorbis/ivorbisfile.h>
#else
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#ifdef OGG_FLAC
#include "AudioFLAC.h"
#endif
#endif
#include "Audio.h"

typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID thid;
        int eof;
        int start;
        int fill;
        int working;
        int frequency;
        OggVorbis_File *vf;
        long offset;
}ogg_extra;
#define VorbisCodec 0x3000

#define OGG_SAMPLES 512

/**
 * Load a ogg file
 *
 * @param filename - Path of the file to load.
 *
 * @param stream -  STREAM_FROM_FILE to read from file and STREAM_NO_STREAM to load to memory.
 *
 * @returns pointer to Audio struct on success, NULL on failure,
 */
PAudio loadOgg(const char *filename, int stream);
OggVorbis_File *getVorbisFileOgg(const PAudio ogg);
#ifdef __cplusplus
}
#endif

#endif
#endif
