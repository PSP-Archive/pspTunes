/*
 * main.c
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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psptypes.h>
#include <pspgu.h>
#include <pspsdk.h>
#include <malloc.h>
#include <psputility.h>
#include <psprtc.h>
#include <pspmath.h>
#include <Audio/Audio.h>
#include <Audio/AudioAT3.h>

//sample to play aa3 acc file

PSP_MODULE_INFO("XXX", 0, 1, 1);
PSP_HEAP_SIZE_KB(-2048);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU); 

static int running = 1;
/* Exit callback */ 
int exit_callback(int arg1, int arg2, void *common) 
{ 
   running = 0; 
   return 0; 
} 
   
/* Callback thread */ 
int CallbackThread(SceSize args, void *argp) 
{ 
   int cbid; 

   cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL); 
   sceKernelRegisterExitCallback(cbid); 

   sceKernelSleepThreadCB(); 

   return 0; 
}


/* Sets up the callback thread and returns its thread id */ 
int SetupCallbacks(void) 
{ 
   int thid = 0; 

   thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0); 
   if(thid >= 0)
   { 
      sceKernelStartThread(thid, 0, 0); 
   } 

   return thid;
} 

//ripped out from graphics.c
#define	PSP_LINE_SIZE 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define FRAMEBUFFER_SIZE (PSP_LINE_SIZE*SCREEN_HEIGHT*4)
#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4) /* change this if you change to another screenmode */
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2) /* zbuffer seems to be 16-bit? */
unsigned int __attribute__((aligned(16))) _list[262144];
unsigned int list = _list;
void guStart()
{
	sceGuStart(GU_DIRECT, list);
}
void initGraphics()
{

	sceGuInit();

	guStart();list = _list+8;
	sceGuDrawBuffer(GU_PSM_8888, (void*)FRAMEBUFFER_SIZE, PSP_LINE_SIZE);
	sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void*)0, PSP_LINE_SIZE);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuDepthBuffer((void*) (FRAMEBUFFER_SIZE*2), PSP_LINE_SIZE);
	sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuEnable(GU_ALPHA_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuAmbientColor(0xffffffff);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}
typedef struct ScePspFVectorX {
	int    	c;
	float 	x;
	float 	y;
	float 	z;
} ScePspFVectorX;

