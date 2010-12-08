/* vi: set sw=4 ts=4: */
/*
 *  md5.c - Compute MD5 checksum of strings according to the
 *          definition of MD5 in RFC 1321 from April 1992.
 *
 *  Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.
 *
 *  Copyright (C) 1995-1999 Free Software Foundation, Inc.
 *  Copyright (C) 2001 Manuel Novoa III
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003 Erik Andersen
 *
 *  Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#define MD5_RESULT_LEN 16

typedef struct md5_ctx_t {
        uint32_t A;
        uint32_t B;
        uint32_t C;
        uint32_t D;
        uint64_t total;
        uint32_t buflen;
        char buffer[128];
} md5_ctx_t;
#define md5_begin abrt_md5_begin
void md5_begin(md5_ctx_t *ctx);
#define md5_hash abrt_md5_hash
void md5_hash(const void *data, size_t length, md5_ctx_t *ctx);
#define md5_end abrt_md5_end
void md5_end(void *resbuf, md5_ctx_t *ctx);
