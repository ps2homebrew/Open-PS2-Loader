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
#include <stddef.h>
#include <lwnbd.h>

uint8_t nbd_buffer[NBD_BUFFER_LEN] __attribute__((aligned(64)));

/** @ingroup nbd
 * Fixed newstyle negotiation.
 * @param client_socket
 * @param ctx NBD callback struct
 */
err_t negotiation_phase(const int client_socket, nbd_context **ctxs, nbd_context **ctx)
{
    register int size;
    uint32_t cflags, gflags; //TODO: move in server_ctx struct
    struct nbd_new_option new_opt;
    struct nbd_fixed_new_option_reply fixed_new_option_reply;
    struct nbd_new_handshake new_hs;
    nbd_context **ptr_ctx = NULL;

    /*** handshake ***/

    new_hs.nbdmagic = htonll(NBD_MAGIC);
    new_hs.version = htonll(NBD_NEW_VERSION);
    gflags = (NBD_FLAG_FIXED_NEWSTYLE | NBD_FLAG_NO_ZEROES);
    new_hs.gflags = htons(gflags);
    size = send(client_socket, &new_hs, sizeof(struct nbd_new_handshake),
                0);
    if (size < sizeof(struct nbd_new_handshake))
        return -1;

    size = nbd_recv(client_socket, &cflags, sizeof(cflags), 0);
    if (size < sizeof(cflags))
        return -1;
    cflags = htonl(cflags);

    if (cflags != gflags) {
        LOG("Unsupported client flags %d\n", cflags);
        return -1;
    }

    while (1) {

        /*** options haggling ***/

        size = nbd_recv(client_socket, &new_opt, sizeof(struct nbd_new_option),
                        0);
        if (size < sizeof(struct nbd_new_option))
            return -1;

        new_opt.version = ntohll(new_opt.version);
        if (new_opt.version != NBD_NEW_VERSION)
            return -1;

        new_opt.option = htonl(new_opt.option);
#ifdef DEBUG
        static const char *NBD_OPTIONS[] = {
            NULL,
            "NBD_OPT_EXPORT_NAME",
            "NBD_OPT_ABORT",
            "NBD_OPT_LIST",
            "NBD_OPT_STARTTLS",
            "NBD_OPT_INFO",
            "NBD_OPT_GO",
            "NBD_OPT_STRUCTURED_REPLY",
            "NBD_OPT_LIST_META_CONTEXT",
            "NBD_OPT_SET_META_CONTEXT",
        };
        LOG("%s\n", NBD_OPTIONS[new_opt.option]);
#endif
        new_opt.optlen = htonl(new_opt.optlen);

        if (new_opt.optlen > 0) {
            size = nbd_recv(client_socket, &nbd_buffer, new_opt.optlen, 0);
            nbd_buffer[new_opt.optlen] = '\0';
            DEBUGLOG("client option: %d %s.\n", new_opt.optlen, nbd_buffer);
        }

        switch (new_opt.option) {

            case NBD_OPT_EXPORT_NAME: {
                struct nbd_export_name_option_reply handshake_finish;
                //temporary workaround
                if (new_opt.optlen > 0) {
                    *ctx = nbd_context_getDefaultExportByName(ctxs, (const char *)&nbd_buffer);
                } else
                    *ctx = nbd_context_getDefaultExportByName(ctxs, gdefaultexport);
                //TODO: is that correct ?
                if (*ctx == NULL)
                    *ctx = ctxs[0];
                handshake_finish.exportsize = htonll((*ctx)->export_size);
                handshake_finish.eflags = htons((*ctx)->eflags);
                memset(handshake_finish.zeroes, 0, sizeof(handshake_finish.zeroes));
                size = send(client_socket, &handshake_finish,
                            (cflags & NBD_FLAG_NO_ZEROES) ? offsetof(struct nbd_export_name_option_reply, zeroes) : sizeof handshake_finish, 0);
                if (size < ((cflags & NBD_FLAG_NO_ZEROES) ? offsetof(struct nbd_export_name_option_reply, zeroes) : sizeof handshake_finish))
                    return -1;
                return NBD_OPT_EXPORT_NAME;
            }

            case NBD_OPT_ABORT:
                fixed_new_option_reply.magic = htonll(NBD_REP_MAGIC);
                fixed_new_option_reply.option = htonl(new_opt.option);
                fixed_new_option_reply.reply = htonl(NBD_REP_ACK);
                fixed_new_option_reply.replylen = 0;
                size = send(client_socket, &fixed_new_option_reply,
                            sizeof(struct nbd_fixed_new_option_reply), 0);
                if (size < (sizeof(struct nbd_fixed_new_option_reply)))
                    return -1;
                return NBD_OPT_ABORT;

            case NBD_OPT_LIST: {
                uint32_t name_len, desc_len, len;
                fixed_new_option_reply.magic = htonll(NBD_REP_MAGIC);
                fixed_new_option_reply.option = htonl(new_opt.option);
                fixed_new_option_reply.reply = htonl(NBD_REP_SERVER);
                ptr_ctx = ctxs;
                while (*ptr_ctx) {
                    name_len = strlen((*ptr_ctx)->export_name);
                    DEBUGLOG("%s\n", (*ptr_ctx)->export_name);
                    desc_len = strlen((*ptr_ctx)->export_desc);
                    len = htonl(name_len);
                    fixed_new_option_reply.replylen = htonl(name_len + sizeof(len) +
                                                            desc_len);

                    size = send(client_socket, &fixed_new_option_reply,
                                sizeof(struct nbd_fixed_new_option_reply), MSG_MORE);
                    if (size < (sizeof(struct nbd_fixed_new_option_reply)))
                        return -1;
                    size = send(client_socket, &len, sizeof len, MSG_MORE);
                    if (size < (sizeof len))
                        return -1;
                    size = send(client_socket, (*ptr_ctx)->export_name, name_len, MSG_MORE);
                    if (size < name_len)
                        return -1;
                    size = send(client_socket, (*ptr_ctx)->export_desc, desc_len, MSG_MORE);
                    if (size < desc_len)
                        return -1;
                    ptr_ctx++;
                }
                fixed_new_option_reply.reply = htonl(NBD_REP_ACK);
                fixed_new_option_reply.replylen = 0;
                size = send(client_socket, &fixed_new_option_reply,
                            sizeof(struct nbd_fixed_new_option_reply), 0);
                if (size < sizeof(struct nbd_fixed_new_option_reply))
                    return -1;
                break;
            }

            case NBD_OPT_INFO:
            case NBD_OPT_GO:
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
                if (size < sizeof(struct nbd_fixed_new_option_reply))
                    return -1;
                break;
        }
    }
}

