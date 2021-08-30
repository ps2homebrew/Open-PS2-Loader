/****************************************************************/ /**
 *
 * @file nbd_protocol.c
 *
 * @author   Ronan Bignaux <ronan@aimao.org>
 *
 * @brief    Network Block Device Protocol server
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

#include <string.h>
#include "nbd_server.h"
uint8_t nbd_buffer[NBD_BUFFER_LEN] __attribute__((aligned(64)));

/** @ingroup nbd
 * Fixed newstyle negotiation.
 * @param client_socket
 * @param ctx NBD callback struct
 */
nbd_context *negotiation_phase(int client_socket, nbd_context **ctxs)
{
    register int size;
    uint32_t cflags, name_len, desc_len, len;
    struct nbd_new_option new_opt;
    struct nbd_export_name_option_reply handshake_finish;
    struct nbd_fixed_new_option_reply fixed_new_option_reply;
    struct nbd_new_handshake new_hs;

    //temporary workaround
    nbd_context *ctx = ctxs[0];

    /*** handshake ***/

    new_hs.nbdmagic = htonll(NBD_MAGIC);
    new_hs.version = htonll(NBD_NEW_VERSION);
    new_hs.gflags = 0; //htons(NBD_FLAG_FIXED_NEWSTYLE);
    size = send(client_socket, &new_hs, sizeof(struct nbd_new_handshake),
                0);
    if (size < sizeof(struct nbd_new_handshake))
        goto error;

    size = nbd_recv(client_socket, &cflags, sizeof(cflags), 0);
    if (size < sizeof(cflags))
        goto error;
    cflags = htonl(cflags);
    /* TODO: manage cflags
     * https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md#client-flags
     */

    ctx->eflags = NBD_FLAG_HAS_FLAGS;

    while (1) {

        /*** options haggling ***/

        size = nbd_recv(client_socket, &new_opt, sizeof(struct nbd_new_option),
                        0);
        if (size < sizeof(struct nbd_new_option))
            goto error;

        new_opt.version = ntohll(new_opt.version);
        if (new_opt.version != NBD_NEW_VERSION)
            goto error;

        new_opt.option = htonl(new_opt.option);
        new_opt.optlen = htonl(new_opt.optlen);

        if (new_opt.optlen > 0) {
            size = nbd_recv(client_socket, &nbd_buffer, new_opt.optlen, 0);
            nbd_buffer[new_opt.optlen] = '\0';
        }

        switch (new_opt.option) {

            case NBD_OPT_EXPORT_NAME:

                handshake_finish.exportsize = htonll(ctx->export_size);
                handshake_finish.eflags = htons(ctx->eflags);
                // TODO if NBD_FLAG_NO_ZEROES / NBD_FLAG_C_NO_ZEROES
                memset(handshake_finish.zeroes, 0, sizeof(handshake_finish.zeroes));
                size = send(client_socket, &handshake_finish,
                            sizeof(struct nbd_export_name_option_reply), 0);
                goto abort;

            case NBD_OPT_ABORT:
                //TODO : test
                fixed_new_option_reply.magic = htonll(NBD_REP_MAGIC);
                fixed_new_option_reply.option = htonl(new_opt.option);
                fixed_new_option_reply.reply = htonl(NBD_REP_ACK);
                fixed_new_option_reply.replylen = 0;
                size = send(client_socket, &fixed_new_option_reply,
                            sizeof(struct nbd_fixed_new_option_reply), 0);
                goto soft_disconnect;
                break;
            // see nbdkit send_newstyle_option_reply_exportnames()
            case NBD_OPT_LIST:

                name_len = strlen(ctx->export_name);
                desc_len = ctx->export_desc ? strlen(ctx->export_desc) : 0;
                len = htonl(name_len);

                //TODO : many export in a loop
                fixed_new_option_reply.magic = htonll(NBD_REP_MAGIC);
                fixed_new_option_reply.option = htonl(new_opt.option);
                fixed_new_option_reply.reply = htonl(NBD_REP_SERVER);
                fixed_new_option_reply.replylen = htonl(name_len + sizeof(len) +
                                                        desc_len);

                size = send(client_socket, &fixed_new_option_reply,
                            sizeof(struct nbd_fixed_new_option_reply), MSG_MORE);
                size = send(client_socket, &len, sizeof len, MSG_MORE);
                size = send(client_socket, ctx->export_name, name_len, MSG_MORE);
                size = send(client_socket, ctx->export_desc, desc_len, 0);
                break;
                //TODO
                //                break;
                //            case NBD_OPT_STARTTLS:
                //                break;
                // see nbdkit send_newstyle_option_reply_info_export()
            case NBD_OPT_INFO:
            case NBD_OPT_GO:


                //            	if (new_opt.option == NBD_OPT_GO)
                //            		goto abort;
                //            	break;
            case NBD_OPT_STRUCTURED_REPLY:
            case NBD_OPT_LIST_META_CONTEXT:
            case NBD_OPT_SET_META_CONTEXT:
            default:
                //TODO: test
                fixed_new_option_reply.magic = htonll(NBD_REP_MAGIC);
                fixed_new_option_reply.option = htonl(new_opt.option);
                fixed_new_option_reply.reply = htonl(NBD_REP_ERR_UNSUP);
                fixed_new_option_reply.replylen = 0;
                size = send(client_socket, &fixed_new_option_reply,
                            sizeof(struct nbd_fixed_new_option_reply), 0);
                break;
        }
    }

abort:
    return ctx;
soft_disconnect:
error:
    return NULL;
}

