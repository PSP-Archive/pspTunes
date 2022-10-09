/*
 * AudioOGG.c
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
#include "AudioOGG.h"

extern void register_audio(PAudio);
void reset_ogg(PAudio ogg){
     ogg_extra *extra = (ogg_extra*)ogg->extra;
     while (extra->working) sceKernelDelayThread(10);
     if (ov_pcm_seek(extra->vf, 0) == 0){
        ogg->offset = 0;
        ogg->frac = 0;
        extra->fill = 0;
        extra->eof = 0;
     }
     else{ 
        printf("ogg reset failed\n");
        ogg->play = BUSY;//Stops it from playing
     }
}
int signal_ogg(PAudio ogg, eventData *event){
    ogg_extra *extra = (ogg_extra*)ogg->extra;
    switch (event->event){
       case EVENT_stopAudio :
          ogg->play = STOP;
          while (extra->working) sceKernelDelayThread(10);
		  reset_ogg(ogg);
		  return 0;
       break;
       case EVENT_freeAudio :
          ogg->play = TERMINATED;
          sceKernelWaitThreadEnd(ogg->extra->thid, 0);
          sceKernelTerminateDeleteThread(ogg->extra->thid);
          ov_clear(extra->vf);
          free(ogg->extra);
          return 0;
       break;
       case EVENT_getFrequencyAudio :
          return extra->frequency;
       break;
       case EVENT_getBitrateAudio :{//GCC :P
          int play = ogg->play;
          ogg->play = BUSY;
          while (extra->working) sceKernelDelayThread(5);
          vorbis_info *vi=ov_info(extra->vf,-1);
          ogg->play = play;
          if (vi->bitrate_nominal)
             return vi->bitrate_nominal;
          if (vi->bitrate_upper)
             return vi->bitrate_upper;
          return 0;
       }   
       break;
       case EVENT_seekAudio :{//GCC :P
          int seek = event->data;
          while (extra->working) sceKernelDelayThread(5);
          if (ov_pcm_seek(extra->vf, seek) == 0){
             ogg->offset = seek;
             ogg->frac = 0;
             extra->fill = 0;
          }
       }
       break;
       case EVENT_suspending :
          extra->offset = ftell((FILE*)ogg->file);
       break;
       case EVENT_resumeComplete :
          if (ogg->filename){//GCC :P
             FILE *file = fopen(ogg->filename, "rb");
             if (!file) break;
             extra->vf->datasource = file;
             ogg->file = (u32)file;
             fseek(file, extra->offset, SEEK_SET);
          }
       break;
       default :
          return 0;
    }
    return 1;
}
int ogg_thread(SceSize args, void *argp){
    PAudio ogg = *(PAudio*)argp;
    ogg_extra *extra = (ogg_extra*)ogg->extra;
    int bps = 2*ogg->channels;
    int x = 0;
    int current_section;
    int ret = 0;
    int length = OGG_SAMPLES*ogg->channels*2;
    long samples;
    extra->fill = 0; 
    short *data[4] = {ogg->data, ogg->data+(length), ogg->data+(length)*2, ogg->data+(length)*3};
    short *out = ogg->data+(length)*4;
    while (1){//till something better comes along
          if (ogg->play != PLAYING){
             if (ogg->play == TERMINATED) return 0;
             sceKernelDelayThread(50);
             continue;
          }
          
          extra->working = 1;
#ifdef USE_TREMOR
          samples = ov_read(extra->vf, (char*)out, length, &current_section);
#else
          samples = ov_read(extra->vf, (char*)out, length, 0, 2, 1, &current_section);
#endif
          extra->working = 0;
          if (samples == 0){
              /* EOF */
              if (!ogg->loop){
                 if (extra->fill){
                    memset(data[x]+(extra->fill*ogg->channel), 0, (OGG_SAMPLES-extra->fill)*bps);
                    ogg->mixBuffer = data[x];
                    if (isSRC())
                       ret = sceAudioSRCOutputBlocking(ogg->lVolume, data[x]);
                    else
                       ret = sceAudioOutputPannedBlocking(ogg->channel, ogg->lVolume, ogg->rVolume, data[x]);
                 }
              }
             extra->eof=1;
             printf("ogg eof\n");
          } 
          else if (samples < 0){
             printf("ogg read error %lx\n", samples);
             extra->eof=1;
          }  
          else{
             samples /= bps;
             ogg->offset += samples;
             if ((ogg->frequency != getsysFrequency()) && !isSRC()){
                float i = 0;
                while (i < samples){
                      data[x][extra->fill*ogg->channels] = out[((int)i)*ogg->channels];

                      if (ogg->channels == 2) {
                         data[x][extra->fill*ogg->channels+1] = out[((int)i)*ogg->channels+1];
                      }
                      extra->fill++;
                      i += ogg->scale;
                      if (extra->fill == OGG_SAMPLES){
                         ogg->mixBuffer = data[x];
                         ret = sceAudioOutputPannedBlocking(ogg->channel, ogg->lVolume, ogg->rVolume, data[x]);
                         if (ret < 0) printf("ogg sceAudioOutputPannedBlocking = %x\n", ret);
                         extra->fill = 0;
                         x = (x+1)&3;
                      }
                }
             }
             else{
                int i = 0;
                while (samples > 0){
                      if (OGG_SAMPLES > samples+extra->fill){
                         memcpy(data[x]+(extra->fill*ogg->channels), out+(i*ogg->channels), (samples)*bps);
                         extra->fill += samples;
                         samples = 0;
                      }
                      else{
                         memcpy(data[x]+(extra->fill*ogg->channels), out+(i*ogg->channels), (OGG_SAMPLES-extra->fill)*bps);
                         ogg->mixBuffer = data[x];
                         if (isSRC())
                            ret = sceAudioSRCOutputBlocking(ogg->lVolume, data[x]);
                         else
                            ret = sceAudioOutputPannedBlocking(ogg->channel, ogg->lVolume, ogg->lVolume, data[x]);
                         if (ret < 0) printf("ogg sceAudioOutputPannedBlocking %x\n", ret);
                         i += OGG_SAMPLES-extra->fill;
                         samples -= OGG_SAMPLES-extra->fill;
                         extra->fill = 0;
                         x = (x+1)&3;                      
                      }
                }
             }
          }

          if (extra->eof==1){
             if (ogg->loop){
                int fill = ogg->extra->fill;
                reset_ogg(ogg);
                ogg->extra->fill = fill;
             }
             else
                stopAudio(ogg);
          }
          sceKernelDelayThread(10);
    }
    return 0;     
}

