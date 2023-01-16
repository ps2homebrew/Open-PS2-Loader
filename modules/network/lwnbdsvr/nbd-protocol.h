/* nbdkit
 * Copyright (C) 2013-2020 Red Hat Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Red Hat nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RED HAT AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RED HAT OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef NBD_PROTOCOL_H
#define NBD_PROTOCOL_H

#include <stdint.h>

/* Note that all NBD fields are sent on the wire in network byte
 * order, so you must use beXXtoh or htobeXX when reading or writing
 * these structures.
 */

#if defined(__GNUC__) || defined(__clang__)
#define NBD_ATTRIBUTE_PACKED __attribute__((__packed__))
#else
#error "Please port to your compiler's notion of a packed struct"
#endif

#define NBD_MAX_STRING 4096 /* Maximum length of a string field */

/* Old-style handshake. */
struct nbd_old_handshake {
  uint64_t nbdmagic;          /* NBD_MAGIC */
  uint64_t version;           /* NBD_OLD_VERSION */
  uint64_t exportsize;
  uint16_t gflags;            /* global flags */
  uint16_t eflags;            /* per-export flags */
  char zeroes[124];           /* must be sent as zero bytes */
} NBD_ATTRIBUTE_PACKED;

#define NBD_MAGIC       UINT64_C(0x4e42444d41474943) /* ASCII "NBDMAGIC" */
#define NBD_OLD_VERSION UINT64_C(0x0000420281861253)

/* New-style handshake. */
struct nbd_new_handshake {
  uint64_t nbdmagic;          /* NBD_MAGIC */
  uint64_t version;           /* NBD_NEW_VERSION */
  uint16_t gflags;            /* global flags */
} NBD_ATTRIBUTE_PACKED;

#define NBD_NEW_VERSION UINT64_C(0x49484156454F5054) /* ASCII "IHAVEOPT" */

/* New-style handshake option (sent by the client to us). */
struct nbd_new_option {
  uint64_t version;           /* NBD_NEW_VERSION */
  uint32_t option;            /* NBD_OPT_* */
  uint32_t optlen;            /* option data length */
  /* option data follows */
} NBD_ATTRIBUTE_PACKED;

/* Newstyle handshake OPT_EXPORT_NAME reply message.
 * Modern clients use NBD_OPT_GO instead of this.
 */
struct nbd_export_name_option_reply {
  uint64_t exportsize;        /* size of export */
  uint16_t eflags;            /* per-export flags */
  char zeroes[124];           /* optional zeroes, unless NBD_FLAG_NO_ZEROES */
} NBD_ATTRIBUTE_PACKED;

/* Fixed newstyle handshake reply message. */
struct nbd_fixed_new_option_reply {
  uint64_t magic;             /* NBD_REP_MAGIC */
  uint32_t option;            /* option we are replying to */
  uint32_t reply;             /* NBD_REP_* */
  uint32_t replylen;
} NBD_ATTRIBUTE_PACKED;

#define NBD_REP_MAGIC UINT64_C(0x3e889045565a9)

/* Global flags. */
#define NBD_FLAG_FIXED_NEWSTYLE    (1 << 0)
#define NBD_FLAG_NO_ZEROES         (1 << 1)

/* Per-export flags. */
#define NBD_FLAG_HAS_FLAGS         (1 << 0)
#define NBD_FLAG_READ_ONLY         (1 << 1)
#define NBD_FLAG_SEND_FLUSH        (1 << 2)
#define NBD_FLAG_SEND_FUA          (1 << 3)
#define NBD_FLAG_ROTATIONAL        (1 << 4)
#define NBD_FLAG_SEND_TRIM         (1 << 5)
#define NBD_FLAG_SEND_WRITE_ZEROES (1 << 6)
#define NBD_FLAG_SEND_DF           (1 << 7)
#define NBD_FLAG_CAN_MULTI_CONN    (1 << 8)
#define NBD_FLAG_SEND_CACHE        (1 << 10)
#define NBD_FLAG_SEND_FAST_ZERO    (1 << 11)