/** @ingroup nbd
 * Transmission phase.
 * @param client_socket
 * @param ctx NBD callback struct
 */
err_t transmission_phase(const int client_socket, nbd_context *ctx)
{
    register int r, size, error = -1, retry = NBD_MAX_RETRIES, sendflag = 0;
    register uint32_t blkremains = 0, byteread = 0, bufbklsz = 0;
    register uint64_t offset = 0;
    struct nbd_simple_reply reply;
    struct nbd_request request;

    if (ctx == NULL) {
        LOG("No context provided.\n");
        return -1;
    }

    while (1) {

        /*** requests handling ***/

        // TODO : blocking here if no proper NBD_CMD_DISC, bad threading design ?
        size = nbd_recv(client_socket, &request, sizeof(struct nbd_request), 0);
        if (size < sizeof(struct nbd_request)) {
            LOG("sizeof nbd_request waited %ld receveid %d\n", sizeof(struct nbd_request), size);
            return -1;
        }

        request.magic = ntohl(request.magic);
        if (request.magic != NBD_REQUEST_MAGIC) {
            LOG("wrong NBD_REQUEST_MAGIC\n");
            return -1;
        }

        request.flags = ntohs(request.flags);
        request.type = ntohs(request.type);
        request.offset = ntohll(request.offset);
        request.count = ntohl(request.count);

        reply.magic = htonl(NBD_SIMPLE_REPLY_MAGIC);
        reply.handle = request.handle;

#ifdef DEBUG
        static const char *NBD_CMD[] = {
            "NBD_CMD_READ",
            "NBD_CMD_WRITE",
            "NBD_CMD_DISC",
            "NBD_CMD_FLUSH",
            "NBD_CMD_TRIM",
            "NBD_CMD_CACHE",
            "NBD_CMD_WRITE_ZEROES",
            "NBD_CMD_BLOCK_STATUS",
        };
        LOG("%s\n", NBD_CMD[request.type]);
#endif

        switch (request.type) {

            case NBD_CMD_READ:

                if (request.offset + request.count > ctx->export_size)
                    error = NBD_EIO;
                else {
                    error = NBD_SUCCESS;
                    sendflag = MSG_MORE;
                    bufbklsz = NBD_BUFFER_LEN / ctx->blocksize;
                    blkremains = request.count / ctx->blocksize;
                    offset = request.offset / ctx->blocksize;
                    byteread = bufbklsz * ctx->blocksize;
                }

                reply.error = ntohl(error);
                r = send(client_socket, &reply, sizeof(struct nbd_simple_reply),
                         sendflag);

                while (sendflag) {

                    if (blkremains < bufbklsz) {
                        bufbklsz = blkremains;
                        byteread = bufbklsz * ctx->blocksize;
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
                        LOG("NBD_CMD_READ : EIO\n");
                        return -1; // -EIO
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
                    bufbklsz = NBD_BUFFER_LEN / ctx->blocksize;
                    blkremains = request.count / ctx->blocksize;
                    offset = request.offset / ctx->blocksize;
                    byteread = bufbklsz * ctx->blocksize;
                }

                while (sendflag) {

                    if (blkremains < bufbklsz) {
                        bufbklsz = blkremains;
                        byteread = bufbklsz * ctx->blocksize;
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
                // There is no reply to an NBD_CMD_DISC.
                return 0;

            case NBD_CMD_FLUSH:
                if (nbd_flush(ctx) == 0)
                    error = NBD_SUCCESS;
                else
                    error = NBD_EIO;
                reply.error = ntohl(error);
                r = send(client_socket, &reply, sizeof(struct nbd_simple_reply),
                         0);
                break;

            case NBD_CMD_TRIM:
            case NBD_CMD_CACHE:
            case NBD_CMD_WRITE_ZEROES:
            case NBD_CMD_BLOCK_STATUS:
            default:
                /* The server SHOULD return NBD_EINVAL if it receives an unknown command. */
                reply.error = ntohl(NBD_EINVAL);
                r = send(client_socket, &reply, sizeof(struct nbd_simple_reply),
                         0);
                break;
        }
    }
    return -1;
}
