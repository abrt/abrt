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
#include "abrtlib.h"
#include "CCpp_sha1.h"

#if defined(__BIG_ENDIAN__) && __BIG_ENDIAN__
# define SHA1_BIG_ENDIAN 1
# define SHA1_LITTLE_ENDIAN 0
#elif __BYTE_ORDER == __BIG_ENDIAN
# define SHA1_BIG_ENDIAN 1
# define SHA1_LITTLE_ENDIAN 0
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define SHA1_BIG_ENDIAN 0
# define SHA1_LITTLE_ENDIAN 1
#else
# error "Can't determine endianness"
#endif


#define rotl32(x,n) (((x) << (n)) | ((x) >> (32 - (n))))
#define rotr32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
/* for sha512: */
#define rotr64(x,n) (((x) >> (n)) | ((x) << (64 - (n))))
#if SHA1_LITTLE_ENDIAN
static inline uint64_t hton64(uint64_t v)
{
	return (((uint64_t)htonl(v)) << 32) | htonl(v >> 32);
}
#else
#define hton64(v) (v)
#endif
#define ntoh64(v) hton64(v)

/* To check alignment gcc has an appropriate operator.  Other
   compilers don't.  */
#if defined(__GNUC__) && __GNUC__ >= 2
# define UNALIGNED_P(p,type) (((uintptr_t) p) % __alignof__(type) != 0)
#else
# define UNALIGNED_P(p,type) (((uintptr_t) p) % sizeof(type) != 0)
#endif


/* Some arch headers have conflicting defines */
#undef ch
#undef parity
#undef maj
#undef rnd

static void sha1_process_block64(sha1_ctx_t *ctx)
{
	unsigned t;
	uint32_t W[80], a, b, c, d, e;
	const uint32_t *words = (uint32_t*) ctx->wbuffer;

	for (t = 0; t < 16; ++t) {
		W[t] = ntohl(*words);
		words++;
	}

	for (/*t = 16*/; t < 80; ++t) {
		uint32_t T = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
		W[t] = rotl32(T, 1);
	}

	a = ctx->hash[0];
	b = ctx->hash[1];
	c = ctx->hash[2];
	d = ctx->hash[3];
	e = ctx->hash[4];

/* Reverse byte order in 32-bit words   */
#define ch(x,y,z)        ((z) ^ ((x) & ((y) ^ (z))))
#define parity(x,y,z)    ((x) ^ (y) ^ (z))
#define maj(x,y,z)       (((x) & (y)) | ((z) & ((x) | (y))))
/* A normal version as set out in the FIPS. This version uses   */
/* partial loop unrolling and is optimised for the Pentium 4    */
#define rnd(f,k) \
	do { \
		uint32_t T = a; \
		a = rotl32(a, 5) + f(b, c, d) + e + k + W[t]; \
		e = d; \
		d = c; \
		c = rotl32(b, 30); \
		b = T; \
	} while (0)

	for (t = 0; t < 20; ++t)
		rnd(ch, 0x5a827999);

	for (/*t = 20*/; t < 40; ++t)
		rnd(parity, 0x6ed9eba1);

	for (/*t = 40*/; t < 60; ++t)
		rnd(maj, 0x8f1bbcdc);

	for (/*t = 60*/; t < 80; ++t)
		rnd(parity, 0xca62c1d6);
#undef ch
#undef parity
#undef maj
#undef rnd

	ctx->hash[0] += a;
	ctx->hash[1] += b;
	ctx->hash[2] += c;
	ctx->hash[3] += d;
	ctx->hash[4] += e;
}

void sha1_begin(sha1_ctx_t *ctx)
{
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
	ctx->total64 = 0;
	ctx->process_block = sha1_process_block64;
}

static const uint32_t init256[] = {
	0x6a09e667,
	0xbb67ae85,
	0x3c6ef372,
	0xa54ff53a,
	0x510e527f,
	0x9b05688c,
	0x1f83d9ab,
	0x5be0cd19
};
static const uint32_t init512_lo[] = {
	0xf3bcc908,
	0x84caa73b,
	0xfe94f82b,
	0x5f1d36f1,
	0xade682d1,
	0x2b3e6c1f,
	0xfb41bd6b,
	0x137e2179
};

/* Used also for sha256 */
void sha1_hash(const void *buffer, size_t len, sha1_ctx_t *ctx)
{
	unsigned in_buf = ctx->total64 & 63;
	unsigned add = 64 - in_buf;

	ctx->total64 += len;

	while (len >= add) {	/* transfer whole blocks while possible  */
		memcpy(ctx->wbuffer + in_buf, buffer, add);
		buffer = (const char *)buffer + add;
		len -= add;
		add = 64;
		in_buf = 0;
		ctx->process_block(ctx);
	}

	memcpy(ctx->wbuffer + in_buf, buffer, len);
}

/* Used also for sha256 */
void sha1_end(void *resbuf, sha1_ctx_t *ctx)
{
	unsigned pad, in_buf;

	in_buf = ctx->total64 & 63;
	/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0... */
	ctx->wbuffer[in_buf++] = 0x80;

	/* This loop iterates either once or twice, no more, no less */
	while (1) {
		pad = 64 - in_buf;
		memset(ctx->wbuffer + in_buf, 0, pad);
		in_buf = 0;
		/* Do we have enough space for the length count? */
		if (pad >= 8) {
			/* Store the 64-bit counter of bits in the buffer in BE format */
			uint64_t t = ctx->total64 << 3;
			t = hton64(t);
			/* wbuffer is suitably aligned for this */
			*(uint64_t *) (&ctx->wbuffer[64 - 8]) = t;
		}
		ctx->process_block(ctx);
		if (pad >= 8)
			break;
	}

	in_buf = (ctx->process_block == sha1_process_block64) ? 5 : 8;
	/* This way we do not impose alignment constraints on resbuf: */
	if (SHA1_LITTLE_ENDIAN) {
		unsigned i;
		for (i = 0; i < in_buf; ++i)
			ctx->hash[i] = htonl(ctx->hash[i]);
	}
	memcpy(resbuf, ctx->hash, sizeof(ctx->hash[0]) * in_buf);
}
