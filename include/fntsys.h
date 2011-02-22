#ifndef __FNTSYS_H
#define __FNTSYS_H

#include <gsToolkit.h>

/// default (built-in) font id
#define FNT_DEFAULT (0)
/// Value returned on errors
#define FNT_ERROR (-1)

/** Initializes the font subsystem */
void fntInit(void);

/** Terminates the font subsystem */
void fntEnd(void);

/** Loads a font
  * @param buffer The memory buffer containing the font
  * @param bufferSize Size of the buffer
  * @param takeover Set to nonzero
  * @return font slot id (negative value means error happened) */
int fntLoad(void* buffer, int bufferSize, int takeover);

/** Loads a font from a file path
  * @param path The path to the font file
  * @return font slot id (negative value means error happened) */
int fntLoadFile(char* path);

/** Replaces the given font slot with the defined font */
void fntReplace(int id, void* buffer, int bufferSize, int takeover, int asDefault);

/** Reloads the default font into the given font slot */
void fntSetDefault(int id);

/** Releases a font slot */
void fntRelease(int id);

/** Sets a new aspect ratio
* @note Invalidates the whole glyph cache for all fonts! */
void fntSetAspectRatio(float aw, float ah);

/** Renders a string
* @return Width of the string rendered */
int fntRenderString(int font, int x, int y, short aligned, const unsigned char* string, u64 colour);

/** Renders a text with specified window dimensions */
void fntRenderText(int font, int sx, int sy, size_t width, size_t height, const unsigned char* string, u64 colour);

/** replaces spaces with newlines so that the text fits into the specified width.
  * @note A destrutive operation - modifies the given string!
  */
void fntFitString(int font, unsigned char *string, size_t width);

/** Calculates the width of the given text string
* We can't use the height for alignment, as the horizontal center would depends of the contained text itself */
int fntCalcDimensions(int font, const unsigned char* str);

#endif
