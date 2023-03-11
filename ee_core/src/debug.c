
#include "ee_core.h"
#include <sio.h>
#include <stdio.h>
#include <unistd.h>

void ee_printf(const char* format, ...)
{
     char buffer[256+1];
     va_list args;
     
     // Format the string.
     va_start(args, format);
     vsnprintf(buffer, sizeof(buffer), format, args);
     va_end(args);
     
     sio_putsn(buffer);
}