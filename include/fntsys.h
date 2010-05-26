#ifndef __FNTSYS_H
#define __FNTSYS_H

#include <gsToolkit.h>

void fntInit(void);
void fntLoad(void* buffer, int bufferSize, short clean);
void fntEnd(void);

/** Sets a new aspect ratio
* @note Invalidates the whole glyph cache! */
void fntSetAspectRatio(float aw, float ah);

/** Renders a string
* @return Width of the string rendered */
int fntRenderString(int x, int y, short aligned, const unsigned char* string, u64 colour);

/** Calculates the dimensions of the given text string
@param str The string to calculate dimensions for
@param w the resulting width of the string
@param h the resulting height of the string
*/
void fntCalcDimensions(const unsigned char* str, int *w, int *h);

#endif