/* NBD options (new style handshake only). */
#define NBD_OPT_EXPORT_NAME        1
#define NBD_OPT_ABORT              2
#define NBD_OPT_LIST               3
#define NBD_OPT_STARTTLS           5
#define NBD_OPT_INFO               6
#define NBD_OPT_GO                 7
#define NBD_OPT_STRUCTURED_REPLY   8
#define NBD_OPT_LIST_META_CONTEXT  9
#define NBD_OPT_SET_META_CONTEXT   10

#define NBD_REP_ERR(val) (0x80000000 | (val))
#define NBD_REP_IS_ERR(val) (!!((val) & 0x80000000))

#define NBD_REP_ACK                  1
#define NBD_REP_SERVER               2
#define NBD_REP_INFO                 3
#define NBD_REP_META_CONTEXT         4
#define NBD_REP_ERR_UNSUP            NBD_REP_ERR (1)
#define NBD_REP_ERR_POLICY           NBD_REP_ERR (2)
#define NBD_REP_ERR_INVALID          NBD_REP_ERR (3)
#define NBD_REP_ERR_PLATFORM         NBD_REP_ERR (4)
#define NBD_REP_ERR_TLS_REQD         NBD_REP_ERR (5)
#define NBD_REP_ERR_UNKNOWN          NBD_REP_ERR (6)
#define NBD_REP_ERR_SHUTDOWN         NBD_REP_ERR (7)
#define NBD_REP_ERR_BLOCK_SIZE_REQD  NBD_REP_ERR (8)
#define NBD_REP_ERR_TOO_BIG          NBD_REP_ERR (9)

#define NBD_INFO_EXPORT      0
#define NBD_INFO_NAME        1
#define NBD_INFO_DESCRIPTION 2
#define NBD_INFO_BLOCK_SIZE  3

/* NBD_INFO_EXPORT reply (follows fixed_new_option_reply). */
struct nbd_fixed_new_option_reply_info_export {
  uint16_t info;                /* NBD_INFO_EXPORT */
  uint64_t exportsize;          /* size of export */
  uint16_t eflags;              /* per-export flags */
} NBD_ATTRIBUTE_PACKED;

/* NBD_INFO_NAME or NBD_INFO_DESCRIPTION reply (follows
 * fixed_new_option_reply).
 */
struct nbd_fixed_new_option_reply_info_name_or_desc {
  uint16_t info;                /* NBD_INFO_NAME, NBD_INFO_DESCRIPTION */
  /* followed by a string name or description */
} NBD_ATTRIBUTE_PACKED;

/* NBD_INFO_BLOCK_SIZE reply (follows fixed_new_option_reply). */
struct nbd_fixed_new_option_reply_info_block_size {
  uint16_t info;                /* NBD_INFO_BLOCK_SIZE */
  uint32_t minimum;             /* minimum block size */
  uint32_t preferred;           /* preferred block size */
  uint32_t maximum;             /* maximum block size */
} NBD_ATTRIBUTE_PACKED;

/* NBD_REP_SERVER reply (follows fixed_new_option_reply). */
struct nbd_fixed_new_option_reply_server {
  uint32_t export_name_len;     /* length of export name */
  /* followed by a string export name and description */
} NBD_ATTRIBUTE_PACKED;

/* NBD_REP_META_CONTEXT reply (follows fixed_new_option_reply). */
struct nbd_fixed_new_option_reply_meta_context {
  uint32_t context_id;          /* metadata context ID */
  /* followed by a string */
} NBD_ATTRIBUTE_PACKED;

/* NBD_REPLY_TYPE_BLOCK_STATUS block descriptor. */
struct nbd_block_descriptor {
  uint32_t length;              /* length of block */
  uint32_t status_flags;        /* block type (hole etc) */
} NBD_ATTRIBUTE_PACKED;

