/*
 * AudioMAD.c
 *
 * MUSAUDIOMAD Version 0 by mowglisanu
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
#ifndef SANE
#include <mad.h>
#include "AudioMPEG.h"

extern void register_audio(PAudio );
extern void unregister_audio(PAudio );
extern int getMp3Size(const char *, int );
extern int estimateMp3Size(char *, int );
extern int mpegsetLastError(int );
extern int seekNextFrame(SceUID, int );
extern int seekNextFrameFILE(FILE*, int );
extern int seekNextFrameMEMORY(void*, int, int );
extern int getMp3Sizefd(SceUID, int );
extern int getMp3SizeFILE(FILE*, int );
extern int getMp3SizeMEMORY(void*, int, int );

static int decode(PAudio mpeg, int second);

int reset_mad(PAudio mpeg){
    mp3_extra *extra = (mp3_extra*)mpeg->extra;
    if (mpeg->flags&AUDIO_FLAG_SCEUID)
       sceIoLseek32(mpeg->file, extra->start, PSP_SEEK_SET);
#ifndef USE_ONLY_SCE_IO
    else if (mpeg->flags&AUDIO_FLAG_FILE)
       fseek((FILE*)mpeg->file, extra->start, PSP_SEEK_SET);
#endif 
    mpeg->offset = 0;
    extra->eof = 0;
    extra->fill = 0;
    extra->position = 0;
    return 0;
}
int mad_EventHandler(PAudio mpeg, eventData *event){
    if (!mpeg) return 1;
    mp3_extra *extra = (mp3_extra*)mpeg->extra;
//    int ret;
    switch (event->event){
       case EVENT_stopAudio :
          mpeg->play = STOP;
          while (extra->working) sceKernelDelayThread(5);
          reset_mad(mpeg);
          return 0;
       break;
       case EVENT_freeAudio : 
          mpeg->play = TERMINATED;
          sceKernelWaitThreadEnd(extra->thid, 0);
          if (mpeg->flags&AUDIO_FLAG_SCEUID)
             sceIoClose(mpeg->file);
#ifndef USE_ONLY_SCE_IO
          else if (mpeg->flags&AUDIO_FLAG_FILE)
             fclose((FILE*)mpeg->file);
#endif             
          if ((mpeg->flags&(AUDIO_FLAG_MEMORY)&&(mpeg->flags&AUDIO_FLAG_FREE)) || (mpeg->flags&(AUDIO_FLAG_SCEUID
#ifndef USE_ONLY_SCE_IO
          |AUDIO_FLAG_FILE
#endif
          )))
             free(extra->data);
          free(extra);
          free(mpeg->data);
          unregister_audio(mpeg);
          free(mpeg);
       break;
       case EVENT_getBitrateAudio : 
          return getBitrateMpeg(mpeg);  
       break;
       case EVENT_seekAudio : 
          //crap I forgot to do a seek, or maybe I didn't feel like it.
       break;
       default :
          return 0;
    }
    return 1;
}

int mad_thread(SceSize args, void *argp){
    PAudio mpeg = *(PAudio*)argp;
    if (!mpeg) return -1;
    while (1){
          if (mpeg->play != PLAYING){
             if (mpeg->play == TERMINATED) break;
             sceKernelDelayThread(50);
             continue;
          }
          decode(mpeg, 0);
          reset_mad(mpeg);
    }
    sceKernelExitDeleteThread(0);
    return 0xdeadbeef;
}

PAudio loadMADMpeg(void *file, int stream, int size, u32 sztype, u32 flags){
       if (!file || (flags&AUDIO_FLAG_MEMORY && size == 0))
          return NULL;
       u32 hMask;
       int dataSize = 0;
       int start = 0;
       int totalSamples = 0;
       PAudio mpeg = NULL;
       mp3_extra *extra = NULL;
       
       SceUID fd  = -1;
       FILE* in = NULL;
       char *filename = NULL;
       void *data = NULL;
       
       if (!(flags&(AUDIO_FLAG_SCEUID|
#ifndef USE_ONLY_SCE_IO
       AUDIO_FLAG_FILE|
#endif
       AUDIO_FLAG_MEMORY))){
          if (MPEG_FLAG_NO_ESTIMATE&flags)
             totalSamples = getMp3Size(file, SZ_SAMPLES);
          else
             totalSamples = estimateMp3Size(file, SZ_SAMPLES);
          if (totalSamples <= 0){
             mpegsetLastError(MPEG_ERROR_INVALID_FILE);
             return NULL;
          }
          filename = file;
          fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
          if (fd < 0){
             mpegsetLastError(MPEG_ERROR_OPEN_FILE);
             return NULL;   
          }
          if (stream == STREAM_FROM_MEMORY){
             data = malloc(size);
             if (!data){
                mpegsetLastError(MPEG_ERROR_MEM_ALLOC);
                sceIoClose(fd);
                return NULL;
             }
             int read = sceIoRead(fd, data, size);
             sceIoClose(fd);
             if (read < size){
                mpegsetLastError(MPEG_ERROR_FILE_READ);
                free(data);
                return NULL;
             }
             start = 0;
             dataSize = read;
             flags |= AUDIO_FLAG_MEMORY|AUDIO_FLAG_FREE;
          }
          else{
             flags |= AUDIO_FLAG_SCEUID;          
          }
       }
       else{
          if (flags&AUDIO_FLAG_SCEUID){
             fd = (u32)file;
          }
          else if (flags&AUDIO_FLAG_FILE){
             in = file;
          }
          else if (flags&AUDIO_FLAG_MEMORY){
             data = file;  
          }
       }
       
       if (flags&AUDIO_FLAG_SCEUID){
          start = seekNextFrame(fd, 0);
          if (start < 0){
             mpegsetLastError(MPEG_ERROR_INVALID_FILE);
             return NULL;
          }
          start = sceIoLseek32(fd, 0, PSP_SEEK_CUR);
          if (!totalSamples)
             if (MPEG_FLAG_NO_ESTIMATE&flags)
                totalSamples = getMp3Sizefd(fd, SZ_SAMPLES);
          if (sceIoRead(fd, &hMask, 4) != 4){
             sceIoClose(fd); 
             mpegsetLastError(MPEG_ERROR_FILE_READ);
             return NULL;
          }
          if (stream != STREAM_FROM_MEMORY){
             dataSize = sceIoLseek32(fd, 0, PSP_SEEK_END) - start;
          }
          sceIoLseek32(fd, start, PSP_SEEK_SET);
       }
#ifndef USE_ONLY_SCE_IO
       else if (flags&AUDIO_FLAG_FILE){
          start = seekNextFrameFILE(in, 0);
          if (start < 0){
             mpegsetLastError(MPEG_ERROR_INVALID_FILE);
             return NULL;
          }
          start = ftell(in);
          if (MPEG_FLAG_NO_ESTIMATE&flags)
             totalSamples = getMp3SizeFILE(file, SZ_SAMPLES);
          if (fread(&hMask, 1, 4, in)<=0){
             fclose(in);
             mpegsetLastError(MPEG_ERROR_FILE_READ);
             return NULL;
          }
          if (stream != STREAM_FROM_MEMORY){
             fseek(in, 0, SEEK_END);
             dataSize = ftell(in) - start;
          }
          fseek(in, start, SEEK_SET);
       }
#endif
       else if (flags&AUDIO_FLAG_MEMORY){
          if (!dataSize) dataSize = size;
          if (MPEG_FLAG_NO_ESTIMATE&flags)
             totalSamples = getMp3SizeMEMORY(data, SZ_SAMPLES, dataSize);
          start += seekNextFrameMEMORY(data+start, 0, dataSize);
          if (start < 0){
             printf("NO SYNC FRAME FOUND\n");
             mpegsetLastError(MPEG_ERROR_INVALID_FILE);
             goto error;
          }
          dataSize -= start;
          hMask = ((u8*)(data+start))[0];
          hMask = hMask|((u8*)(data+start))[1]<<8;
          hMask = hMask|((u8*)(data+start))[2]<<16;
          hMask = hMask|((u8*)(data+start))[3]<<24;
       }
       u32 samples_per_frame; 
       u32 channels; 
       int type = getType(hMask);
       int layer = getLayer(hMask);
       int frequency = mpeg_frequencies[type][getFrequency(hMask)];
       channels = getChannelCount(hMask);
       
       int frameScale;//obsolito
       if (layer == MPEG_LAYER_1)
          frameScale = 12*1000;
       else if (((layer == MPEG_LAYER_3) && (type == MPEG_VERSION_2)) || (type == MPEG_VERSION_2_5))
          frameScale = 72*1000;
       else
          frameScale = 144*1000;
       int index;
       if (type == MPEG_VERSION_1)
          index = 3 - layer;
       else{
          if (layer == MPEG_LAYER_1) index = 3;
          else index = 4;
       }
       samples_per_frame = mpeg_samples[type][layer];
       if (!samples_per_frame  || !layer || type == MPEG_VERSION_RESERVED){
             printf("NO SYNC FRAME FOUND\n");
             mpegsetLastError(MPEG_ERROR_INVALID_FILE);
             goto error;
       }

//       hMask = hMask & 0xc00cfeff;//my mask

       if (!stream){
          if (sztype == SZ_BYTES)
             size = size/(2*channels);
//          if (sztype == SZ_ALL)//is sz all 0?
//             size = totalSamples;
          if (sztype == SZ_SECONDS)
             size = size*frequency;
          size = (size/samples_per_frame)*samples_per_frame;
          if (size == 0){
          if (flags&AUDIO_FLAG_SCEUID)
             size = getMp3Sizefd(fd, SZ_SAMPLES);
#ifndef USE_ONLY_SCE_IO          
          if (flags&AUDIO_FLAG_FILE)
             size = getMp3SizeFILE(in, SZ_SAMPLES);
#endif
          if (flags&AUDIO_FLAG_MEMORY)
             size = getMp3SizeMEMORY(data, SZ_SAMPLES, dataSize);
          }
       }
       
       mpeg = malloc(sizeof(Audio));
       if (!mpeg){
          mpegsetLastError(MPEG_ERROR_MEM_ALLOC);
          goto error;
       }
       extra = malloc(sizeof(mp3_extra));
       if (!extra){
          printf("malloc failed on extra\n");
          mpegsetLastError(MPEG_ERROR_MEM_ALLOC);
          goto error;
       }
       mpeg->extra = (audioExtra*)extra;
//       mpeg->file = (u32)file;
       mpeg->type = MADMpegCodec;
       mpeg->sampleCount = 1024;
       mpeg->mixBuffer = NULL;
       mpeg->data = NULL;
       mpeg->size = totalSamples;
       mpeg->offset = 0;
       mpeg->frac = 0;
       mpeg->stream = stream;
       mpeg->channels = channels;
       mpeg->bitRate = 16;
       mpeg->frequency = frequency;
       mpeg->scale = (float)frequency/(float)getsysFrequency();
       mpeg->channel = 0x80026008;
       mpeg->lVolume = PSP_AUDIO_VOLUME_MAX;
       mpeg->rVolume = PSP_AUDIO_VOLUME_MAX;
       mpeg->play = STOP;
       mpeg->loop = 0;
       mpeg->fstate = STATE_F_CLOSE;
       mpeg->ufunc = NULL;
       mpeg->flags = flags;
       extra->size = sizeof(mp3_extra);
       extra->stream = (StreamEventHandler)mad_EventHandler;
       extra->eof = 0;
       extra->fill = 0;
       extra->start = start;
       extra->position = 0;
       extra->working = 0;
       if ((flags&(AUDIO_FLAG_SCEUID
#ifndef USE_ONLY_SCE_IO
       |AUDIO_FLAG_FILE
#endif
       ))){
          if (fd >= 0) 
             mpeg->file = fd;
          else if (in)
             mpeg->file = (u32)in;
          extra->data = malloc(8*1024);
          if (!extra->data){
             printf("malloc failed on extra->data\n");
             goto error;
          }
       }  
       else{
          extra->data = data;
       }
       extra->type = type;
       extra->layer = layer;
       extra->frequency = frequency;
       extra->channels = channels;
       extra->index = index;
       extra->index = 0;
       extra->dataSize = dataSize;
       extra->mask = hMask&0xc00cfeff;
       if (stream){
          mpeg->data = malloc(mpeg->sampleCount*mpeg->channels*2*4);
          if (!mpeg->data){
             printf("malloc failed on mpeg->data\n");
             goto error;
          }
          extra->bitrate = mpeg_bitrates[index][getBitrate(hMask)]*1000;
          extra->thid = sceKernelCreateThread("mad_stream", mad_thread, 0x10, 32*0x400, PSP_THREAD_ATTR_USER, NULL);
          if (extra->thid < 0){
             printf("Error thread creation \"mad_stream\"\n");
             mpegsetLastError(MPEG_ERROR_THREAD_FAIL);
             goto error;
          }
          if (filename){
             if (!strrchr(filename, ':')){
                char path[256];
                getcwd(path, 256);
                mpeg->filename = malloc(strlen(path)+strlen(filename)+2);
                if (mpeg->filename){
                   strcat(strcat(strcpy(mpeg->filename, path), "/"), filename);
                }
                else{
                   printf("Error memory\n");
                   audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
                }
             }
             else{ 
                mpeg->filename = malloc(strlen(filename)+1);
                if (mpeg->filename){
                   strcpy(mpeg->filename, filename);
                }
                else{
                   printf("Error memory\n");
                   audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
                }
             }
          }
          sceKernelStartThread(mpeg->extra->thid, 4, &mpeg);
       }
       else{
          mpeg->data = malloc(size*4);
          if (!mpeg->data){
             printf("malloc failed on mpeg->data\n");
             goto error;
          }
          mpeg->size = size;
          decode(mpeg, 1);
          mpeg->type = 1;
          mpeg->size = mpeg->offset;
          mpeg->offset = 0;
          if (flags&AUDIO_FLAG_SCEUID)
             sceIoClose(fd);
#ifndef USE_ONLY_SCE_IO
          else if (flags&AUDIO_FLAG_FILE)
             fclose(in);
#endif             
          free(extra->data);
          free(extra);
          mpeg->flags = 0;
       }
       register_audio(mpeg);
       return mpeg;
error:
      printf("mad error\n");
      if (flags&AUDIO_FLAG_FREE){
         if (flags&AUDIO_FLAG_SCEUID)
            sceIoClose((SceUID)file);
#ifndef USE_ONLY_SCE_IO
         else if (flags&AUDIO_FLAG_FILE)
           fclose((FILE*)file);
#endif           
         if (flags&AUDIO_FLAG_MEMORY)
           free(file);
      }
      if (extra){
         if (flags&AUDIO_FLAG_MEMORY)
           free(extra->data);
         free(extra);
      }
      if (mpeg){
         free(mpeg);
      }
      return NULL;
}

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *pdata,
                    struct mad_stream *stream)
{
      
  PAudio mpeg = (PAudio)pdata;
  mp3_extra *extra = (mp3_extra*)mpeg->extra;
 
  if (extra->eof){
     if (mpeg->stream){
        if (!mpeg->loop){
           if (extra->fill){
              short *out = (short*)mpeg->data;
              memset(out+extra->index*mpeg->sampleCount*mpeg->channels+extra->fill, 0, (mpeg->sampleCount-extra->fill)*mpeg->channels*2);
              sceAudioOutputPannedBlocking(mpeg->channel, mpeg->lVolume, mpeg->rVolume, out+extra->index*mpeg->sampleCount*mpeg->channels);
              mpeg->offset += extra->fill;
           }
           mpeg->play = STOP;
        }
        if (!mpeg->size) mpeg->size = mpeg->offset;
     }
     mad_stream_buffer(stream, extra->data, 0);
     return MAD_FLOW_STOP;
  }

  void *data;
  int size = 8*1024;
  
  if (mpeg->flags&AUDIO_FLAG_MEMORY){
     if (stream->this_frame){
        data = (void*)stream->this_frame;
        extra->position -= extra->data+extra->start+extra->position - data;
     }
     else{
        data = extra->data+extra->start+extra->position;
     }
     if (extra->position+size >= extra->dataSize){
        size = extra->dataSize-extra->position;
        extra->eof = 1;
     }
     extra->position+=size;
  }
  else{ 
     int remain = 0;
     if (stream->this_frame){
        remain = size-((u32)stream->this_frame-(u32)extra->data);
        if (remain < size){
           memmove(extra->data, stream->this_frame, remain);
           size -= remain;
        }
        else{
             mad_stream_buffer(stream, extra->data, size);//quick fix
             return MAD_FLOW_CONTINUE;
        }
     }
     if (mpeg->flags&AUDIO_FLAG_SCEUID){
        extra->working = 1;
        int read = sceIoRead(mpeg->file, extra->data+remain, size);
        if (read < size){
           if (sceIoLseek(mpeg->file, 0, PSP_SEEK_CUR) >= extra->start+extra->dataSize)
              extra->eof = 1;
           size = read;
           printf("read small %x %d\n", read, extra->eof);
        }
        extra->working = 0;
     }
#ifndef USE_ONLY_SCE_IO
     else if (mpeg->flags&AUDIO_FLAG_FILE){
        extra->working = 1;
        int read = fread(extra->data+remain, 1, size, (FILE*)mpeg->file);
        if (read < size){
           if (ftell((FILE*)mpeg->file) >= extra->start+extra->dataSize)
              extra->eof = 1;
           size = read;
        }
        extra->working = 0;
     }
#endif  
     data = extra->data;
  }

  mad_stream_buffer(stream, data, size);

  return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

static
enum mad_flow output(void *data,
                     struct mad_header const *header,
                     struct mad_pcm *pcm)
{
  PAudio mpeg = data;
  mp3_extra *extra = (mp3_extra*)mpeg->extra;
  short *out[4] = {mpeg->data, mpeg->data+(mpeg->sampleCount*mpeg->channels*2), mpeg->data+(mpeg->sampleCount*mpeg->channels*2)*2, mpeg->data+(mpeg->sampleCount*mpeg->channels*2)*3};
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  /* pcm->samplerate contains the sampling frequency */

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  float i = 0.0f, sScale;
  int fill = extra->fill, index = extra->index, ret;
  if (isSRC())
     sScale = 1.0f;
  else
     sScale = mpeg->scale;
  while (i < nsamples) {

    /* output sample(s) in 16-bit signed little-endian PCM */

    out[index][(fill*mpeg->channels)] = (short)scale(left_ch[(int)i]);

    if (nchannels == 2) {
       out[index][(fill*mpeg->channels)+1] = (short)scale(right_ch[(int)i]);
    }
    fill++;
    i += sScale;
    if (fill == mpeg->sampleCount){
       while (mpeg->play != PLAYING){
             if (mpeg->play == STOP)
                return MAD_FLOW_STOP;
             if (mpeg->play == TERMINATED)
                return MAD_FLOW_STOP;
             sceKernelDelayThread(50); 
       }
       while (sceAudioGetChannelRestLength(mpeg->channel) > 0) sceKernelDelayThread(5);
       mpeg->mixBuffer = out[index];
       if (isSRC())
          ret = sceAudioSRCOutputBlocking(mpeg->lVolume, out[index]);
       else
          ret = sceAudioOutputPanned(mpeg->channel, mpeg->lVolume, mpeg->rVolume, out[index]);
       if (ret < 0) printf("mad audio output 0x%x\n", ret);
       fill = 0;
       index = (index+1)&3;
       mpeg->offset += mpeg->sampleCount*sScale;
    }
  }
  extra->fill = fill;
  extra->index = index;

  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow output2(void *data,
                     struct mad_header const *header,
                     struct mad_pcm *pcm)
{
  PAudio mpeg = data;
//  mp3_extra *extra = (mp3_extra*)mpeg->extra;
  short *out = mpeg->data;
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  /* pcm->samplerate contains the sampling frequency */


