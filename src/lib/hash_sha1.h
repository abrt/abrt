/* vi: set sw=4 ts=4: */
/*
 * Based on shasum from http://www.netsw.org/crypto/hash/
 * Majorly hacked up to use Dr Brian Gladman's sha1 code
 *
 * Copyright (C) 2002 Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * Copyright (C) 2003 Glenn L. McGrath
 * Copyright (C) 2003 Erik Andersen
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * ---------------------------------------------------------------------------
 * Issue Date: 10/11/2002
 *
 * This is a byte oriented version of SHA1 that operates on arrays of bytes
 * stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
 *
 * ---------------------------------------------------------------------------
 */
#ifndef HASH_SHA1_H
#define HASH_SHA1_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define SHA1_RESULT_LEN (5 * 4)

typedef struct sha1_ctx_t {
	uint8_t wbuffer[64]; /* always correctly aligned for uint64_t */
	/* for sha256: void (*process_block)(struct md5_ctx_t*); */
	uint64_t total64;    /* must be directly before hash[] */
	uint32_t hash[8];    /* 4 elements for md5, 5 for sha1, 8 for sha256 */
} sha1_ctx_t;

#define sha1_begin abrt_sha1_begin
void sha1_begin(sha1_ctx_t *ctx);
#define sha1_hash abrt_sha1_hash
void sha1_hash(sha1_ctx_t *ctx, const void *buffer, size_t len);
#define sha1_end abrt_sha1_end
void sha1_end(sha1_ctx_t *ctx, void *resbuf);

#ifdef __cplusplus
}
#endif

#endif
