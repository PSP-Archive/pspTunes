/*
 * AudioMUSIC.h
 * This library is used to load midi files(*.mid)
 *
 * AUDIOMUSIC Version 0 by mowglisanu
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
#ifndef NO_MIDI
#ifndef __AUDIOMUSIC_H__
#define __AUDIOMUSIC_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "Audio.h"
#include <libtimidity/timidity.h>

typedef struct{
        int size;
        StreamEventHandler stream;
        SceUID thid;
        int eof;
        int working;
        MidSong *song;
}midiExtra;

extern char *config_file;
extern int midi;

/**
 * Sets the config file(./timidity.cfg by default)
 *
 * @param file - Path of the file.
 */
void setMidiConfigFile(char *file);

/**
 * Load a midi file
 *
 * @param filename - Path of the file to load.
 *
 * @returns pointer to Audio struct on success, NULL on failure,
 */
PAudio loadMusic(const char* filename);
     
#ifdef __cplusplus
}
#endif
#endif
#endif
