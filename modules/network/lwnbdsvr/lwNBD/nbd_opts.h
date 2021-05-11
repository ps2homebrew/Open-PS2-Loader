/****************************************************************/ /**
 *
 * @file nbd_opts.h
 *
 * @author   Ronan Bignaux <ronan@aimao.org>
 *
 * @brief    Network Block Device Protocol implementation options
 *
 * Copyright (c) Ronan Bignaux. 2021
 * All rights reserved.
 *
 ********************************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification,are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Ronan Bignaux <ronan@aimao.org>
 *
 */

#ifndef LWIP_HDR_APPS_NBD_OPTS_H
#define LWIP_HDR_APPS_NBD_OPTS_H

#ifndef PS2SDK
#include "lwip/opt.h"
#else
#include "tcpip.h"
#endif

/**
 * @defgroup NBD_opts Options
 * @ingroup NBD
 * @{
 */

/**
 * Enable NBD debug messages
 */
#if !defined NBD_DEBUG || defined __DOXYGEN__
#define NBD_DEBUG LWIP_DBG_ON
#endif

/**
 * NBD server port
 */
#if !defined NBD_SERVER_PORT || defined __DOXYGEN__
#define NBD_SERVER_PORT 10809
#endif

/**
 * NBD timeout
 */
#if !defined NBD_TIMEOUT_MSECS || defined __DOXYGEN__
#define NBD_TIMEOUT_MSECS 10000
#endif

/**
 * Max. number of retries when a file is read from server
 */
#if !defined NBD_MAX_RETRIES || defined __DOXYGEN__
#define NBD_MAX_RETRIES 5
#endif

/**
 * NBD timer cyclic interval
 */
#if !defined NBD_TIMER_MSECS || defined __DOXYGEN__
#define NBD_TIMER_MSECS 50
#endif

/**
 * Max. length of NBD buffer : NBD_MAX_STRING is minimal size for the buffer
 */
#if !defined NBD_BUFFER_LEN || defined __DOXYGEN__
#define NBD_BUFFER_LEN 512 * 256
#endif

/**
 * Max. length of NBD mode
 */
#if !defined NBD_MAX_MODE_LEN || defined __DOXYGEN__
#define NBD_MAX_MODE_LEN 7
#endif

/**
 * @}
 */

#endif /* LWIP_HDR_APPS_NBD_OPTS_H */
