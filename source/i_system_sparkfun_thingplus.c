#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#ifdef RP2040

    #include "doomdef.h"
    #include "doomtype.h"
    #include "d_main.h"
    #include "d_event.h"

    #include "global_data.h"

    #include "tables.h"

#include "i_system_e32.h"

#include "lprintf.h"


#define DCNT_PAGE 0x0010

#define TM_FREQ_1024 0x0003
#define TM_ENABLE 0x0080
#define TM_CASCADE 0x0004
#define TM_FREQ_1024 0x0003
#define TM_FREQ_256 0x0002

#define REG_WAITCNT	*((vu16 *)(0x4000204))


//**************************************************************************************


void I_InitScreen_e32()
{
  
}

//**************************************************************************************

void I_BlitScreenBmp_e32()
{

}

//**************************************************************************************

void I_StartWServEvents_e32()
{

}

//**************************************************************************************

void I_PollWServEvents_e32()
{
    
}

//**************************************************************************************

void I_ClearWindow_e32()
{

}

//**************************************************************************************

static unsigned short buffer1[MAX_SCREENHEIGHT*MAX_SCREENWIDTH];
static unsigned short buffer2[MAX_SCREENHEIGHT*MAX_SCREENWIDTH];
static char show_buffer1 = true;
 
unsigned short* I_GetBackBuffer()
{
   if (show_buffer1) return &buffer2[0];
   else return &buffer1[0];
}

//**************************************************************************************

unsigned short* I_GetFrontBuffer()
{
    if (show_buffer1) return &buffer1[0];
   else return &buffer2[0];
}

//**************************************************************************************

void I_CreateWindow_e32()
{


}

//**************************************************************************************

void I_CreateBackBuffer_e32()
{
    I_CreateWindow_e32();
}

//**************************************************************************************

void I_FinishUpdate_e32(const byte* srcBuffer, const byte* pallete, const unsigned int width, const unsigned int height)
{
   show_buffer1 = !show_buffer1;
}

//**************************************************************************************

void I_SetPallete_e32(const byte* pallete)
{
   
}

//**************************************************************************************

int I_GetVideoWidth_e32()
{
    return 120;
}

//**************************************************************************************

int I_GetVideoHeight_e32()
{
    return 160;
}



//**************************************************************************************

void I_ProcessKeyEvents()
{
 
}

//**************************************************************************************

#define MAX_MESSAGE_SIZE 1024

void I_Error (const char *error, ...)
{
    char msg[MAX_MESSAGE_SIZE];

    va_list v;
    va_start(v, error);

    vsprintf(msg, error, v);

    va_end(v);

    printf("%s", msg);
}

//**************************************************************************************

void I_Quit_e32()
{

}

//**************************************************************************************

#endif 