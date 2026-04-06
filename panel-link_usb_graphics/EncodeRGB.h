/**
 * EncodeRGB.h
 *
 * RGBA Convert Algorithm
 *  - Weidong Zhou
 *
 * This is a RGBA convert algorithm for BeadaPanel USB Graphics project.
 *
 * Features
 *  - Implements D3D RGBA format to low level RGB16 for BeadaPanel.
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
#ifndef _ENCODE_RGB_H
#define _ENCODE_RGB_H

#ifdef __cplusplus
extern "C"
{
#endif

// - convertBGR16 -
//   len 
//       in -  length of buffer
//       out - length of payload after process
//    mappedRect
//       in - contain length of a pix line
//    frameDescriptor
//       in - contain width and height of frame buffer
//
int convertBGR16(unsigned char * dest,
                 unsigned int * len,
                 unsigned char * src,
				int pitch,
				unsigned int width,
				unsigned int height
				);


#ifdef __cplusplus
}  // extern C
#endif

#endif  //_ENCODE_RGB_H

