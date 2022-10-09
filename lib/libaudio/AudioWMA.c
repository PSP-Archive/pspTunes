/*
 * AudioWMA.c
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
#include "AudioWMA.h"

unsigned long wma_cache_samples = 0;

u16 wma_channels  = 2;
u32 wma_samplerate = 0xAC44;
u16 wma_format_tag = 0x0161;
u32 wma_avg_bytes_per_sec = 3998;
u16 wma_block_align = 1485;
u16 wma_bits_per_sample = 16;
u16 wma_flag = 0x1F;

static SceUID libasfparser = 0xffffffff;
static SceUID modid = 0xffffffff;
static char *meBootStart = "MeBootStart.prx";

extern int registerEndAudio(EndAudio p);
extern void register_audio(PAudio audio);

void setBootStartWma(char *location){
     if (location){
        meBootStart = location;
     }
}
char *getBootStartWma(){
     return meBootStart;
}
int asf_read_cb(void *userdata, void *buf, SceSize size){
    return sceIoRead((SceUID)userdata, buf, size);
}
SceOff asf_seek_cb(void *userdata, void *buf, SceOff offset, int whence){
       return sceIoLseek((SceUID)userdata, offset, whence);
}

void reset_wma(PAudio wma){
     wmaExtra *extra = (wmaExtra*)wma->extra;
     int npt = 0 * 1000;
     int result;
     if (extra->eof){
        //printf("size %d, offset %d\n", wma->size, wma->offset);
        result = sceAsfInitParser(extra->parser, (void*)wma->file, &asf_read_cb, &asf_seek_cb);
        if (result < 0){
           printf("sceAsfInitParser 0x%x\n", result);
           if (wma->play != BUSY){
              wma->play = BUSY;
              stopAudio(wma);
           }
           return;
        } 
        extra->eof = 0;
        if (!wma->loop) extra->fill = 0;
     }
     else{     
        result = sceAsfSeekTime(extra->parser, 1, &npt);
        if (result < 0){
           printf("sceAsfSeekTime 0x%x\n", result);
           return;
        } 
        extra->fill = 0;
     }
     wma->offset = 0;
}

int signal_wma(PAudio wma, eventData *event){
    int ret;
    wmaExtra *extra = (wmaExtra*)wma->extra;
    switch (event->event){
       case EVENT_stopAudio :
          if (wma->play == PLAYING)
             wma->play = STOP;
          while (extra->working) sceKernelDelayThread(5);
          reset_wma(wma);
          wma->play = STOP;
          return 0;
       break;
       case EVENT_freeAudio :
          wma->play = TERMINATED;
          sceKernelWaitThreadEnd(wma->extra->thid, 0);
          sceKernelTerminateDeleteThread(wma->extra->thid);
          extra->wma_mix_top = -1;
          sceKernelWaitThreadEnd(extra->out_thid, 0);
          ret = sceIoClose(wma->file);
          if (ret < 0){
             audiosetLastError(ret);
          }
          free(extra->parser->pNeedMemBuffer);
          free(extra->wma_frame_buffer);
          free(extra->parser);
          sceAudiocodecReleaseEDRAM(extra->wma_codec_buffer);
          free(extra->wma_codec_buffer);
          free(wma->extra);
          return 0;
       break;
       case EVENT_seekAudio :
          while (extra->working) sceKernelDelayThread(5);
          if (event->dataType == SZ_SAMPLES){
             event->data = (double)event->data*1000.0/(double)extra->frequency;
             event->dataType = SZ_SECONDS;
          }
          if (event->dataType == SZ_SECONDS){
             int npt = event->data;
             ret = sceAsfSeekTime(extra->parser, 1, &npt);
             if (ret < 0){
                printf("sceAsfSeekTime 0x%x\n", ret);
                return 0;
             }
             extra->fill = 0;
             wma->offset = (double)npt*((double)extra->frequency/1000.0);
          }
       break;
       case EVENT_getFrequencyAudio :
          return extra->frequency;
       break;
       case EVENT_resumeComplete :
          if (wma->filename){
             SceUID fd = sceIoOpen(wma->filename, PSP_O_RDONLY, 0777);
             if (fd < 0){
                pspDebugScreenPrintf("sceIoOpen 0x%x %s\n", fd, wma->filename);
                if (wma->play == PLAYING){
                   wma->play = BUSY;
                   stopAudio(wma);
                }
                break;
             }
             wma->file = fd;
             int offset = extra->parser->sFrame.iFrameMs;
             reset_wma(wma);
             ret = sceAsfSeekTime(extra->parser, 1, &offset);
             if (ret < 0){
                pspDebugScreenPrintf("sceAsfSeekTime 0x%x\n", ret);
             }
             extra->fill = 0;
             wma->offset = (double)offset*((double)extra->frequency/1000.0);
          }
       break;
       default :
          return 0;
    }
    return 1;
}

int wma_decode(SceSize args, void *argp){
    PAudio wma = *(PAudio*)argp;
    wmaExtra *extra = (wmaExtra*)wma->extra;

    unsigned long *wma_codec_buffer = extra->wma_codec_buffer;
    short *wma_output_buffer = extra->wma_output_buffer;
    SceAsfParser* parser = extra->parser;
    int ret;
    void *wma_frame_buffer = extra->wma_frame_buffer;
    extra->wma_mix_decode = 0;

    wma_codec_buffer[6] = (unsigned long)wma_frame_buffer;
    wma_codec_buffer[9] = 12032;
  
    if (sceKernelStartThread(extra->out_thid, 4, &wma) < 0){
       wma->play = TERMINATED;
    }
 
    start:
    while (1){
          if (wma->play != PLAYING){
             if (wma->play == TERMINATED)
                return 0;
             sceKernelDelayThread(50);
             continue;
          }          

          parser->sFrame.pData = wma_frame_buffer;

          extra->working = 1;
          ret = sceAsfGetFrameData(parser, 1, &parser->sFrame);//ret = 0x86400404 last frame
          extra->working = 0;
          if (ret < 0){
             printf("sceAsfGetFrameData 0x%x\n", ret);  
             if (ret == 0x86401021){//0x86401021 broken file?
                int offset = parser->sFrame.iFrameMs+100;// wma->offset+3008;
//                seekAudio(wma, offset, SZ_SAMPLES, PSP_SEEK_SET);
                extra->working = 1;
                ret = sceAsfSeekTime(parser, 1, &offset);
                extra->working = 0;
                if (ret < 0) printf("sceAsfSeekTime 0x%x\n", ret);  
                continue;
             }
             else{
//               if (ret == 0x86400404){
                   extra->eof = 1;
                   break;
//                }
             }
          }

          wma_codec_buffer[8] = (unsigned long)(wma_output_buffer+extra->wma_mix_decode*2);
        
          wma_codec_buffer[15] = parser->sFrame.iUnk2;
          wma_codec_buffer[16] = parser->sFrame.iUnk3;
          wma_codec_buffer[17] = parser->sFrame.iUnk5;
          wma_codec_buffer[18] = parser->sFrame.iUnk6;
          wma_codec_buffer[19] = parser->sFrame.iUnk4;
         
          ret = sceAudiocodecDecode(wma_codec_buffer, SceWmaCodec);//0x807F0001,0x807F0002
          if (ret < 0){
             printf("sceAudiocodecDecode %x\n", ret);
             continue;
          }
          int samples = wma_codec_buffer[9]/4;
          extra->wma_mix_decode += samples;

          if (extra->wma_mix_decode > extra->wma_mix_output){
             extra->wma_mix_top = extra->wma_mix_decode;
          }
          if (extra->wma_mix_decode > WMA_MAX_SAMPLES){
             while (extra->wma_mix_output > extra->wma_mix_decode){
                   if (wma->play == TERMINATED) return 0;
                   sceKernelDelayThread(5);
             }
             while (extra->wma_mix_output < WMA_MAX_SAMPLES){
                   if (wma->play == TERMINATED) return 0;
                   sceKernelDelayThread(5);
             }
             extra->wma_mix_decode = 0;
          }
          else if (extra->wma_mix_decode < extra->wma_mix_output){
             if ((extra->wma_mix_output-extra->wma_mix_decode) < WMA_MAX_SAMPLES){
                if ((extra->wma_mix_top-extra->wma_mix_decode) < WMA_MAX_SAMPLES)
                   while (extra->wma_mix_decode < extra->wma_mix_output){
                         if (wma->play == TERMINATED) return 0;
                         sceKernelDelayThread(5);
                   }
                else if ((extra->wma_mix_top-extra->wma_mix_decode) == WMA_MAX_SAMPLES)
                   while (extra->wma_mix_output>extra->wma_mix_decode){
                         if (wma->play == TERMINATED) return 0;
                         sceKernelDelayThread(5);
                   }
                else
                   while ((extra->wma_mix_output-extra->wma_mix_decode) < WMA_MAX_SAMPLES){
                         if (wma->play == TERMINATED) return 0;
                         sceKernelDelayThread(5);
                   }
             }
          }
    }
    if (wma->loop)
       reset_wma(wma);
    else
       stopAudio(wma);
    goto start;
    return -1;
}
int wma_output(SceSize args, void *argp){
    PAudio wma = *(PAudio*)argp;
    wmaExtra *extra = (wmaExtra*)wma->extra;

    short *wma_output_buffer = extra->wma_output_buffer;
    int ret;
    extra->fill = 0;
    int x = 0; 

    s16 *data[4] = {wma->data, wma->data+(wma->sampleCount*2*2), wma->data+(wma->sampleCount*2*2)*2, wma->data+(wma->sampleCount*2*2)*3};

    while (1){
          if (wma->play == TERMINATED){
             sceKernelExitDeleteThread(0);}

          while (extra->wma_mix_top == 0) sceKernelDelayThread(5);
          if (isSRC() || wma->frequency == getsysFrequency()){
             while (extra->wma_mix_top - extra->wma_mix_output >= WMA_SAMPLES){
                   wma->mixBuffer = wma_output_buffer+extra->wma_mix_output*2;
                   if (isSRC())
                      ret = sceAudioSRCOutputBlocking(wma->lVolume, wma_output_buffer+extra->wma_mix_output*2);
                   else
                      ret = sceAudioOutputPannedBlocking(wma->channel, wma->lVolume, wma->rVolume, wma_output_buffer+extra->wma_mix_output*2);
                   if (ret < 0) printf("wma sceAudioOutputPannedBlocking %x %x\n", ret, wma->channel);
                   extra->wma_mix_output += WMA_SAMPLES;
                   wma->offset += WMA_SAMPLES;
             }
          }
          else{
             float i = 0.0f, scale = wma->scale;
             int fill = extra->fill, 
             wma_mix_output = extra->wma_mix_output;
             while (extra->wma_mix_top > (wma_mix_output+i)){
                   data[x][fill*2] = (wma_output_buffer+wma_mix_output*2)[(int)i*2];
                   data[x][fill*2+1] = (wma_output_buffer+wma_mix_output*2)[(int)i*2+1];
                      
                   fill++;
                   i += scale;
                   
                   if (fill == WMA_SAMPLES){
                      while (sceAudioGetChannelRestLength(wma->channel) > 0) sceKernelDelayThread(5);
                      wma->mixBuffer = data[x];
                      ret = sceAudioOutputPanned(wma->channel, wma->lVolume, wma->rVolume, data[x]);
                      fill = 0;
                      x = (x+1)&3;
                   }
             }
             extra->fill = fill;
             extra->wma_mix_output += i;
             wma->offset += i;
             if (extra->eof){
                if (fill && wma->loop == 0){
                   memset(data[x]+fill*2, 0, (WMA_SAMPLES-extra->fill)*2*2);
                   while (sceAudioGetChannelRestLength(wma->channel) > 0) sceKernelDelayThread(5);
                   wma->mixBuffer = data[x];
                   ret = sceAudioOutputPanned(wma->channel, wma->lVolume, wma->rVolume, data[x]);
                   wma->offset += fill;
                   extra->fill = 0;
                   x = (x+1)&3;
                }
             }                   
          
          }

          if (extra->wma_mix_decode < extra->wma_mix_output){
             extra->wma_mix_top = extra->wma_mix_decode;
             extra->wma_mix_output = 0;
          }
          sceKernelDelayThread(5);
    }
    return -1;
}

void wmaEnd(){
     int status;
     sceKernelStopModule(libasfparser, 0, NULL, &status, NULL);
     sceKernelUnloadModule(libasfparser);
     sceKernelStopModule(modid, 0, NULL, &status, NULL);
     sceKernelUnloadModule(modid);     
}
//the game could not be started 80220089 resetting psplink with a bad fake memstick
PAudio loadWma(const char *filename){
       int result = 0;
       SceUID wma_fd = -1;
       unsigned long *wma_codec_buffer = NULL;
       ScePVoid need_mem_buffer = NULL;
       u8 wma_getEDRAM = 0;
       int status;
       SceAsfParser* parser = 0;
       PAudio wma = NULL;
       wmaExtra* extra = NULL;
    
       if (!loadAudioCodec(PSP_AV_MODULE_AVCODEC)){
          printf("sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC) 0x%x\n", result);
          return NULL;
       }
       if (libasfparser < 0){
          registerEndAudio(wmaEnd);
          libasfparser = sceKernelLoadModule("flash0:/kd/libasfparser.prx", 0, NULL);
          if (libasfparser >= 0) {
             sceKernelStartModule(libasfparser, 0, 0, &status, NULL);
          }
          else{
             printf("sceKernelLoadModule 0x%x\n", libasfparser);
             goto error;
          }
       }
       if (modid < 0){
          modid = sceKernelLoadModule(meBootStart, 0, NULL);
          if (modid >= 0) {
             sceKernelStartModule(modid, 0, 0, &status, NULL);
          }
          else{
             printf("sceKernelLoadModule(MeBootStart) 0x%x\n", modid);
             goto error;
          }
       }
    
       wma_fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
       if (wma_fd < 0) {
          printf("sceIoOpen 0x%x\n", wma_fd);
          goto error;
       }   
       //sceAsfCheckNeedMem    
       parser = memalign(64, sizeof(SceAsfParser));
       if (!parser){
          printf("memalign(64, SceAsfParser) fail");
          goto error;
       }
       memset(parser, 0, sizeof(SceAsfParser));
    
       parser->iUnk0 = 0x01;
       parser->iUnk1 = 0x00;
       parser->iUnk2 = 0x000CC003;
       parser->iUnk3 = 0x00;
       parser->iUnk4 = 0x00000000;
       parser->iUnk5 = 0x00000000;
       parser->iUnk6 = 0x00000000;
       parser->iUnk7 = 0x00000000;
    
       result = sceAsfCheckNeedMem(parser);
       if (result < 0){
          printf("sceAsfCheckNeedMem 0x%x\n", result);
          goto error;
       }
       if (parser->iNeedMem > 0x8000){
          printf("parser->iNeedMem = 0x%08x > 0x8000\n", parser->iNeedMem);
          goto error;
       }
    
       //sceAsfInitParser
       need_mem_buffer = memalign(64, parser->iNeedMem);
       if (!need_mem_buffer) {
          printf("memalign(64, need_mem_buffer) fail");
          goto error;
       }
    
       parser->pNeedMemBuffer = need_mem_buffer;
       parser->iUnk3356 = 0x00000000;
       result = sceAsfInitParser(parser, (void*)wma_fd, &asf_read_cb, &asf_seek_cb);
       if (result < 0){
          printf("sceAsfInitParser 0x%x\n", result);
          goto error;
       }
       wma_channels = *((u16*)need_mem_buffer);
       wma_samplerate = *((u32*)(need_mem_buffer+4));
       wma_format_tag = 0x0161;// 0x0164 pro 0x0200 lossless
       wma_avg_bytes_per_sec = *((u32*)(need_mem_buffer+8));
       wma_block_align = *((u16*)(need_mem_buffer+12));
       wma_bits_per_sample = 16;
       wma_flag = *((u16*)(need_mem_buffer+14));
       
       wma_codec_buffer = memalign(64, sizeof(unsigned long)*65);
       if (!wma_codec_buffer) {
          printf("memalign wma_codec_buffer fail\n");
          goto error;
       }
       memset(wma_codec_buffer, 0, sizeof(unsigned long)*65);
    
       //CheckNeedMem
       wma_codec_buffer[5] = 1;
       result = sceAudiocodecCheckNeedMem(wma_codec_buffer, SceWmaCodec);
       if (result < 0){
          printf("sceAudiocodecCheckNeedMem 0x%x\n", result);
          goto error;
       }
    
       //GetEDRAM
       result = sceAudiocodecGetEDRAM(wma_codec_buffer, SceWmaCodec);//sceAudiocodecGetEDRAM=0x807F0003 no boot start
       if (result < 0){
          printf("sceAudiocodecGetEDRAM 0x%x\n", result);
          goto error;
       }
       wma_getEDRAM = 1;
    
       //Init
       u16* p16 = (u16*)(&wma_codec_buffer[10]);
       p16[0] = wma_format_tag;
       p16[1] = wma_channels;
       wma_codec_buffer[11] = wma_samplerate;
       wma_codec_buffer[12] = wma_avg_bytes_per_sec;
       p16 = (u16*)(&wma_codec_buffer[13]);
       p16[0] = wma_block_align;
       p16[1] = wma_bits_per_sample;
       p16[2] = wma_flag;
    
       result = sceAudiocodecInit(wma_codec_buffer, SceWmaCodec);
       if (result < 0){
          printf("sceAudiocodecInit 0x%x\n", result);
          goto error;
       }
    
       int npt = 0;
       result = sceAsfSeekTime(parser, 1, &npt);
    
       if (result < 0) {    
          printf("sceAsfSeekTime 0x%x %d\n", result, npt);        
          goto error;
       }
       wma = malloc(sizeof(Audio));
       if (!wma) goto error;
       wma->type = SceWmaCodec;
       wma->channels = wma_channels;
       wma->frequency = wma_samplerate;
       wma->bitRate = wma_bits_per_sample;
       wma->stream = 1;
       wma->size = parser->lDuration/10000;
       wma->size = (u32)(((double)wma->size*(double)wma_samplerate/(double)1000));
       wma->sampleCount = WMA_SAMPLES;
       wma->scale = (float)wma_samplerate/(float)getsysFrequency();
       wma->offset = 0;
       wma->frac = 0;
       wma->channel = -1;
       wma->lVolume = PSP_VOLUME_MAX;
       wma->rVolume = PSP_VOLUME_MAX;
       wma->play = STOP;
       wma->loop = 0;
       wma->data = NULL;
       wma->filename = NULL;
       wma->file = wma_fd;
       wma->fstate = STATE_F_OPEN;
       extra = malloc(sizeof(wmaExtra));
       if (!extra){
          printf("malloc wma extra fail\n");
          goto error;
       }
       wma->extra = (audioExtra*)extra;        
       extra->size = sizeof(wmaExtra);
       extra->parser = parser;
       extra->wma_codec_buffer = wma_codec_buffer;
       extra->frequency = wma->frequency;
       extra->eof = 0;
       if (!strrchr(filename, ':')){
          char path[256];
          getcwd(path, 256);
          wma->filename = malloc(strlen(path)+strlen(filename)+2);
          if (wma->filename){
             strcat(strcat(strcpy(wma->filename, path), "/"), filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       else{ 
          wma->filename = malloc(strlen(filename)+1);
          if (wma->filename){
             strcpy(wma->filename, filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       wma->data = malloc(wma->sampleCount*2*2*4);
       if (!wma->data){
          printf("malloc wma mix buffer2 fail\n");
          goto error;
       }
       extra->wma_output_buffer = memalign(64, WMA_MAX_OUTPUT*2);
       if (!extra->wma_output_buffer){
          printf("memalign wma mix buffer1 fail\n");
          goto error;
       }
       memset(extra->wma_output_buffer, 0, WMA_MAX_OUTPUT*2);
       extra->wma_frame_buffer = memalign(64, wma_block_align);
       if (!extra->wma_frame_buffer) {
          printf("memalign wma_frame_buffer fail\n");
          goto error;
       }
       extra->stream = (StreamEventHandler)signal_wma;
       extra->dec_thid = sceKernelCreateThread("wma_decode_stream",wma_decode , 0x10, 20*0x400, PSP_THREAD_ATTR_USER, NULL);//sceAsfSeekTime uses a shitload of stack
       if (extra->dec_thid < 0){
          printf("Error decode thread creation 0x%x\n", extra->dec_thid);
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          goto error;
       }
       extra->out_thid = sceKernelCreateThread("wma_output_stream", wma_output, 0x10, 0x400, PSP_THREAD_ATTR_USER, NULL);
       if (extra->out_thid < 0){
          printf("Error output thread creation 0x%x\n", extra->out_thid);
          sceKernelDeleteThread(wma->extra->thid);
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          goto error;
       }
       sceKernelStartThread(wma->extra->thid, 4, &wma);
       register_audio(wma);
       return(wma);
error:
      if (!(wma_fd<0))
          sceIoClose(wma_fd);
      if (parser)
          free(parser);
      if (need_mem_buffer)
          free(need_mem_buffer);
      if (wma){    
         if (wma->extra){
            if (extra->wma_output_buffer)
               free(extra->wma_output_buffer);
            if (extra->wma_frame_buffer)
               free(extra->wma_frame_buffer);
            free(wma->extra);
         }
         if (wma->filename)
            free(wma->filename);
         if (wma->data)
            free(wma->data);
         free(wma);
      }
      if (wma_getEDRAM)
         sceAudiocodecReleaseEDRAM(wma_codec_buffer);
      if (wma_codec_buffer)
         free(wma_codec_buffer);
      return NULL;
}