PAudio loadOgg(const char *filename, int stream){
       if (!filename) return NULL;
       OggVorbis_File *vf;
       int op = 0;
       int current_section;
       PAudio ogg = NULL;
       int ret;

       FILE *in = fopen(filename, "rb");
       if (!in){ 
          printf("error loading %s\n", filename);
          return NULL;
       } 
       vf = malloc(sizeof(OggVorbis_File));
       if (!vf){ 
          printf("error allocating vf\n");
          goto error;
       }
       ret = ov_open(in, vf, NULL, 0);
       if (ret < 0) {
          printf("Input does not appear to be an Ogg-Vorbis bitstream. %d\n", ret);
#ifndef USE_TREMOR
          free(vf);
          fclose(in);
#ifdef OGG_FLAC
          return loadFlacAdvance((int)filename, stream, 0, 0, FLAC_FLAG_OGG);//check to see if it contains a flac stream
#endif
          return NULL;
#endif
          goto error;
       }
       op = 1;
  
       vorbis_info *vi=ov_info(vf,-1);
       int totalSamples = ov_pcm_total(vf,-1);
  
       ogg = malloc(sizeof(Audio));
       if (!ogg){ 
          printf("error allocating ogg\n");
          goto error;
       }
       ogg->type = VorbisCodec;
       ogg->channels = vi->channels;
       ogg->frequency = vi->rate;
       ogg->bitRate = 16;
       ogg->data = NULL;
       ogg->size = totalSamples;
       ogg->sampleCount = OGG_SAMPLES;
       ogg->mixBuffer = NULL;
       ogg->scale = (float)ogg->frequency/(float)getsysFrequency();
       ogg->offset = 0;
       ogg->frac = 0;
       ogg->stream = stream;
       ogg->channel = 0x80260008;
       ogg->lVolume = PSP_AUDIO_VOLUME_MAX;
       ogg->rVolume = PSP_AUDIO_VOLUME_MAX;
       ogg->play = STOP;
       ogg->loop = 0;
       ogg->fstate = STATE_F_OPEN;
       ogg->file = (u32)in;
       ogg->ufunc = NULL;
       ogg->filename = NULL;
       ogg->flags = AUDIO_FLAG_FILE;
       ogg->extra = NULL;
       
       int eof = 0;
       if (!stream){
          int bps = 2*ogg->channels;
          int length = totalSamples*bps;
          ogg->data = malloc(length);
          if (!ogg->data){ 
             printf("error allocating ogg->data\n");
             goto error;
          }
          while (!eof){
#ifdef USE_TREMOR
                long ret=ov_read(vf, ogg->data+(ogg->offset*bps), length-(ogg->offset*bps), &current_section);
#else
                long ret=ov_read(vf, ogg->data+(ogg->offset*bps), length-(ogg->offset*bps), 0, 2, 1, &current_section);
#endif
                if (ret == 0){
                   /* EOF */
                   eof=1;
                   printf("ogg eof\n");
                } 
                else if (ret < 0){
	               printf("ogg read %ld\n", ret);
	               if (ret == OV_EBADLINK)
	                  eof=1;
                } 
                else{
                   ogg->offset += ret/bps;
                }
          }
          ogg->size = ogg->offset;
          ogg->offset = 0;
          ogg->type = 0;
          ov_clear(vf);
          ogg->fstate = STATE_F_CLOSE;
          register_audio(ogg);
          return ogg;
       }
       ogg_extra *extra = malloc(sizeof(ogg_extra));
       if (!extra){ 
          printf("error allocating ogg->extra\n");
          goto error;
       }
       ogg->extra = (audioExtra*)extra;
       extra->size = sizeof(ogg_extra);
       extra->working = 0;
       extra->frequency = ogg->frequency;
       if (!strrchr(filename, ':')){
          char path[256];
          getcwd(path, 256);
          ogg->filename = malloc(strlen(path)+strlen(filename)+2);
          if (ogg->filename){
             strcat(strcat(strcpy(ogg->filename, path), "/"), filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       else{ 
          ogg->filename = malloc(strlen(filename)+1);
          if (ogg->filename){
             strcpy(ogg->filename, filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       ogg->data = malloc(OGG_SAMPLES*ogg->channels*2*5);
       if (!ogg->data){ 
          printf("error allocating ogg->data\n");
          goto error;
       }
       extra->thid = sceKernelCreateThread("ogg_stream", ogg_thread, 0x10, 0x400*24, PSP_THREAD_ATTR_USER, NULL);//8k stack is big enough for ordinary streams
       if (extra->thid < 0){
          printf("Error ogg thread creation\n");
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          goto error;
       }
       sceKernelStartThread(ogg->extra->thid, 4, &ogg);
       extra->stream = (StreamEventHandler)signal_ogg;
       extra->vf = vf;
       extra->start = 0;
       extra->eof = 0;
       extra->fill = 0;

       register_audio(ogg);
       return ogg;
error:
      if (op){
         ov_clear(vf);
      }
      else{
         if (in) fclose(in);
      }
      if (vf) free(vf);
      if (ogg){
         if (ogg->data) free(ogg->data);
         if (ogg->extra) free(ogg->extra);
         if (ogg->filename) free(ogg->filename);
         free(ogg);
      }
      return NULL;
}

OggVorbis_File *getVorbisFileOgg(const PAudio ogg){
               if (!ogg) return NULL;
               if (ogg->type != VorbisCodec) return NULL;
               ogg_extra *extra = (ogg_extra*)ogg->extra;
               return extra->vf;
}
#endif
