#include <pspsdk.h>
#include <pspkernel.h>
#include <string.h>

PSP_MODULE_INFO("MeBootStart", 0x1006, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

int sceMeBootStart(int mebooterType);
int sceMeBootStart371(int mebooterType);
int sceMeBootStart380(int mebooterType);
int sceMeBootStart395(int mebooterType);
int sceMeBootStart500(int mebooterType);


int MeBootStart(int devkitVersion, int mebooterType) {
	u32 k1; 
   	k1 = pspSdkSetK1(0);
   	int ret; 
   	if (devkitVersion < 0x03070000)
		ret = sceMeBootStart(mebooterType);
	else if ( devkitVersion < 0x03080000 )
		ret = sceMeBootStart371(mebooterType);
	else if ( devkitVersion < 0x03090500 )
		ret = sceMeBootStart380(mebooterType);
	else if ( devkitVersion < 0x05000000 )
		ret = sceMeBootStart395(mebooterType);
	else
		ret = sceMeBootStart500(mebooterType);
	pspSdkSetK1(k1);
	return ret;
}

int module_start(SceSize args, void *argp){
	return MeBootStart(sceKernelDevkitVersion(), 3);
}

int module_stop(){
	return 0;
}