  nchannels  = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  int i = 0;
  while (nsamples--) {

    /* output sample(s) in 16-bit signed little-endian PCM */

    out[mpeg->offset*mpeg->channels] = scale(left_ch[i]);

    if (nchannels == 2) {
      out[mpeg->offset*mpeg->channels+1] = scale(right_ch[i]);
    }

    mpeg->offset++;
    i++;
    if (mpeg->offset == mpeg->size){
       return MAD_FLOW_STOP;
    }
  }
  return MAD_FLOW_CONTINUE;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static
enum mad_flow error(void *data,
                    struct mad_stream *stream,
                    struct mad_frame *frame)
{

  printf("decoding error 0x%04x (%s) at byte offset %p\n",
          stream->error, mad_stream_errorstr(stream),
          stream->this_frame);
  if (stream->error == 0x0201){
     PAudio mpeg = data;
     mpeg->play = BUSY;
  }
/*  MAD_ERROR_BUFLEN 0x0001
  MAD_ERROR_BUFPTR	0x0002
  MAD_ERROR_NOMEM 0x0031	  

  MAD_ERROR_LOSTSYNC 0x0101*/
  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
}

/*
 * This is the function called by mad_thread() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */

//static
//enum mad_flow header(void *user, struct mad_header const *header){
//}

static
int decode(PAudio mpeg, int second)
{
  struct mad_decoder decoder;
  int result;

  /* configure input, output, and error functions */

  mad_decoder_init(&decoder, mpeg,
                   input, 0 /* header */, 0 /* filter */, second?output2:output,
                   error, 0 /* message */);

  /* start decoding */

  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  /* release the decoder */

  mad_decoder_finish(&decoder);

  return result;
}
#endif
