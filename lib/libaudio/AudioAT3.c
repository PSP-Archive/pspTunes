/*
 * AudioAT3.c
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
#include "AudioAT3.h"

extern int getID3v2TagSize(const char *id3);
extern void seekAudio0(PAudio audio, int seekTime);
extern int sceAtracGetBufferInfoForReseting(int, int, void*);
extern int sceAtracResetPlayPosition(int, int, int, int);
void at3_reset(PAudio at3){
     int ret;
     int fsize, readSize = 32*1024;
     at3_extra *extra = (at3_extra*)at3->extra;
     _PspBufferInfo pBufferInfo;
     sceAtracGetBufferInfoForReseting(extra->ID, 0, &pBufferInfo);
     u8* data = pBufferInfo.pucWritePositionFirstBuf;
     sceAtracReleaseAtracID(extra->ID);
     if (at3->stream == STREAM_FROM_FILE){
        fsize = sceIoLseek32(at3->file, 0, PSP_SEEK_END);
        sceIoLseek32(at3->file, 0, PSP_SEEK_SET);
        if (readSize > fsize)
           readSize = fsize;
        ret = sceIoRead(at3->file, data, readSize);
        if (ret <= 0){
           printf("at3 reset: read file 0x%08x\n", ret);//nothing else?
        }
     }
     else if (at3->stream == STREAM_FROM_MEMORY){
        if (readSize > at3->file)
           readSize = at3->file;
        memcpy(data, extra->data, readSize);
     }
     int ID = sceAtracSetDataAndGetID(data, readSize);
     if (ID < 0){
        printf("at3 reset: 0x%08x %p\n", ID, data);//nothing else? again?
        stopAudio(at3);
     }
     extra->ID = ID;
     at3->offset = 0;
}
void aa3_reset(PAudio aa3){
     seekAudio0(aa3, 0);
}

void at3_seek(PAudio at3, int offset){
     int ret;
     at3_extra *extra = (at3_extra*)at3->extra;
     int ID = extra->ID;
     while (extra->working) sceKernelDelayThread(50);
     ret = sceAtracResetPlayPosition(ID, offset, 0, 0);
     if (ret == 0x80630016){
        printf("at3 seek: reset 0x%08x\n", ret);
        _PspBufferInfo pBufferInfo;
        ret = sceAtracGetBufferInfoForReseting(ID, offset, &pBufferInfo);
        if (ret < 0){
           printf("at3 seek: sceAtracGetBufferInfoForReseting 0x%08x\n", ret);
           audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
           return;
        }
        if (at3->stream == STREAM_FROM_MEMORY){
           memcpy(pBufferInfo.pucWritePositionFirstBuf, extra->data+pBufferInfo.uiReadPositionFirstBuf, pBufferInfo.uiWritableByteFirstBuf);
        }
        else{
           ret = sceIoLseek32(at3->file, pBufferInfo.uiReadPositionFirstBuf, PSP_SEEK_SET);
           if (ret < 0){
              printf("at3 seek: 0x%08x\n", ret);
              audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
              return;
           }
           ret = sceIoRead(at3->file, pBufferInfo.pucWritePositionFirstBuf, pBufferInfo.uiWritableByteFirstBuf);
           if (ret <= 0){
              printf("at3 seek: read file 0x%08x\n", ret);
              audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
              return;
           }
        }
        ret = sceAtracResetPlayPosition(ID, offset, pBufferInfo.uiWritableByteFirstBuf, 0);
        if (ret < 0){
           printf("at3 seek: sceAtracGetBufferInfoForReseting 0x%08x\n", ret);
           audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
           return;
        }
     }
     else if (ret < 0){
        printf("at3 seek: sceAtracResetPlayPosition = 0x%08x\n", ret);
        audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
        return;
     }
     at3->offset = offset;
}
void aa3_seek(PAudio aa3, int offset){
     if (!aa3) return;
     if (offset < 0) return;
     if (offset > aa3->size) return;
     aa3_extra *extra = (aa3_extra*)aa3->extra;
     if (sceIoLseek32(aa3->file, (offset/aa3->sampleCount)*extra->aa3_data_align+extra->start, PSP_SEEK_SET) < 0){
        printf("aa3 seek\n");
        aa3->play = BUSY;   
     }
     aa3->offset = (offset/aa3->sampleCount)*aa3->sampleCount;
     extra->fill = 0;
     extra->eof = 0;
}

int at3_EventHandler(PAudio at3, eventData *event){
    int ret;
    at3_extra *extra = (at3_extra*)at3->extra;
    switch (event->event){
       case EVENT_stopAudio :
          at3->play = STOP;
          while (extra->working) sceKernelDelayThread(5);
          at3_reset(at3);
          return 0;
       break;
       case EVENT_freeAudio :
          at3->play = TERMINATED;
          sceKernelWaitThreadEnd(at3->extra->thid, 0);
          sceKernelTerminateDeleteThread(at3->extra->thid);
          _PspBufferInfo pBufferInfo;
          sceAtracGetBufferInfoForReseting(extra->ID, 0, &pBufferInfo);
          sceAtracReleaseAtracID(extra->ID);  
          free(pBufferInfo.pucWritePositionFirstBuf);
          sceIoClose(at3->file);
          if (at3->stream == STREAM_FROM_MEMORY)
             free(extra->data);
          free(at3->extra);
          return 0;
       break;
       case EVENT_seekAudio :
          at3_seek(at3, event->data);
       break;
       case EVENT_getFrequencyAudio :
          return extra->frequency;
       break;
       case EVENT_getBitrateAudio :{//GCC :P
          int Bitrate, play = at3->play;
          at3->play = BUSY;
          while (extra->working) sceKernelDelayThread(5);
          ret = sceAtracGetBitrate(extra->ID, &Bitrate);
          if (ret < 0){
             audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
             at3->play = play;
             return 0;
          }
          at3->play = play;
          return Bitrate*1000;
       }
       break;
       default :
          return 0;
    }
    return 1;
}
int aa3_EventHandler(PAudio aa3, eventData *event){
    if (!aa3) return 1;
    aa3_extra* extra = (aa3_extra*)aa3->extra;
    int ret = 0;
    switch (event->event){
       case EVENT_stopAudio :
          aa3->play = STOP;
		  while (extra->working) sceKernelDelayThread(5);
          aa3_reset(aa3);
		  return 0;
       break;
       case EVENT_freeAudio : 
          aa3->play = TERMINATED;
          sceKernelWaitThreadEnd(aa3->extra->thid, 0);
          sceKernelTerminateDeleteThread(aa3->extra->thid);
          ret = sceIoClose(aa3->file);
          if (ret < 0){
             audiosetLastError(ret);
          }
          sceAudiocodecReleaseEDRAM(extra->aa3_codec_buffer);
          free(extra->aa3_codec_buffer);
          free(extra->data);
          free(extra);
          return 0;
       break;
       case EVENT_getFrequencyAudio :
          return extra->frequency;
       break;
       case EVENT_getBitrateAudio :
          return extra->aa3_data_align*((float)extra->frequency/(float)aa3->sampleCount);
       break;
       case EVENT_seekAudio :
          while (extra->working) sceKernelDelayThread(5);
          aa3_seek(aa3, event->data);
       break;
       default :
          return 0;
    }
    return 1;
}

int decodeAa3Frame(PAudio aa3, s16 *data){
    aa3_extra *extra = (aa3_extra*)aa3->extra;
    if (aa3->type == SceAt3Codec){ 
       if (sceIoRead(aa3->file, extra->data, extra->aa3_data_align) != extra->aa3_data_align){ 
          return 0; 
       } 
       if (extra->aa3_channel_mode){ 
          memcpy(extra->data+extra->aa3_data_align, extra->data, extra->aa3_data_align); 
       } 
    } 
    else{ 
       if (sceIoRead(aa3->file, extra->data+8, extra->aa3_data_align) != extra->aa3_data_align){ 
          return 0; 
       } 
    } 
    
    extra->aa3_codec_buffer[6] = (unsigned long)extra->data; 
    extra->aa3_codec_buffer[8] = (unsigned long)data; 
    
    int res = sceAudiocodecDecode(extra->aa3_codec_buffer, aa3->type); 
    if (res < 0){
       printf("aa3 decode 0x%08x\n", res);
       return 0;
    }
    return aa3->sampleCount;
}

int at3_thread(SceSize args, void *argp){   
    PAudio at3 = *(PAudio*)argp;
    at3_extra *extra = (at3_extra*)at3->extra;
    int ID = extra->ID;
    int x = 0;
    int ret;
    s16 *data[4] = {at3->data, at3->data+(at3->sampleCount*2*2), at3->data+(at3->sampleCount*2*2)*2, at3->data+(at3->sampleCount*2*2)*3};
    s16 *read = at3->data+(at3->sampleCount*2*2)*4;
    restart:
    do{ 
        if (at3->play != PLAYING){
           if (at3->play == TERMINATED) return 0;
           sceKernelDelayThread(50);
           continue;
        }
        int outEnd = 0, outN, outRemainFrame;
        extra->working = 1;
        ret = sceAtracDecodeData(ID, (!isSRC())&&(at3->frequency!=getsysFrequency())?(u16*)read:(u16*)data[x], &outN, &outEnd, &outRemainFrame);
        extra->working = 0;
        if (ret < 0){
           break;
        }
        int samples = outN;
        if (isSRC()){
           at3->mixBuffer = data[x];
           ret = sceAudioSRCOutputBlocking(at3->lVolume, data[x]);
           x = (x+1)&3;
        }
        else{
           if (at3->frequency != getsysFrequency()){
              float i = 0;
              while (i < samples){
                    data[x][extra->fill*2] = read[(int)i*2];
                    data[x][extra->fill*2+1] = read[(int)i*2+1];
                       
                    extra->fill++;
                    i += at3->scale;
                    
                    if (extra->fill == at3->sampleCount){
                       while (sceAudioGetChannelRestLength(at3->channel) > 0) sceKernelDelayThread(5);
                       at3->mixBuffer = data[x];
                       ret = sceAudioOutputPanned(at3->channel, at3->lVolume, at3->rVolume, data[x]);
                       extra->fill = 0;
                       x = (x+1)&3;
                    }
              }
              if (extra->eof){
                 if (extra->fill && at3->loop == 0){
                    memset(data[x]+extra->fill*2, 0, (at3->sampleCount-extra->fill)*2*2);
                    while (sceAudioGetChannelRestLength(at3->channel) > 0) sceKernelDelayThread(5);
                    at3->mixBuffer = data[x];
                    ret = sceAudioOutputPanned(at3->channel, at3->lVolume, at3->rVolume, data[x]);
                    extra->fill = 0;
                    x = (x+1)&3;
                 }
              }                   
           }
           else{
              while (sceAudioGetChannelRestLength(at3->channel) > 0) sceKernelDelayThread(5);
              at3->mixBuffer = data[x];
              ret = sceAudioOutputPanned(at3->channel, at3->lVolume, at3->rVolume, data[x]);
              x = (x+1)&3;
           }
        }
        if (ret < 0){
           printf("at3 audio output 0x%08x\n", ret);
        }
        at3->offset += outN;
        if (outEnd){
           break;
        }
        if (outRemainFrame == 0){
           u8 *writePointer;
           u32 availableBytes, readOffset;
           extra->working = 1;
           ret = sceAtracGetStreamDataInfo(ID, &writePointer, &availableBytes, &readOffset);
           if (ret < 0){
              printf("at3 sceAtracGetStreamDataInfo 0x%08x\n", ret);
           }
           if (at3->stream == STREAM_FROM_MEMORY)
              memcpy(writePointer, extra->data+readOffset, availableBytes);
           else if (at3->stream == STREAM_FROM_FILE)
              sceIoRead(at3->file, writePointer, availableBytes);
           ret = sceAtracAddStreamData(ID, availableBytes);
           extra->working = 0;
           if (ret < 0){
              printf("at3 sceAtracAddStreamData 0x%08x\n", ret);
              stopAudio(at3);
           }
        }
        sceKernelDelayThread(10);
    }
    while (1);
    if (at3->loop)
       at3_reset(at3);
    else
       stopAudio(at3);
    goto restart;
    return 1;
}
int aa3_thread(SceSize args, void *argp){
    PAudio aa3 = *(PAudio*)argp;
    if (!aa3) return -1;
    aa3_extra *extra = (aa3_extra*)aa3->extra;
    int x = 0;
    int ret;
    s16 *data[4] = {aa3->data, aa3->data+(aa3->sampleCount*2*2), aa3->data+(aa3->sampleCount*2*2)*2, aa3->data+(aa3->sampleCount*2*2)*3};//remember 2 not 4
    s16 *read = aa3->data+(aa3->sampleCount*2*2)*4;
    start:
    do{
        if (aa3->play != PLAYING){
           if (aa3->play == TERMINATED) return 0;
           sceKernelDelayThread(50);
           continue;
        }
        extra->working = 1;
        ret = decodeAa3Frame(aa3, (!isSRC())&&(aa3->frequency!=getsysFrequency())?read:data[x]);
        extra->working = 0;
        if (ret == 0){
           extra->eof = 1;
           if (isSRC() || aa3->frequency == getsysFrequency()){
              break;
           }
        }
        int samples = ret;

        aa3->offset += samples;

        if (isSRC()){
           aa3->mixBuffer = data[x];
           ret = sceAudioSRCOutputBlocking(aa3->lVolume, data[x]);
           x = (x+1)&3;
        }
        else{
           if (aa3->frequency != getsysFrequency()){
              float i = 0;
              while (i < samples){
                    data[x][extra->fill*2] = read[(int)i*2];
                    data[x][extra->fill*2+1] = read[(int)i*2+1];
                    extra->fill++;
                    i += aa3->scale;
                    if (extra->fill == aa3->sampleCount){
                       while (sceAudioGetChannelRestLength(aa3->channel) > 0) sceKernelDelayThread(5);
                       aa3->mixBuffer = data[x];
                       ret = sceAudioOutputPanned(aa3->channel, aa3->lVolume, aa3->rVolume, data[x]);
                       extra->fill = 0;
                       x = (x+1)&3;
                    }
              }
              if (extra->eof){
                 if (extra->fill && aa3->loop == 0){
                    memset(data[x]+extra->fill*2, 0, (aa3->sampleCount-extra->fill)*2*2);
                    while (sceAudioGetChannelRestLength(aa3->channel) > 0) sceKernelDelayThread(5);
                    aa3->mixBuffer = data[x];
                    ret = sceAudioOutputPanned(aa3->channel, aa3->lVolume, aa3->rVolume, data[x]);
                    extra->fill = 0;
                    x = (x+1)&3;
                 }
              }                   
           }
           else{
              while (sceAudioGetChannelRestLength(aa3->channel) > 0) sceKernelDelayThread(5);
              aa3->mixBuffer = data[x];
              ret = sceAudioOutputPanned(aa3->channel, aa3->lVolume, aa3->rVolume, data[x]);
              x = (x+1)&3;
           }
        }
        if (ret < 0)
           printf("aa3 audio output 0x%08x\n", ret);
        if (extra->eof) break;
    }
    while (1);//meh
    if (aa3->loop){
       int fill = extra->fill;
       aa3_reset(aa3);
       extra->fill = fill;
    }
    else
       stopAudio(aa3);
    goto start;
    return 1;
}

extern int getFormat(fmt *f, SceUID in, int dsize);
extern void register_audio(PAudio audio);
extern void unregister_audio(PAudio audio);
PAudio loadAt3(const char *file, u32 stream){
       if (!file) return NULL;
       if (stream > STREAM_FROM_MEMORY) return NULL;
       int ret;
       SceUID in = sceIoOpen(file, PSP_O_RDONLY, 0777);
       if (in < 0){
          printf("at3 open file %s %d\n", file, in);
          audiosetLastError(AUDIO_ERROR_OPEN_FILE);
          return NULL;
       }

       RIFFHeader head; 
       sceIoRead(in, &head, 12);
       if (head.fsig != RIFF){
          printf("at3 format RIFF\n");
          audiosetLastError(AUDIO_ERROR_FILE_READ);
          sceIoClose(in);
          return NULL;
       } 
       if (head.fmt != WAVE){
          printf("at3 format WAVE\n");
          audiosetLastError(AUDIO_ERROR_INVALID_FILE);
          sceIoClose(in);
          return NULL;
       }

       chunk cnk;
       fmt f;
       int totalSamples;
       int smpl = 0;
       while (sceIoRead(in, &cnk, 8) == 8){
             if (cnk.csig == fmt__){
                getFormat(&f, in, cnk.dsize);
                cnk.dsize = 0;
             }
             else if (cnk.csig == data_){
                break;
             }
             else if (cnk.csig == fact_){
                sceIoRead(in, &totalSamples, 4);
                cnk.dsize -= 4;
             }
             else if (cnk.csig == smpl_){
                smpl = 1;
             }
             sceIoLseek32(in, cnk.dsize, PSP_SEEK_CUR);
       }
       if (!smpl && f.id == SceAtrac3plusCodec)
          totalSamples /= 4;
       if (!loadAudioCodec(PSP_AV_MODULE_ATRAC3PLUS)){
          audiosetLastError(AUDIO_ERROR_LOAD_MODULES);
          printf("at3 load av module avcodec \n");
          sceIoClose(in);
          free(cnk.data);
          return NULL;
       }
       int fsize = sceIoLseek32(in, 0, PSP_SEEK_END);
       sceIoLseek32(in, 0, PSP_SEEK_SET);
       int size = 32*1024;
       if (stream == STREAM_NO_STREAM){
          size = fsize;
       }
       else if (fsize < size){
          size = fsize;
          stream = STREAM_FROM_FILE;
       }
       cnk.data = malloc(size);
       ret = sceIoRead(in, cnk.data, size);
       if (ret != size){
          audiosetLastError(AUDIO_ERROR_FILE_READ);
          printf("at3 read file 0x%08x\n", ret);
          sceIoClose(in);
          free(cnk.data);
          return NULL;
       }
       int ID = sceAtracSetDataAndGetID(cnk.data, size);
       if (ID < 0){
          audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
          printf("at3 sceAtracSetDataAndGetID 0x%08x\n", ID);
          free(cnk.data);
          sceIoClose(in);
          return NULL;
       }
       int maxSamples;
       ret = sceAtracGetMaxSample(ID, &maxSamples);
       if (ret < 0){
          printf("at3 sceAtracGetMaxSample 0x%08x\n", ret);
          sceAtracReleaseAtracID(ID);
          free(cnk.data);
          audiosetLastError(AUDIO_ERROR_OPERATION_FAILED);
          return NULL;
       }
       
       void *data = NULL;
       int samples = 0;
       if (!stream){
          data = malloc(totalSamples*4);
          void *tmp = data;
          samples = 0;
          int outEnd;
          do{
             int outN, outRemainFrame;
             ret = sceAtracDecodeData(ID, (u16*)tmp, &outN, &outEnd, &outRemainFrame);
             if (ret < 0){
                printf("at3 sceAtracDecodeData 0x%08x\n", ret);
                break;
             }
             samples += outN;
             tmp = ((u32*)tmp)+outN;//(u(16*2)*)
          }
          while (!outEnd);
          ret = sceAtracReleaseAtracID(ID);
          if (ret < 0){
             printf("at3 sceAtracReleaseAtracID 0x%08x\n", ret);
          }
          free(cnk.data);
          sceIoClose(in);
       }
       else if (stream == STREAM_FROM_MEMORY){
          size = sceIoLseek32(in, 0, PSP_SEEK_END);  
          sceIoLseek32(in, 0, PSP_SEEK_SET);  
          data = malloc(size);
          sceIoRead(in, data, size);          
          sceIoClose(in);          
       }
       PAudio at3 = malloc(sizeof(Audio));
       if (!at3){
          printf("at3 memory(at3)\n");
          if (data)
             free(data);
          sceAtracReleaseAtracID(ID);          
          if (stream)
             free(cnk.data);
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          return NULL;
       }
       at3->type = 1;
       at3->channels = 2;//f.channels; //me always returns 2 channels
       at3->frequency = f.frequency;
       at3->bitRate = 16;
       at3->data = data;
       at3->size = stream?totalSamples:samples;
       at3->scale = (float)f.frequency/(float)getsysFrequency();
       at3->offset = 0;
       at3->frac = 0;
       at3->stream = stream;
       at3->channel = 0x80026008;
       at3->sampleCount = stream?maxSamples:PSP_NUM_AUDIO_SAMPLES; 
       at3->mixBuffer = NULL; 
       at3->lVolume = PSP_VOLUME_MAX;
       at3->rVolume = PSP_VOLUME_MAX;
       at3->play = STOP;
       at3->loop = 0;
       at3->ufunc = NULL;
       at3->flags = 0;
       at3->extra = NULL;
       if (stream){
          if (f.id == 0x270)
             at3->type = SceAtrac3Codec;
          else
             at3->type = SceAtrac3plusCodec;
          if (stream == STREAM_FROM_MEMORY){
             at3->file = size;
             at3->fstate = STATE_F_OTHER;
          }
          else{
             at3->file = in;
             at3->fstate = STATE_F_OPEN;
             at3->flags = AUDIO_FLAG_SCEUID;
          }
          at3->stream = stream;
          at3->filename = malloc(strlen(file)+1);
          if (!strrchr(file, ':')){
             char path[256];
             getcwd(path, 256);
             at3->filename = malloc(strlen(path)+strlen(file)+2);
             if (at3->filename){
                strcat(strcat(strcpy(at3->filename, path), "/"), file);
             }
             else{
                printf("Error memory\n");
                audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
             }
          }
          else{ 
             at3->filename = malloc(strlen(file)+1);
             if (at3->filename){
                strcpy(at3->filename, file);
             }
             else{
                printf("Error memory\n");
                audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
             }
          }
          at3->data = malloc(maxSamples*at3->channels*2*5);
          if (!at3->data){
             printf("at3 memory(at3->data)\n");
             if (data)
                free(data);
             free(at3);
             sceAtracReleaseAtracID(ID);
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
             return NULL;
          }
          at3_extra *extra = malloc(sizeof(at3_extra));
          at3->extra = (audioExtra*)extra;
          extra->size = sizeof(at3_extra);
          extra->ID = ID;
          extra->working = 0;
          extra->frequency = at3->frequency;
          if (stream == STREAM_FROM_MEMORY) extra->data = data;
          extra->stream = (StreamEventHandler) at3_EventHandler;
          extra->thid = sceKernelCreateThread("at3_stream", at3_thread, 0x10, 16*0x400, PSP_THREAD_ATTR_USER, NULL);
          if (extra->thid < 0){
             printf("at3 create thread\n");
             sceAtracReleaseAtracID(ID);
             if (extra->data)
                free(extra->data);
             free(extra);
             free(at3->data);
             free(at3);
             audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
             return NULL;
          }
          extra->eof = 0;
          sceKernelStartThread(at3->extra->thid, 4, &at3);
       }
       register_audio(at3);
       return at3;  
}
PAudio loadAa3(const char *filename){
       if (!filename) return NULL;
       SceUID aa3_handle = -1; 
       u16 aa3_type; 
       u8* aa3_data_buffer = NULL; 
       u16 aa3_data_align; 
       u32 aa3_sample_per_frame; 
       u16 aa3_channel_mode; 
       u8 aa3_at3plus_flagdata[2]; 
       u32 aa3_data_start; 
       u32 aa3_data_size; 
       u8 aa3_getEDRAM; 
       u32 aa3_channels; 

       unsigned long *aa3_codec_buffer = NULL;
       PAudio aa3 = NULL;
       aa3_extra* extra = NULL;

       if (!loadAudioCodec(PSP_AV_MODULE_ATRAC3PLUS)){
          printf("aa3 load av module avcodec \n");
          goto error;
       }

       aa3_handle = sceIoOpen(filename, PSP_O_RDONLY, 0777); 
       if (aa3_handle < 0){
          audiosetLastError(AUDIO_ERROR_OPEN_FILE);
          printf("aa3 open file %s 0x%08x\n", filename, aa3_handle);
          goto error;
       }
       char id3_buf[10];
       if (sceIoRead(aa3_handle, id3_buf, 10) != 10){ 
          sceIoClose(aa3_handle);
          audiosetLastError(AUDIO_ERROR_FILE_READ);
          printf("aa3 read file\n");
          goto error;
       }

       int id3_size = getID3v2TagSize(id3_buf);
       if (sceIoLseek32(aa3_handle, id3_size, PSP_SEEK_SET) < 0)
          goto error;
 
       aa3_channels = 2; //it will be decoded as stereo, so... 
    
       u8 ea3_header[0x60]; 
       if (sceIoRead(aa3_handle, ea3_header, 0x60) != 0x60) 
          goto error; 
       if (ea3_header[0] != 0x45 || ea3_header[1] != 0x41 || ea3_header[2] != 0x33) 
          goto error;

       aa3_at3plus_flagdata[0] = ea3_header[0x22]; 
       aa3_at3plus_flagdata[1] = ea3_header[0x23]; 
    
       aa3_type = (ea3_header[0x22] == 0x20) ? SceAt3Codec : ((ea3_header[0x22] == 0x28) ? SceAt3PCodec : 0x0); 
    
       if (aa3_type == 0) 
          goto error; 
 
       if (aa3_type == SceAt3Codec) 
          aa3_data_align = ea3_header[0x23]*8; 
       else 
          aa3_data_align = (ea3_header[0x23]+1)*8; 
    
       aa3_data_start = id3_size+0x60; 
       aa3_data_size = sceIoLseek32(aa3_handle, 0, PSP_SEEK_END) - aa3_data_start; 
    
       int frames = aa3_data_size/aa3_data_align;
       if (frames < 1)
          goto error;
       int broken = 0;
       if (aa3_data_size % aa3_data_align != 0) 
          broken = 1;
    
       sceIoLseek32(aa3_handle, aa3_data_start, PSP_SEEK_SET); 
    
       aa3_codec_buffer = memalign(64, 65*sizeof(unsigned int));
       if (!aa3_codec_buffer)
          goto error;
       memset(aa3_codec_buffer, 0, 65*sizeof(unsigned int)); 
    
       if (aa3_type == SceAt3Codec){ 
          aa3_channel_mode = 0x0; 
          if (aa3_data_align == 0xC0) // atract3 have 3 bitrate, 132k,105k,66k, 132k align=0x180, 105k align = 0x130, 66k align = 0xc0 
             aa3_channel_mode = 0x1; 
          aa3_sample_per_frame = 1024; 
          aa3_data_buffer = (u8*)memalign(64, 0x180); 
          if (aa3_data_buffer == NULL) 
             goto error; 
          aa3_codec_buffer[26] = 0x20; 
          if (sceAudiocodecCheckNeedMem(aa3_codec_buffer, aa3_type) < 0) 
             goto error; 
          if (sceAudiocodecGetEDRAM(aa3_codec_buffer, aa3_type) < 0) 
             goto error; 
          aa3_getEDRAM = 1; 
          aa3_codec_buffer[10] = 4; 
          aa3_codec_buffer[44] = 2; 
          if (aa3_data_align == 0x130) 
             aa3_codec_buffer[10] = 6; 
          if (sceAudiocodecInit(aa3_codec_buffer, aa3_type) < 0){ 
             goto error; 
          } 
       } 
       else if (aa3_type == SceAt3PCodec){ 
          aa3_sample_per_frame = 2048; 
          int temp_size = aa3_data_align+8; 
          int mod_64 = temp_size & 0x3f; 
          if (mod_64 != 0) temp_size += 64 - mod_64; 
          aa3_data_buffer = (u8*)memalign(64, temp_size); 
          if ( aa3_data_buffer == NULL) 
             goto error;
          memset(aa3_data_buffer, 0, temp_size);
          aa3_codec_buffer[5] = 0x1; 
          aa3_codec_buffer[10] = aa3_at3plus_flagdata[1]; 
          aa3_codec_buffer[10] = (aa3_codec_buffer[10] << 8 ) | aa3_at3plus_flagdata[0]; 
          aa3_codec_buffer[12] = 0x1; 
          aa3_codec_buffer[14] = 0x1; 
          if (sceAudiocodecCheckNeedMem(aa3_codec_buffer, aa3_type) < 0) 
             goto error; 
          if (sceAudiocodecGetEDRAM(aa3_codec_buffer, aa3_type) < 0) 
             goto error;
          aa3_getEDRAM = 1; 
          if (sceAudiocodecInit(aa3_codec_buffer, aa3_type) < 0){ 
             goto error; 
          } 
          aa3_data_buffer[0] = 0x0F; 
          aa3_data_buffer[1] = 0xD0; 
          aa3_data_buffer[2] = aa3_at3plus_flagdata[0]; 
          aa3_data_buffer[3] = aa3_at3plus_flagdata[1]; 
       } 

       aa3 = malloc(sizeof(Audio));
       if (!aa3){
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          goto error;
       }
       aa3->type = aa3_type;
       aa3->channels = aa3_channels;
       aa3->frequency = 44100;
       aa3->bitRate = 16;
       aa3->data = NULL;
       aa3->sampleCount = aa3_sample_per_frame;
       aa3->mixBuffer = NULL;
       aa3->size = frames*aa3_sample_per_frame;
       aa3->scale = (float)aa3->frequency/(float)getsysFrequency();
       aa3->offset = 0;
       aa3->frac = 0;
       aa3->stream = 1;
       aa3->channel = -1;
       aa3->lVolume = PSP_VOLUME_MAX;
       aa3->rVolume = PSP_VOLUME_MAX;
       aa3->play = STOP;
       aa3->loop = 0;
       aa3->file = aa3_handle;
       aa3->fstate = STATE_F_OPEN;
       aa3->ufunc = NULL;       
       aa3->flags = AUDIO_FLAG_SCEUID;
       extra = malloc(sizeof(aa3_extra));
       if (!extra){
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          goto error;
       }
       aa3->extra = (audioExtra*)extra;
       extra->size = sizeof(aa3_extra);
       extra->aa3_codec_buffer = aa3_codec_buffer;
       extra->frequency = aa3->frequency;
       extra->stream = (StreamEventHandler) aa3_EventHandler;
       extra->fill = 0;       
       extra->eof = 0;
       extra->data = aa3_data_buffer;
       extra->aa3_data_align = aa3_data_align;
       extra->start = aa3_data_start;
       aa3->data = memalign(64, aa3->sampleCount*2*2*5);
       if (!aa3->data){
          audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          goto error;
       }
       memset(aa3->data, 0, aa3->sampleCount*2*2*5);
       if (!strrchr(filename, ':')){
          char path[256];
          getcwd(path, 256);
          aa3->filename = malloc(strlen(path)+strlen(filename)+2);
          if (aa3->filename){
             strcat(strcat(strcpy(aa3->filename, path), "/"), filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       else{ 
          aa3->filename = malloc(strlen(filename)+1);
          if (aa3->filename){
             strcpy(aa3->filename, filename);
          }
          else{
             printf("Error memory\n");
             audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
          }
       }
       extra->thid = sceKernelCreateThread("aa3_stream", aa3_thread, 0x10, 4*0x400, PSP_THREAD_ATTR_USER, NULL);
       if (extra->thid < 0){
          printf("aa3 create thread\n");
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          goto error;
       }
       sceKernelStartThread(extra->thid, 4, &aa3);
       register_audio(aa3);
       return aa3; 
       error:
            printf("aa3 load %s\n", filename);
            if (aa3_handle >= 0)
               sceIoClose(aa3_handle); 
            if (aa3_codec_buffer){
               if (aa3_getEDRAM == 1)
                  sceAudiocodecReleaseEDRAM(aa3_codec_buffer);
               if (aa3_data_buffer)
                  free(aa3_data_buffer);
               if (aa3){ 
                  if (extra){ 
                     free(extra);   
                  }
                  if (aa3->data){ 
                     free(aa3->data);
                  }
                  free(aa3);
               }
            }
            return NULL;
}

int At3ToWav(const char *at3file, const char *wavfile){
       if (!at3file || !wavfile) return 0;

       SceUID in = sceIoOpen(at3file, PSP_O_RDONLY, 0777);
       if (in < 0){
          printf("Error file %d\n", in);
          return 0;
       }
       SceUID out = sceIoOpen(wavfile, PSP_O_WRONLY|PSP_O_CREAT, 0777);
       if (out < 0){
          printf("Error file %d\n", out);
          sceIoClose(in);
          return 0;
       }

       RIFFHeader head; 
       sceIoRead(in, &head, 12);
       if (head.fsig != RIFF){
          printf("Error RIFF\n");
          sceIoClose(in);
          return 0;
       } 
       if (head.fmt != WAVE){
          printf("Error WAVE\n");
          sceIoClose(in);
          return 0;
       }

       chunk cnk;
       fmt f;
       while (sceIoRead(in, &cnk, 8) == 8){
             if (cnk.csig == fmt__){
                getFormat(&f, in, cnk.dsize);
                break;
             }
             sceIoLseek32(in, cnk.dsize, PSP_SEEK_CUR);
       }
       int fsize;
       fsize = sceIoLseek32(in, 0, PSP_SEEK_END);
       cnk.data = memalign(64, fsize);
       if (!cnk.data){
          printf("Error memory\n");
          sceIoClose(in);
          return 0;
       }
       int ret;
       sceIoLseek32(in, 0, PSP_SEEK_SET);
       ret = sceIoRead(in, cnk.data, fsize);
       if (ret != fsize){
          printf("Error read 0x%08x\n", ret);
          sceIoClose(in);
          free(cnk.data);
          return 0;
       }
       if (!loadAudioCodec(PSP_AV_MODULE_ATRAC3PLUS)){
          printf("Error AVCODEC \n");
          sceIoClose(in);
          free(cnk.data);
          return 0;
       }
       int ID = sceAtracSetDataAndGetID(cnk.data, fsize);
       if (ID < 0){
          printf("Error sceAtracSetDataAndGetID 0x%08x\n", ID);
          free(cnk.data);
          sceIoClose(in);
          return 0;
       }
       ret = sceAtracSetLoopNum(ID, 100);//huh?
       if (ret < 0){
          printf("Error sceAtracSetLoopNum 0x%08x\n", ret);
       }
       
       char wave[21] = "RIFF    WAVEfmt \020\0\0\0";
       sceIoWrite(out, wave, 20);
       fmt g = {1, 2, 44100, 44100*4, 4, 16};
       sceIoWrite(out, &g, sizeof(g));
       sceIoWrite(out, "data    ", 8);
       
       int data[2048];
       int samples = 0;
       int outEnd;
       do{
          int outN, outRemainFrame;
          ret = sceAtracDecodeData(ID, (u16*)data, &outN, &outEnd, &outRemainFrame);//shouldn't it be (s16*)
          if (ret < 0){
             printf("sceAtracDecodeData 0x%08x\n", ret);
             break;
          }
          samples += outN;
          sceIoWrite(out, data, outN*2*2);

       }
       while (!outEnd);
       sceIoLseek32(out, 40, PSP_SEEK_SET);
       samples *= 4;
       sceIoWrite(out, &samples, 4);
       sceIoLseek32(out, 4, PSP_SEEK_SET);
       samples += 32;
       sceIoWrite(out, &samples, 4);
       
       ret = sceAtracReleaseAtracID(ID);
       if (ret < 0){
          printf("Error sceAtracReleaseAtracID 0x%08x\n", ret);
       }
       free(cnk.data);
       sceIoClose(in);
       sceIoClose(out);
       return 1;
}  
int Aa3ToWav(const char *aa3file,const char *wavfile){
    if (!aa3file || !wavfile) return 0;

    u16 aa3_type; 
    u8* aa3_data_buffer = NULL; 
    s16* aa3_mix_buffer = NULL; 
    u16 aa3_data_align; 
    u32 aa3_sample_per_frame; 
    u16 aa3_channel_mode; 
    u8 aa3_at3plus_flagdata[2]; 
    u32 aa3_data_start; 
    u32 aa3_data_size; 
    u8 aa3_getEDRAM = 0; 
    u32 aa3_channels; 

    unsigned long *aa3_codec_buffer = NULL;

    SceUID in = sceIoOpen(aa3file, PSP_O_RDONLY, 0777);
    if (in < 0){
       printf("Error file %d\n", in);
       return 0;
    }
    SceUID out = sceIoOpen(wavfile, PSP_O_WRONLY|PSP_O_CREAT, 0777);
    if (out < 0){
       printf("Error file %d\n", out);
       sceIoClose(in);
       return 0;
    }

    char id3_buf[10];
    if (sceIoRead(in, id3_buf, 10) != 10){ 
       sceIoClose(in);
       audiosetLastError(AUDIO_ERROR_FILE_READ);
       printf("aa3 read file\n");
       goto error;
    }

    int id3_size = getID3v2TagSize(id3_buf);
    if (sceIoLseek32(in, id3_size, PSP_SEEK_SET) < 0)
       goto error;
 
    aa3_channels = 2; //it will be decoded as stereo, so... 
    
    u8 ea3_header[0x60]; 
    if (sceIoRead(in, ea3_header, 0x60) != 0x60) 
       goto error; 
    if (ea3_header[0] != 0x45 || ea3_header[1] != 0x41 || ea3_header[2] != 0x33) 
       goto error;

    aa3_at3plus_flagdata[0] = ea3_header[0x22]; 
    aa3_at3plus_flagdata[1] = ea3_header[0x23]; 
    
    aa3_type = (ea3_header[0x22] == 0x20) ? SceAt3Codec : ((ea3_header[0x22] == 0x28) ? SceAt3PCodec : 0x0); 
    
    if (aa3_type == 0) 
       goto error; 
 
    if (aa3_type == SceAt3Codec) 
       aa3_data_align = ea3_header[0x23]*8; 
    else 
       aa3_data_align = (ea3_header[0x23]+1)*8; 
    
    aa3_data_start = id3_size+0x60; 
    aa3_data_size = sceIoLseek32(in, 0, PSP_SEEK_END) - aa3_data_start; 
    
    int frames = aa3_data_size/aa3_data_align;
    if (frames < 1)
       goto error;
    int broken = 0;
    if (aa3_data_size % aa3_data_align != 0) 
       broken = 1;
    
    sceIoLseek32(in, aa3_data_start, PSP_SEEK_SET); 
    
    if (!loadAudioCodec(PSP_AV_MODULE_ATRAC3PLUS)){
       printf("aa3 load av module avcodec \n");
       goto error;
    }

    aa3_codec_buffer = memalign(64, 65*sizeof(unsigned int));
    if (!aa3_codec_buffer)
       goto error;
    memset(aa3_codec_buffer, 0, 65*sizeof(unsigned int)); 
    
    if (aa3_type == SceAt3Codec){ 
       aa3_channel_mode = 0x0; 
       if (aa3_data_align == 0xC0) // atract3 have 3 bitrate, 132k,105k,66k, 132k align=0x180, 105k align = 0x130, 66k align = 0xc0 
          aa3_channel_mode = 0x1; 
       aa3_sample_per_frame = 1024; 
       aa3_data_buffer = (u8*)memalign(64, 0x180); 
       if (aa3_data_buffer == NULL) 
          goto error; 
       aa3_codec_buffer[26] = 0x20; 
       if (sceAudiocodecCheckNeedMem(aa3_codec_buffer, aa3_type) < 0) 
          goto error; 
       if (sceAudiocodecGetEDRAM(aa3_codec_buffer, aa3_type) < 0) 
          goto error; 
       aa3_getEDRAM = 1; 
       aa3_codec_buffer[10] = 4; 
       aa3_codec_buffer[44] = 2; 
       if (aa3_data_align == 0x130) 
          aa3_codec_buffer[10] = 6; 
       if (sceAudiocodecInit(aa3_codec_buffer, aa3_type) < 0){ 
          goto error; 
       } 
    } 
    else if (aa3_type == SceAt3PCodec){ 
       aa3_sample_per_frame = 2048; 
       int temp_size = aa3_data_align+8; 
       int mod_64 = temp_size & 0x3f; 
       if (mod_64 != 0) temp_size += 64 - mod_64; 
       aa3_data_buffer = (u8*)memalign(64, temp_size); 
       if ( aa3_data_buffer == NULL) 
          goto error;
       memset(aa3_data_buffer, 0, temp_size);
       aa3_codec_buffer[5] = 0x1; 
       aa3_codec_buffer[10] = aa3_at3plus_flagdata[1]; 
       aa3_codec_buffer[10] = (aa3_codec_buffer[10] << 8 ) | aa3_at3plus_flagdata[0]; 
       aa3_codec_buffer[12] = 0x1; 
       aa3_codec_buffer[14] = 0x1; 
       if (sceAudiocodecCheckNeedMem(aa3_codec_buffer, aa3_type) < 0) 
          goto error; 
       if (sceAudiocodecGetEDRAM(aa3_codec_buffer, aa3_type) < 0) 
          goto error;
       aa3_getEDRAM = 1; 
       if (sceAudiocodecInit(aa3_codec_buffer, aa3_type) < 0){ 
          goto error; 
       } 
       aa3_data_buffer[0] = 0x0F; 
       aa3_data_buffer[1] = 0xD0; 
       aa3_data_buffer[2] = aa3_at3plus_flagdata[0]; 
       aa3_data_buffer[3] = aa3_at3plus_flagdata[1]; 
    } 
    aa3_mix_buffer = memalign(64, aa3_sample_per_frame*2*2);
    if (!aa3_mix_buffer){
       audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
       goto error;
    }
    memset(aa3_mix_buffer, 0, aa3_sample_per_frame*2*2);
    
    aa3_extra extra;
    Audio aa3;
    aa3.type = aa3_type;
    aa3.sampleCount = aa3_sample_per_frame;
    aa3.file = in;
    aa3.fstate = STATE_F_OPEN;
    aa3.ufunc = NULL;    
    aa3.flags = AUDIO_FLAG_SCEUID;
    aa3.extra = (audioExtra*)&extra;
    extra.aa3_codec_buffer = aa3_codec_buffer;
    extra.data = aa3_data_buffer;
    extra.aa3_data_align = aa3_data_align;
    
    char wave[21] = "RIFF    WAVEfmt \020\0\0\0";
    sceIoWrite(out, wave, 20);
    fmt g = {1, 2, 44100, 44100*4, 4, 16};
    sceIoWrite(out, &g, sizeof(g));
    sceIoWrite(out, "data    ", 8);
    int samples = 0, outN;
    while ((outN = decodeAa3Frame(&aa3, aa3_mix_buffer)) > 0){
          sceIoWrite(out, aa3_mix_buffer, outN*2*2);
          samples += outN;
    }
    sceIoLseek32(out, 40, PSP_SEEK_SET);
    samples *= 4;
    sceIoWrite(out, &samples, 4);
    sceIoLseek32(out, 4, PSP_SEEK_SET);
    samples += 32;
    sceIoWrite(out, &samples, 4);

    sceAudiocodecReleaseEDRAM(aa3_codec_buffer);
    free(aa3_codec_buffer);
    free(aa3_data_buffer);
    free(aa3_mix_buffer);
    sceIoClose(in);
    sceIoClose(out);
    return 1;
    error:
          printf("aa3towav failed\n");
          sceIoClose(in);
          sceIoClose(out);
          if (aa3_getEDRAM) sceAudiocodecReleaseEDRAM(aa3_codec_buffer);
          if (aa3_codec_buffer) free(aa3_codec_buffer);
          if (aa3_data_buffer) free(aa3_data_buffer);
          if (aa3_mix_buffer) free(aa3_mix_buffer);
          return 0;
}