/* Request (client -> server). */
struct nbd_request {
  uint32_t magic;               /* NBD_REQUEST_MAGIC. */
  uint16_t flags;               /* Request flags. */
  uint16_t type;                /* Request type. */
  uint64_t handle;              /* Opaque handle. */
  uint64_t offset;              /* Request offset. */
  uint32_t count;               /* Request length. */
} NBD_ATTRIBUTE_PACKED;

/* Simple reply (server -> client). */
struct nbd_simple_reply {
  uint32_t magic;               /* NBD_SIMPLE_REPLY_MAGIC. */
  uint32_t error;               /* NBD_SUCCESS or one of NBD_E*. */
  uint64_t handle;              /* Opaque handle. */
} NBD_ATTRIBUTE_PACKED;

/* Structured reply (server -> client). */
struct nbd_structured_reply {
  uint32_t magic;               /* NBD_STRUCTURED_REPLY_MAGIC. */
  uint16_t flags;               /* NBD_REPLY_FLAG_* */
  uint16_t type;                /* NBD_REPLY_TYPE_* */
  uint64_t handle;              /* Opaque handle. */
  uint32_t length;              /* Length of payload which follows. */
} NBD_ATTRIBUTE_PACKED;

struct nbd_structured_reply_offset_data {
  uint64_t offset;              /* offset */
  /* Followed by data. */
} NBD_ATTRIBUTE_PACKED;

struct nbd_structured_reply_offset_hole {
  uint64_t offset;
  uint32_t length;              /* Length of hole. */
} NBD_ATTRIBUTE_PACKED;

struct nbd_structured_reply_error {
  uint32_t error;               /* NBD_E* error number */
  uint16_t len;                 /* Length of human readable error. */
  /* Followed by human readable error string, and possibly more structure. */
} NBD_ATTRIBUTE_PACKED;

#define NBD_REQUEST_MAGIC           0x25609513
#define NBD_SIMPLE_REPLY_MAGIC      0x67446698
#define NBD_STRUCTURED_REPLY_MAGIC  0x668e33ef

/* Structured reply flags. */
#define NBD_REPLY_FLAG_DONE         (1<<0)

#define NBD_REPLY_TYPE_ERR(val) ((1<<15) | (val))
#define NBD_REPLY_TYPE_IS_ERR(val) (!!((val) & (1<<15)))

/* Structured reply types. */
#define NBD_REPLY_TYPE_NONE         0
#define NBD_REPLY_TYPE_OFFSET_DATA  1
#define NBD_REPLY_TYPE_OFFSET_HOLE  2
#define NBD_REPLY_TYPE_BLOCK_STATUS 5
#define NBD_REPLY_TYPE_ERROR        NBD_REPLY_TYPE_ERR (1)
#define NBD_REPLY_TYPE_ERROR_OFFSET NBD_REPLY_TYPE_ERR (2)

/* NBD commands. */
#define NBD_CMD_READ              0
#define NBD_CMD_WRITE             1
#define NBD_CMD_DISC              2 /* Disconnect. */
#define NBD_CMD_FLUSH             3
#define NBD_CMD_TRIM              4
#define NBD_CMD_CACHE             5
#define NBD_CMD_WRITE_ZEROES      6
#define NBD_CMD_BLOCK_STATUS      7

#define NBD_CMD_FLAG_FUA       (1<<0)
#define NBD_CMD_FLAG_NO_HOLE   (1<<1)
#define NBD_CMD_FLAG_DF        (1<<2)
#define NBD_CMD_FLAG_REQ_ONE   (1<<3)
#define NBD_CMD_FLAG_FAST_ZERO (1<<4)

/* NBD error codes. */
#define NBD_SUCCESS     0
#define NBD_EPERM       1
#define NBD_EIO         5
#define NBD_ENOMEM     12
#define NBD_EINVAL     22
#define NBD_ENOSPC     28
#define NBD_EOVERFLOW  75
#define NBD_ENOTSUP    95
#define NBD_ESHUTDOWN 108

#endif /* NBD_PROTOCOL_H */
