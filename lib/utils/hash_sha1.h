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

#ifdef __cplusplus
extern "C" {
#endif

#define SHA1_RESULT_LEN (5 * 4)

typedef struct sha1_ctx_t {
	uint32_t hash[8];    /* 5, +3 elements for sha256 */
	uint64_t total64;
	uint8_t wbuffer[64]; /* NB: always correctly aligned for uint64_t */
	void (*process_block)(struct sha1_ctx_t*);
} sha1_ctx_t;

void sha1_begin(sha1_ctx_t *ctx);
void sha1_hash(const void *buffer, size_t len, sha1_ctx_t *ctx);
void sha1_end(void *resbuf, sha1_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
