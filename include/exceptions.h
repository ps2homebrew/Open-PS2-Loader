/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * Copyright (C) 2021 fjtrujy (fjtrujy@gmail.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#ifndef EXCEPTION_H
#define EXCEPTION_H

void pkoExceptionHandler(void);

void installExceptionHandlers(void);
void restoreExceptionHandlers();

#endif /* EXCEPTION_H */