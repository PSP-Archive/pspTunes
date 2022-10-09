/*
 * AudioMUSIC.c
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
#include "AudioMUSIC.h"

extern void register_audio(PAudio );
extern int registerEndAudio(EndAudio );
char *config_file = NULL;
void setMidiConfigFile(char *file){
     if (config_file)
        free(config_file);
     config_file = malloc(strlen(file)+1);
     if (!config_file)
        return;
     strcpy(config_file, file);
}

int midi = 0;
void midiEnd(){
     if (!midi) return;
     mid_exit();
     midi = 0;
}

void reset_mid(PAudio mid){
     midiExtra *extra = (midiExtra*)mid->extra;
     mid_song_start(extra->song);
     mid->offset = 0;
}

int signal_mid(PAudio mid, eventData *event){
    midiExtra *extra = (midiExtra*)mid->extra;
    switch (event->event){
       case EVENT_stopAudio :
          mid->play = STOP;
          while (extra->working) sceKernelDelayThread(10);
          reset_mid(mid);
          return 0;
       break;
       case EVENT_freeAudio :
          mid->play = TERMINATED;
          sceKernelWaitThreadEnd(mid->extra->thid, 0);
          sceKernelTerminateDeleteThread(mid->extra->thid);
          mid_song_free (extra->song);
          free(mid->extra);
          return 0;
       break;
       case EVENT_setFrequencyAudio :
          //not supported
       break;
       case EVENT_seekAudio :
          while (extra->working) sceKernelDelayThread(10);
          mid_song_seek(extra->song, (event->data*1000)/mid->frequency);
          mid->offset = event->data;
       break;
       default :
          return 0;
    }
    return 1;
}

int mid_thread(SceSize args, void *argp){
    PAudio mid = *(PAudio*)argp;
    midiExtra *extra = (midiExtra*)mid->extra;
    int length = mid->sampleCount*2*mid->channels;
    short *buffer[4] = {mid->data, mid->data+(length), mid->data+(length)*2, mid->data+(length)*3};
    int x = 0;
    int ret = 0;
    size_t bytes_read = 1;
    start:
    do{
          if (mid->play != PLAYING){
             if (mid->play == TERMINATED)
                return 0;
             sceKernelDelayThread(50);
             continue;
          }
          extra->working = 1;
          bytes_read = mid_song_read_wave (extra->song, buffer[x], length);
          extra->working = 0;
          if (bytes_read != length)
             memset(buffer[x]+(bytes_read>>1), 0, length - bytes_read);
          mid->mixBuffer = buffer[x];
          ret = sceAudioOutputPannedBlocking(mid->channel, mid->lVolume, mid->rVolume, buffer[x]);
          if (ret < 0) printf("midi sceAudioOutputPannedBlocking 0x%x\n", ret);
          mid->offset += bytes_read>>2;
          x = (x+1)&3;

    }
    while(bytes_read);
    if (mid->loop)
       reset_mid(mid);
    else
       stopAudio(mid);
    goto start;
    return 1;
}

PAudio loadMusic(const char* filename){
       MidIStream *stream;
       MidSongOptions options;
       MidSong *song;
       long total_time;
       if (!midi){
          if (mid_init(config_file) < 0){
             printf("config fail\n");
             return NULL;
          }
          registerEndAudio(midiEnd);
          midi = 1;
       }
       
       stream = mid_istream_open_file (filename);
       if (stream == NULL){
	      printf("stream fail\n");
          return NULL;
       }
       
       options.rate = getsysFrequency();
       options.format = MID_AUDIO_S16LSB;
       options.channels = 2;
       options.buffer_size = 512;

       song = mid_song_load (stream, &options);
       mid_istream_close (stream);

       if (song == NULL){
          printf("song fail\n");
          return NULL;
       }

       total_time = mid_song_get_total_time (song);

       mid_song_set_volume (song, 100);
       mid_song_start (song);

       PAudio mid = malloc(sizeof(Audio));
       if (!mid){
          printf("mid alloc failed\n");
          mid_song_free(song);
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          return NULL;          
       }
       mid->type = 'IDIM';
       mid->channels = 2;
       mid->frequency = getsysFrequency();
       mid->bitRate = 16;
       mid->size = ((double)total_time*(double)getsysFrequency())/1000;
       mid->play = 0;
       mid->sampleCount = 512;
       mid->mixBuffer = NULL;
       mid->lVolume = PSP_VOLUME_MAX;
       mid->rVolume = PSP_VOLUME_MAX;
       mid->offset = 0;
       mid->ufunc = NULL;
       mid->filename = NULL;
       mid->loop = 0;
       mid->flags = 0;
       mid->stream = 2;
       mid->data = malloc(mid->sampleCount*2*2*4);
       if (!mid->data){
          printf("mix alloc failed\n");
          mid_song_free(song);
          free(mid);
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          return NULL;          
       }
       midiExtra *extra = malloc(sizeof(midiExtra));
       mid->extra = (audioExtra*)extra;
       extra->size = sizeof(midiExtra);
       extra->song = song;
       extra->stream = (StreamEventHandler) signal_mid;
       extra->thid = sceKernelCreateThread("midi_stream", mid_thread, 0x10, 0x400*8, PSP_THREAD_ATTR_USER, NULL);
       if (extra->thid < 0){
          printf("Error thread creation\n");
          mid_song_free(song);
          free(extra);
          free(mid);
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          return NULL;
       }
       sceKernelStartThread(mid->extra->thid, 4, &mid);

       register_audio(mid);
       return mid;
}
#endif