int main(int argc, char* argv[]){
    SetupCallbacks();
    pspDebugScreenInit();
    audioInit(0);
    initGraphics();
    PAudio audio = loadAa3("sample.aa3");
    if (!audio) sceKernelExitGame();
    loopAudio(audio);
    playAudio(audio);
    short *mix;
    void *drawBuffer = 0;// = sceGeEdramGetAddr();
    int channels = getChannelsAudio(audio);
    int samples;
    ScePspFVectorX *left, *right;
    SceCtrlData pad, oldpad; 
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(1);
    sceCtrlReadBufferPositive(&oldpad, 0);   
    unsigned long ticksPerSec; 
    unsigned long frame = 0; 
    unsigned long fps = 0; 
    unsigned long long fpsTickLast; 
    unsigned long long fpsTickNow;
    sceRtcGetCurrentTick( &fpsTickLast );
	ticksPerSec = sceRtcGetTickResolution();
	int lines = 0;
	int circles = 0;
	int waves = 1;
	
	typedef float (*trig_f)(float);
	trig_f sin = sinf, cos = cosf;

    while (running){
          sceCtrlReadBufferPositive(&pad, 1);
          if (pad.Buttons != oldpad.Buttons){
             if (pad.Buttons & PSP_CTRL_LTRIGGER){
                circles ^= 1;
             } 
             if (pad.Buttons & PSP_CTRL_SQUARE){
                lines ^= 1;
             } 
             if (pad.Buttons & PSP_CTRL_TRIANGLE){
                waves ^= 1;        
             } 
             if (pad.Buttons & PSP_CTRL_CROSS){
                pauseAudio2(audio);             
             } 
             if (pad.Buttons & PSP_CTRL_CIRCLE){
                stopAudio(audio);             
             }
             if (pad.Buttons & PSP_CTRL_UP){
                sin = sinf; cos = cosf;
             }
             if (pad.Buttons & PSP_CTRL_DOWN){
                sin = vfpu_sinf; cos = vfpu_cosf;
             }
             if (pad.Buttons & PSP_CTRL_LEFT){
                
             }
             if (pad.Buttons & PSP_CTRL_RIGHT){
                
             }
             if (pad.Buttons & PSP_CTRL_START){
                freeAudio(audio);
                break;            
             }
             if (pad.Buttons & PSP_CTRL_SELECT){
                scePowerRequestSuspend();
             }
          }
          mix = getMixBufferAudio(audio, &samples);
          if (!mix){ 
             continue;
          }  
          sceGuStart(GU_DIRECT, list);
          sceGuClearColor(0);
          sceGuClear(GU_COLOR_BUFFER_BIT);          
          sceGuDisable(GU_TEXTURE_2D);
          if (waves){
             left = sceGuGetMemory(samples*sizeof(ScePspFVectorX));
             right = sceGuGetMemory(samples*sizeof(ScePspFVectorX));
             float dx = 480.0f/(float)samples;
             int i;
             for (i = 0; i < samples; i++){
                 float ldy = ((float)mix[channels*i]/32767.0f);
                 if (ldy < 0)
                    left[i].c = GU_COLOR(1.0f,0.0f,0.0f,1.0f);
                 else
                    left[i].c = GU_COLOR(0.0f,0.0f,1.0f,1.0f);
                 left[i].y = 80 - ldy*50.0f;
                 if (channels == 2){
                    float rdy = ((float)mix[2*i+1]/32767.0f);
                    if (rdy < 0)
                       right[i].c = GU_COLOR(1.0f,0.0f,0.0f,1.0f);
                    else
                       right[i].c = GU_COLOR(0.0f,0.0f,1.0f,1.0f);
                    right[i].y = 180 - rdy*50.0f;
                 }
                 else{
                    right[i].c = 0;
                    right[i].y = 150.0f;
                 }
                 left[i].x = i*dx;
                 right[i].x = i*dx;
                 left[i].z = 0.0f;
                 right[i].z = 0.0f;
              }
              sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples, 0, left);
              sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples, 0, right);
          }
          if (lines){
             left = sceGuGetMemory(samples*sizeof(ScePspFVectorX)*2);
             right = sceGuGetMemory(samples*sizeof(ScePspFVectorX)*2);
             float dx = 480.0f/(float)samples;
             int i;
             for (i = 0; i < samples; i++){
                 float ldy = ((float)mix[channels*i]/32767.0f);
                 left[i*2].c = GU_COLOR(0.0f,1.0f,0.0f,1.0f);
                 if (ldy < 0)
                    left[i*2+1].c = GU_COLOR(fabs(ldy),0.0f,0.0f,1.0f);
                 else
                    left[i*2+1].c = GU_COLOR(0.0f,0.0f,ldy,1.0f);
                 left[i*2].y = 80;
                 left[i*2+1].y = 80 - ldy*50.0f;
                 if (channels == 2){
                    float rdy = ((float)mix[2*i+1]/32767.0f);
                    if (rdy < 0)
                       right[i*2+1].c = GU_COLOR(fabs(ldy),0.0f,0.0f,1.0f);
                    else
                       right[i*2+1].c = GU_COLOR(0.0f,0.0f,ldy,1.0f);
                    right[i*2].y = 180;
                    right[i*2+1].y = 180 - rdy*50.0f;
                    right[i*2].c = GU_COLOR(0.0f,1.0f,0.0f,1.0f);
                 }
                 else{
                    right[i*2].c = 0;
                    right[i*2+1].c = 0;
                    right[i*2].y = 150.0f;
                    right[i*2+1].y = 150.0f;
                 }
                 left[i*2].x = i*dx;
                 right[i*2].x = i*dx;
                 left[i*2].z = 0.0f;
                 right[i*2].z = 0.0f;
                 left[i*2+1].x = i*dx;
                 right[i*2+1].x = i*dx;
                 left[i*2+1].z = 0.0f;
                 right[i*2+1].z = 0.0f;
             }
             sceGuDrawArray(GU_LINES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples*2, 0, left);
             sceGuDrawArray(GU_LINES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples*2, 0, right);
          } 
          if (circles){
             left = sceGuGetMemory(samples*sizeof(ScePspFVectorX)*2);
             if (channels == 2)
                right = sceGuGetMemory(samples*sizeof(ScePspFVectorX)*2);
             int i;
         	 float angle = 0.0f, angre = GU_PI, theta = 2.0f*GU_PI/(float)samples;
             for (i = 0; i < samples; i++){
                 float ldy = ((float)mix[channels*i]/327.670f);
                 left[i*2].c = 0xff00ff00;
                 left[i*2].x = 100.0f;
                 left[i*2].y = 136.0f;
                 left[i*2].z = 0.0f;
                 if (ldy < 0.0f){
                    left[i*2+1].c = 0xff0000ff;
                 }
                 else{
                    left[i*2+1].c = 0xffff0000;
                 }
                 left[i*2+1].x = left[i*2].x+fabs(ldy)*sin(angle);
                 left[i*2+1].y = left[i*2].y+fabs(ldy)*-1.0f*cos(angle);
                 left[i*2+1].z = 0.0f;
                 if (channels == 2){
                    float rdy = ((float)mix[2*i+1]/327.670f);
                    right[i*2].c = 0xff00ff00;
                    right[i*2].x = 300.0f;
                    right[i*2].y = left[i*2].y;
                    right[i*2].z = left[i*2].z;
                    if (rdy < 0.0f){
                       right[i*2+1].c = 0xff0000ff;
                    }
                    else{
                       right[i*2+1].c = 0xffff0000;
                    }
                    right[i*2+1].x = right[i*2].x+fabs(rdy)*sin(angre);
                    right[i*2+1].y = right[i*2].y+fabs(rdy)*-1.0f*cos(angre);
                    right[i*2+1].z = 0.0f;
                 }
                 angle += theta;
                 angre += theta;
             }
             sceGuDrawArray(GU_LINES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples*2, 0, left);
             if (channels == 2)
                sceGuDrawArray(GU_LINES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, samples*2, 0, right);
          }
          sceGuFinish();
          sceGuSync(0,0);
          pspDebugScreenSetOffset((int)drawBuffer);
	      pspDebugScreenSetXY(0, 0);
	      pspDebugScreenPrintf("Select Suspend\tFrequency: %d\tFps: %lu\n^ Normal Math\t%d:%d\nv Vector floating point Math\t%c/%c/L Toggle Visualizations", 
                               getFrequencyAudio(audio), fps, getTimeAudio(audio, SZ_SAMPLES)/getFrequencyAudio(audio), getLengthAudio(audio, SZ_SAMPLES)/getFrequencyAudio(audio), (char)219, (char)206);
          drawBuffer = sceGuSwapBuffers();
          frame++;
          sceRtcGetCurrentTick(&fpsTickNow);
          if(((fpsTickNow - fpsTickLast)/((float)ticksPerSec)) >= 1.0f ){
		      fpsTickLast = fpsTickNow;
		      fps = frame;
		      frame = 0;
	      }          
          oldpad = pad;
    }
    audioEnd();
    sceKernelExitGame();
    return 0;
}