/** @ingroup nbd
 * Transmission phase.
 * @param client_socket
 * @param ctx NBD callback struct
 */
int transmission_phase(int client_socket, nbd_context *ctx)
{
    register int r, size, error = -1, retry = NBD_MAX_RETRIES, sendflag = 0;
    register uint32_t blkremains = 0, byteread = 0, bufbklsz = 0;
    register uint64_t offset = 0;
    struct nbd_simple_reply reply;
    struct nbd_request request;

    reply.magic = htonl(NBD_SIMPLE_REPLY_MAGIC);

    while (1) {

        /*** requests handling ***/

        // TODO : blocking here if no proper NBD_CMD_DISC, bad threading design ?
        size = nbd_recv(client_socket, &request, sizeof(struct nbd_request), 0);
        if (size < sizeof(struct nbd_request)) {
            printf("lwNBD : sizeof NOK\n");
            goto error;
        }

        // printf("lwNBD : sizeof OK.\n");

        request.magic = ntohl(request.magic);
        if (request.magic != NBD_REQUEST_MAGIC) {
            printf("lwNBD : wrong NBD_REQUEST_MAGIC\n");
            goto error;
        }

        request.flags = ntohs(request.flags);
        request.type = ntohs(request.type);
        request.offset = ntohll(request.offset);
        request.count = ntohl(request.count);

        reply.handle = request.handle;

        // printf("lwNBD: entering NBD_CMD %d.\n", request.type);

        switch (request.type) {

            case NBD_CMD_READ:

                if (request.offset + request.count > ctx->export_size)
                    error = NBD_EIO;
                else {
                    error = NBD_SUCCESS;
                    sendflag = MSG_MORE;
                    bufbklsz = NBD_BUFFER_LEN >> ctx->blockshift;
                    blkremains = request.count >> ctx->blockshift;
                    offset = request.offset >> ctx->blockshift;
                    byteread = bufbklsz << ctx->blockshift;
                }

                reply.error = ntohl(error);
                r = send(client_socket, &reply, sizeof(struct nbd_simple_reply),
                         sendflag);

                while (sendflag) {

                    if (blkremains < bufbklsz) {
                        bufbklsz = blkremains;
                        byteread = bufbklsz << ctx->blockshift;
                    }

                    if (blkremains <= bufbklsz)
                        sendflag = 0;

                    r = nbd_read(ctx, ctx->buffer, offset, bufbklsz);

                    if (r == 0) {
                        r = send(client_socket, ctx->buffer, byteread, sendflag);
                        if (r != byteread)
                            break;
                        offset += bufbklsz;
                        blkremains -= bufbklsz;
                        retry = NBD_MAX_RETRIES;
                    } else if (retry) {
                        retry--;
                        sendflag = 1;
                    } else {
                        printf("NBD_CMD_READ : EIO\n");
                        goto error; // -EIO
                                    //                    	LWIP_DEBUGF(NBD_DEBUG | LWIP_DBG_STATE, ("nbd: error read\n"));
                    }
                }

                break;

            case NBD_CMD_WRITE:

                if (ctx->eflags & NBD_FLAG_READ_ONLY)
                    error = NBD_EPERM;
                else if (request.offset + request.count > ctx->export_size)
                    error = NBD_ENOSPC;
                else {
                    error = NBD_SUCCESS;
                    sendflag = MSG_MORE;
                    bufbklsz = NBD_BUFFER_LEN >> ctx->blockshift;
                    blkremains = request.count >> ctx->blockshift;
                    offset = request.offset >> ctx->blockshift;
                    byteread = bufbklsz << ctx->blockshift;
                }

                while (sendflag) {

                    if (blkremains < bufbklsz) {
                        bufbklsz = blkremains;
                        byteread = bufbklsz << ctx->blockshift;
                    }

                    if (blkremains <= bufbklsz)
                        sendflag = 0;

                    r = nbd_recv(client_socket, ctx->buffer, byteread, 0);

                    if (r == byteread) {
                        r = nbd_write(ctx, ctx->buffer, offset, bufbklsz);
                        if (r != 0) {
                            error = NBD_EIO;
                            sendflag = 0;
                            //                    	LWIP_DEBUGF(NBD_DEBUG | LWIP_DBG_STATE, ("nbd: error read\n"));
                        }
                        offset += bufbklsz;
                        blkremains -= bufbklsz;
                        retry = NBD_MAX_RETRIES;
                    } else {
                        error = NBD_EOVERFLOW; //TODO
                        sendflag = 0;
                        //                    	LWIP_DEBUGF(NBD_DEBUG | LWIP_DBG_STATE, ("nbd: error read\n"));
                    }
                }

                reply.error = ntohl(error);
                r = send(client_socket, &reply, sizeof(struct nbd_simple_reply),
                         0);

                break;

            case NBD_CMD_DISC:
                //TODO
                goto soft_disconnect;
                break;

            case NBD_CMD_FLUSH:
                nbd_flush(ctx);
                break;

            case NBD_CMD_TRIM:
            case NBD_CMD_CACHE:
            case NBD_CMD_WRITE_ZEROES:
            case NBD_CMD_BLOCK_STATUS:
            default:
                /* NBD_REP_ERR_INVALID */
                goto error;
        }
    }

soft_disconnect:
    return NBD_SUCCESS;
error:
    return error;
}
