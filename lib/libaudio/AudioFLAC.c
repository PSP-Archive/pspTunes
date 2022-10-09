/*
 * AudioFLAC.c
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
#include "AudioFLAC.h"

extern void register_audio(PAudio);
int flac_reset(PAudio flac){
    flacExtra *extra = (flacExtra*)flac->extra;
    if (FLAC__stream_decoder_reset(extra->decoder)){
       if (FLAC__stream_decoder_process_until_end_of_metadata(extra->decoder) == false){
          printf("flac_thread:reset-%s\n", FLAC__stream_decoder_get_resolved_state_string(extra->decoder));//sceKernelDelayThread(1000000);}//delay
          flac->play = TERMINATED;//critical error I guess
       }
       else while (FLAC__stream_decoder_get_state(extra->decoder) != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC)sceKernelDelayThread(10);
    }
    flac->offset = 0;
    return 0;
}
static int signal_flac(PAudio flac, eventData *event){
    if (!flac) return 1;
    int ret = 0;
    flacExtra *extra = (flacExtra*)flac->extra;
    switch (event->event){
       case EVENT_stopAudio :
          flac->play = STOP;
		  while (extra->working) sceKernelDelayThread(5);
          flac_reset(flac);
          return 0;
       break;
       case EVENT_freeAudio : 
          flac->play = TERMINATED;
          sceKernelWaitThreadEnd(flac->extra->thid, 0);
          sceKernelTerminateDeleteThread(flac->extra->thid);
          if (flac->flags&AUDIO_FLAG_SCEUID)
             ret = sceIoClose((SceUID)flac->file);
          else if (flac->flags&AUDIO_FLAG_FILE)
             fclose((FILE*)flac->file);
          else if (flac->flags&AUDIO_FLAG_MEMORY){
             free(extra->data);
          }
          if (ret < 0){
             printf("close file error\n");
          }
          FLAC__stream_decoder_finish(extra->decoder);
          FLAC__stream_decoder_delete(extra->decoder);
          free(extra);
          return 0;
       break;
       case EVENT_getFrequencyAudio :
          return extra->frequency;
       break;
       case EVENT_seekAudio :
          while (extra->working) sceKernelDelayThread(5);
          if (!FLAC__stream_decoder_seek_absolute(extra->decoder, event->data)){
             printf("Seek:%d:%s\n", event->data, FLAC__stream_decoder_get_resolved_state_string(extra->decoder));
             flac_reset(flac);
          }
          else
             flac->offset = event->data;
       break;
       default :
          return 0;
    }
    return 1;
}

int flac_thread(SceSize args, void *argp){
    PAudio flac = *(PAudio*)argp;
    flacExtra *extra = (flacExtra*)flac->extra;
    FLAC__StreamDecoder *decoder = extra->decoder;
    while (1){//till something better comes along
          if (flac->play != PLAYING){
             if (flac->play == TERMINATED) return 0;
             sceKernelDelayThread(50);
             continue;
          }
          extra->working = 1;
          if (FLAC__stream_decoder_process_single(decoder)){
             if (FLAC__stream_decoder_get_state(decoder) == FLAC__STREAM_DECODER_END_OF_STREAM){
                extra->working = 0;
                if (flac->offset == 0) flac->play = BUSY;//something went wrong 
                if (!flac->loop)
                   stopAudio(flac);  
                else
                   flac_reset(flac);
             }
          } 
          else printf("flac_thread:decode-%s\n", FLAC__stream_decoder_get_resolved_state_string(decoder));
          extra->working = 0;
          sceKernelDelayThread(5);
    }
    return 1;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~callbacks~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     if (extra->eof)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
     if (flac->flags&AUDIO_FLAG_MEMORY){
        int read;
        if (extra->dataSize > extra->position+*bytes*sizeof(FLAC__byte))
           read = *bytes*sizeof(FLAC__byte);
        else
           read = extra->dataSize - extra->position;
        memcpy(buffer, extra->data+extra->position, read);
        extra->position += read;
        *bytes = read/sizeof(FLAC__byte);
        if (extra->position == extra->dataSize)
           extra->eof = 1;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
     }
     if (flac->flags&AUDIO_FLAG_FILE){
        int read = fread(buffer, 1, *bytes*sizeof(FLAC__byte), (FILE*)flac->file);
        *bytes = read/sizeof(FLAC__byte);
        if (feof((FILE*)flac->file))
           extra->eof = 1;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
     }     
     if (flac->flags&AUDIO_FLAG_SCEUID){
        int read = sceIoRead((SceUID)flac->file, buffer, *bytes*sizeof(FLAC__byte));
        if (read < *bytes*sizeof(FLAC__byte))
           extra->eof = 1;
        *bytes = read/sizeof(FLAC__byte);
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
     }
     return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}
static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     if (absolute_byte_offset > extra->dataSize)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
     if (flac->flags&AUDIO_FLAG_MEMORY){
        extra->position = absolute_byte_offset;
        if (extra->position < extra->dataSize)
           extra->eof = 0;
        else
           extra->eof = 1;
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
     }
     if (flac->flags&AUDIO_FLAG_FILE){
        if (fseek((FILE*)flac->file, extra->start+(int)absolute_byte_offset, SEEK_SET) < 0)
           return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
        if (feof((FILE*)flac->file))
           extra->eof = 1;
        else
           extra->eof = 0;
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
     }     
     if (flac->flags&AUDIO_FLAG_SCEUID){
        SceOff pos = sceIoLseek((SceUID)flac->file, extra->start+absolute_byte_offset, SEEK_SET);
        if (pos < extra->start+extra->dataSize)
           extra->eof = 0;
        else
           extra->eof = 1;
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
     }     
     return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
}
static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     if (flac->flags&AUDIO_FLAG_MEMORY){
        *absolute_byte_offset = (FLAC__uint64)extra->position;
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
     }
     if (flac->flags&AUDIO_FLAG_FILE){
        *absolute_byte_offset = (FLAC__uint64)(ftell((FILE*)flac->file)-extra->start);
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
     }     
     if (flac->flags&AUDIO_FLAG_SCEUID){
        *absolute_byte_offset = (FLAC__uint64)(sceIoLseek((SceUID)flac->file, 0, SEEK_CUR)-(SceOff)extra->start);
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
     }     
     return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
}
static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     *stream_length = extra->dataSize;
     return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}
static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     return extra->eof?true:false;
}
extern s16 scale24(const u8 *sample);
static FLAC__StreamDecoderWriteStatus write_callback1(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], PAudio flac){
     flacExtra *extra = (flacExtra*)flac->extra;
     short *out[4] = {flac->data, flac->data+(2*FLAC_SAMPLES*flac->channels), flac->data+(2*FLAC_SAMPLES*flac->channels)*2, flac->data+(2*FLAC_SAMPLES*flac->channels)*3};
     int s = frame->header.blocksize;
     float i = 0.0f, scale;
     if (flac->frequency == getsysFrequency()){
        if (flac->channels == 1){
           if (flac->bitRate == 16){
              while (i < s){
                    int amt;
                    amt = (FLAC_SAMPLES-extra->fill)<(s-i)?(FLAC_SAMPLES-extra->fill):s-i;
                    memcpy(out[extra->index]+extra->fill, buffer[0], amt*2);
                    i += amt;
                    extra->fill = amt;
                    if (extra->fill == FLAC_SAMPLES){
                       flac->mixBuffer = out[extra->index];
                       sceAudioOutputPannedBlocking(flac->channel, flac->lVolume, flac->rVolume, out[extra->index]);
                       extra->index = (extra->index+1)&3;
                       flac->offset += extra->fill;
                       extra->fill = 0;
                    }
              }
              return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
           }
        }
     }
     if (isSRC())
        scale = 1.0f;
     else
        scale = flac->scale;
     //make more efficient? ...nah
     while (i < s){
           while (extra->fill < FLAC_SAMPLES){
#ifdef ALLOW_24_32_BITS                                    
                 if (flac->bitRate == 24)
                    out[extra->index][(extra->fill*flac->channels)] = scale24((u8*)(buffer[0])+(int)i*4*flac->channels);
                 else
#endif
                    out[extra->index][(extra->fill*flac->channels)] = (FLAC__int16)buffer[0][(int)i];
                 if (flac->channels > 1){
#ifdef ALLOW_24_32_BITS                                    
                    if (flac->bitRate == 24)
                       out[extra->index][(extra->fill*flac->channels)+1] = scale24((u8*)(buffer[1])+(int)i*4*flac->channels);
                    else
#endif                    
                       out[extra->index][(extra->fill*flac->channels)+1] = (FLAC__int16)buffer[1][(int)i];
                 }
                 extra->fill++;
                 i += scale;
                 if (i >= s) break;
           }
           if (extra->fill == FLAC_SAMPLES){
              flac->mixBuffer = out[extra->index];
              if (isSRC())
                 sceAudioSRCOutputBlocking(flac->lVolume, out[extra->index]);
              else
                 sceAudioOutputPannedBlocking(flac->channel, flac->lVolume, flac->rVolume, out[extra->index]);
              flac->offset += (float)FLAC_SAMPLES*scale;
              extra->index = (extra->index+1)&3;
              extra->fill = 0;
           }
     }
     return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
static FLAC__StreamDecoderWriteStatus write_callback2(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], PAudio flac){
     short *out = (short*)flac->data;
     int s = frame->header.blocksize;
     int i = 0;
     while (i < s){
           out[flac->size*flac->channels] = (FLAC__int16)buffer[0][i];
           if (flac->channels > 1)
              out[flac->size*flac->channels+1] = (FLAC__int16)buffer[1][i];
           flac->size++;
           i++;
     }
     return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, PAudio flac){
     if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        flac->channels = metadata->data.stream_info.channels;
        flac->frequency = metadata->data.stream_info.sample_rate;
        flac->bitRate = metadata->data.stream_info.bits_per_sample;
        flac->scale = (float)flac->frequency/(float)getsysFrequency();
     }
}
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data){
     printf("Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}
PAudio loadFlacAdvance(int dataSrc, int stream, int size, u32 type, u32 flags){
       if(!dataSrc) return NULL;
       PAudio flac = NULL;
       flacExtra *extra = NULL;
       
       SceUID fd = -1;
       FILE *in = NULL;
       void* data = NULL;
       char *filename = NULL;
       
       FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
       if (!decoder){
          printf("Unable to initialize the decoder\n");
          return NULL;
       }
       flac = malloc(sizeof(Audio));
       if (!flac){
          printf("malloc failed on data\n");
          goto error;
       }
       FLAC__StreamDecoderWriteCallback write_callback;
       if (stream)
          write_callback = (FLAC__StreamDecoderWriteCallback)write_callback1;
       else
          write_callback = (FLAC__StreamDecoderWriteCallback)write_callback2;
       FLAC__StreamDecoderInitStatus ret;
       if (stream == STREAM_FROM_MEMORY){
          if (flags&AUDIO_FLAG_FILE){
             in = (FILE*)dataSrc;
             if (size == 0){
                int start = ftell(in);
                fseek(in, 0, SEEK_END);
                size = ftell(in) - start;
                fseek(in, start, SEEK_SET);
             }
             data = malloc(size);
             if (!data){
                printf("malloc failed on file\n");
                goto error;
             }
             fread(data, 1, size, in);
          }
          else if (flags&AUDIO_FLAG_SCEUID){
             fd = (SceUID)dataSrc;
             if (size == 0){
                int start = sceIoLseek(fd, 0, PSP_SEEK_CUR);
                size = sceIoLseek(fd, 0, PSP_SEEK_END) - start;
                sceIoLseek(fd, start, PSP_SEEK_SET);
             }
             data = malloc(size);
             if (!data){
                printf("malloc failed on data\n");
                goto error;
             }
             sceIoRead(fd, data, size);
          }
          else if (flags&AUDIO_FLAG_MEMORY){
             data = (void*)dataSrc;
          }
          else{
             filename = (char*)dataSrc;
             fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
             if (fd < 0){
                printf("sceIoOpen failed on %s\n", filename);
                goto error;
             }
             if (size == 0){
                size = sceIoLseek(fd, 0, PSP_SEEK_END);
                sceIoLseek(fd, 0, PSP_SEEK_SET);
             }
             data = malloc(size);
             if (!data){
                printf("malloc failed on data\n");
                goto error;
             }
             sceIoRead(fd, data, size);
             sceIoClose(fd);
          }
          flags |= AUDIO_FLAG_MEMORY;
       }
       flac->data = NULL;
       flac->extra = NULL;
       flac->type = fLaC;
       if (!(flags&(AUDIO_FLAG_FILE|AUDIO_FLAG_SCEUID|AUDIO_FLAG_MEMORY))){
          filename = (char*)dataSrc;
          fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
          if (fd < 0){
             printf("sceIoOpen failed on %s\n", filename);
             goto error;
          }
          flags |= AUDIO_FLAG_SCEUID;
       }
       extra = malloc(sizeof(flacExtra));
       if (!extra){
          printf("malloc failed on extra\n");
          goto error;
       }
       flac->extra = (audioExtra*)extra;
       flac->flags = flags;
       extra->dataSize = size;
       if (flags&AUDIO_FLAG_FILE){
          extra->start = ftell(in);
          fseek(in, 0, SEEK_END);
          extra->dataSize = ftell(in) - extra->start;
          fseek(in, extra->start, SEEK_SET);
          flac->file = (u32)in;
       }
       else if (flags&AUDIO_FLAG_SCEUID){           
          extra->start = sceIoLseek(fd, 0, PSP_SEEK_CUR);
          extra->dataSize = sceIoLseek(fd, 0, PSP_SEEK_END) - extra->start;
          sceIoLseek(fd, extra->start, PSP_SEEK_SET);
          flac->file = fd;
       }
       else{
          extra->start = (int)data;
          extra->data = data;
          flac->file = (u32)data;
       }
       extra->eof = 0;
#ifdef OGG_FLAC
       if (flags&FLAC_FLAG_OGG)
          ret = FLAC__stream_decoder_init_ogg_stream(decoder, (FLAC__StreamDecoderReadCallback)read_callback, (FLAC__StreamDecoderSeekCallback)seek_callback, 
                (FLAC__StreamDecoderTellCallback)tell_callback, (FLAC__StreamDecoderLengthCallback)length_callback, (FLAC__StreamDecoderEofCallback)eof_callback, 
                write_callback, (FLAC__StreamDecoderMetadataCallback)metadata_callback, error_callback, flac);
       else
#endif
          ret = FLAC__stream_decoder_init_stream(decoder, (FLAC__StreamDecoderReadCallback)read_callback, (FLAC__StreamDecoderSeekCallback)seek_callback, 
                (FLAC__StreamDecoderTellCallback)tell_callback, (FLAC__StreamDecoderLengthCallback)length_callback, (FLAC__StreamDecoderEofCallback)eof_callback, 
                write_callback, (FLAC__StreamDecoderMetadataCallback)metadata_callback, error_callback, flac);
       if (ret != FLAC__STREAM_DECODER_INIT_STATUS_OK){
          printf("init decoder %s\n", FLAC__stream_decoder_get_resolved_state_string(decoder));
          goto error;
       }
       if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == false){
          printf("end metadata %s\n", FLAC__stream_decoder_get_resolved_state_string(decoder));
          goto error;                                                            
       }
       else while (FLAC__stream_decoder_get_state(decoder) != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC)sceKernelDelayThread(10);
       flac->size = FLAC__stream_decoder_get_total_samples(decoder);
       flac->offset = 0;
       flac->frac = 0;
       flac->stream = stream;
       flac->channel = 0x80026008;
       flac->lVolume = PSP_AUDIO_VOLUME_MAX;
       flac->rVolume = PSP_AUDIO_VOLUME_MAX;
       flac->mixBuffer = NULL;
       flac->play = STOP;
       flac->loop = 0;
       flac->fstate = STATE_F_CLOSE;
       flac->ufunc = NULL;
       if (!stream){
          if (size == 0){
             if (flac->size == 0){
                printf("totalSamples unknown => unsupported\n");
                goto error;
             }
             size = flac->size;
             type = SZ_SAMPLES;
          }
          if (type == SZ_SECONDS){
             size = (size*flac->frequency)/1000;   
          }
          else if (type == SZ_BYTES){
             size = (size*8)/(flac->frequency*flac->bitRate);//size/(flac->frequency*(flac->bitRate/8));
          }
          flac->data = malloc(size*flac->channels*flac->bitRate/8);
          if (!flac->data){
             printf("malloc failed on data\n");
             goto error;
          }
          flac->size = 0;
          while (FLAC__stream_decoder_process_single(decoder)){
                if (FLAC__stream_decoder_get_state(decoder) == FLAC__STREAM_DECODER_END_OF_STREAM){
                   break;
                }
                if (flac->size >= size) break;
          }
          FLAC__stream_decoder_finish(decoder);
          FLAC__stream_decoder_delete(decoder);
          if (flac->flags&(AUDIO_FLAG_FILE|AUDIO_FLAG_MEMORY|AUDIO_FLAG_SCEUID)){
             if (flac->flags&AUDIO_FLAG_MEMORY)
                free(extra->data);
             free(extra);                                                                      
          }
          flac->file = 0;
          flac->type = 0;
          register_audio(flac);
          return flac;
       }
       flac->sampleCount = 1024;
       if (filename){
          if (!strrchr(filename, ':')){
             char path[256];
             getcwd(path, 256);
             flac->filename = malloc(strlen(path)+strlen(filename)+2);
             if (flac->filename){
                strcat(strcat(strcpy(flac->filename, path), "/"), filename);
             }
             else{
                printf("Error memory\n");
                audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
             }
          }
          else{ 
             flac->filename = malloc(strlen(filename)+1);
             if (flac->filename){
                strcpy(flac->filename, filename);
             }
             else{
                printf("Error memory\n");
                audiosetLastError(AUDIO_ERROR_MEM_ALLOC);
             }
          }
       }
       flac->data = malloc(flac->sampleCount*flac->channels*sizeof(short)*4);
       if (!flac->data){
          printf("malloc failed on data\n");
          goto error;
       }
       if (!extra){
          extra = malloc(sizeof(flacExtra));
          if (!extra){
             printf("malloc failed on extra\n");
             goto error;
          }
          flac->extra = (audioExtra*)extra;
       }
       extra->size = sizeof(flacExtra);
       extra->stream = (StreamEventHandler)signal_flac;
       extra->thid = sceKernelCreateThread("flac_stream", flac_thread, 0x10, 0x400*16, PSP_THREAD_ATTR_USER, NULL);
       if (extra->thid < 0){
          printf("Error thread creation\n");
          audiosetLastError(AUDIO_ERROR_THREAD_FAIL);
          goto error;
       } 
       extra->decoder = decoder;
       extra->index = 0;
       extra->eof = 0;
       extra->frequency = flac->frequency;
       if (stream){
          sceKernelStartThread(flac->extra->thid, 4, &flac);
       }
       register_audio(flac);
       return flac;
       error: 
             printf("flac load failed\n");
             if (decoder){
                FLAC__stream_decoder_finish(decoder);
                FLAC__stream_decoder_delete(decoder);
                if (filename && data)
                   free(data);
                if (flac){
                   if (flac->data)
                      free(flac->data);
                   if (flac->extra)
                      free(flac->extra);
                   free(flac);
                }
             } 
             return NULL;
}

PAudio loadFlac(const char *filename, int stream){
       return loadFlacAdvance((int)filename, stream, 0, 0, 0);
}

FLAC__StreamDecoder* getDecoderFlac(const PAudio flac){
                     if (!flac) return NULL;
                     if (flac->type != fLaC) return NULL;
                     flacExtra *extra = (flacExtra*)flac->extra;
                     return extra->decoder;
}
#endif
