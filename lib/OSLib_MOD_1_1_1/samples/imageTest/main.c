#include <pspkernel.h>
#include <oslib/oslib.h>

PSP_MODULE_INFO("Image Test", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(12*1024);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init OSLib:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int initOSLib(){
    oslInit(0);
    oslInitGfx(OSL_PF_8888, 1);
    oslInitAudio();
    oslSetQuitOnLoadFailure(1);
    oslSetKeyAutorepeatInit(20);
    oslSetKeyAutorepeatInterval(5);
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(){
    int skip = 0;
    char message[100] = "";

    initOSLib();
    oslIntraFontInit(INTRAFONT_CACHE_MED);

    //Loads backgrond:
    OSL_IMAGE *bkg = oslLoadImageFilePNG("bkg.png", OSL_IN_RAM | OSL_SWIZZLED, OSL_PF_8888);

    //Loads image:
    OSL_IMAGE *image = oslLoadImageFilePNG("image.png", OSL_IN_RAM | OSL_SWIZZLED, OSL_PF_8888);
    oslSetImageRotCenter(image);

    //Load font:
    OSL_FONT *pgfFont = oslLoadFontFile("flash0:/font/ltn0.pgf");
    oslIntraFontSetStyle(pgfFont, 1.0, RGBA(255,255,255,255), RGBA(0,0,0,0), INTRAFONT_ALIGN_LEFT);
    oslSetFont(pgfFont);

    int posX = 100;
    int posY = 100;

    while(!osl_quit){
        if (!skip){
            oslStartDrawing();

            oslDrawImageXY(bkg, 0, 0);

            oslDrawString(180, 10, "Image Test");

            oslDrawString(30, 25, "CenterX");
            sprintf(message, "%i", image->centerX);
            oslDrawString(200, 25, message);
            oslDrawString(30, 40, "CenterY");
            sprintf(message, "%i", image->centerY);
            oslDrawString(200, 40, message);
            oslDrawString(30, 55, "Angle");
            sprintf(message, "%i", image->angle);
            oslDrawString(200, 55, message);

            oslDrawString(150, 250, "Press X to quit");

            oslDrawImageXY(image, posX, posY);

            oslEndDrawing();
        }
        oslEndFrame();
        skip = oslSyncFrame();

        oslReadKeys();
        if (osl_keys->released.cross)
            oslQuit();
        else if(osl_keys->pressed.R)
            image->angle += 2;
        else if(osl_keys->pressed.L)
            image->angle -= 2;
        else if(osl_keys->released.square)
            oslMirrorImageH(image);
        else if(osl_keys->pressed.right){
            if (posX + image->sizeX + 2 <= 470)
                posX += 2;
        }else if(osl_keys->pressed.left){
            if (posX - 2 >= 0)
                posX -= 2;
        }
    }
    //Quit OSL:
    oslEndGfx();

    sceKernelExitGame();
    return 0;

}
