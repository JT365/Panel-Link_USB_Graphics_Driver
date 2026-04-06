/**
 * EncodeRGB.c
 *
 * RGBA Convert Algorithm
 *  - Weidong Zhou
 *
 * This is a RGBA convert algorithm for BeadaPanel USB Graphics project.
 *
 * Features
 *  - Implements D3D11 RGBA format to low level RGB16 for BeadaPanel.
 *  - No dynamic allocations.
 *
 * This library is coded in the spirit of the stb libraries and mostly follows
 * the stb guidelines.
 *
 * It is written in C99. And depends on the C standard library.
 * Works with C++11
 *
 *
 */

#include "EncodeRGB.h"

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

static int convertPixes(unsigned char *dest, unsigned char *src, int bytes)
{
    unsigned int k;
    unsigned short *temp;
    unsigned int *ptemp;
			
    temp = (unsigned short *)dest;
    ptemp = (unsigned int *)src;
	
    for (int i = 0; i < bytes/4; i++) {
        k = ptemp[i];
        temp[i] = (unsigned short) (((k>>8) & 0xF800) | ((k>>5) & 0x07E0) | ((k>>3) & 0x001F));		
    }

	return 0;		
}

// - convertBGR16 -
//   len 
//       in -  length of buffer
//       out - length of  payload after process
//    mappedRect
//       in - contains length of a pix line
//    frameDescriptor
//       in - contains width and height of frame buffer
//
int convertBGR16(unsigned char * dest,
				unsigned int * len,
				unsigned char * src,
				int pitch,
				unsigned int width,
				unsigned int height
)
{
	unsigned int nImgSize;

    if ((nImgSize =  width * height * 4) > *len)
        return -1;
        
    if ((unsigned int)(pitch / 4) == width) {
        convertPixes((unsigned char *)dest, src, nImgSize);
    }
    else {
        unsigned char *ptr_tmp = src;
        for (unsigned int i = 0; i < height; i++)
        {
            convertPixes((unsigned char *)dest + i*width*4, ptr_tmp, width*4);
            ptr_tmp += pitch;
        }
    }
        
    *len =  nImgSize/2;
	return 0;
}






