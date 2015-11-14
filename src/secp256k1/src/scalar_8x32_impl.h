/**********************************************************************
 * copyright (c) 2014 pieter wuille                                   *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_scalar_repr_impl_h_
#define _secp256k1_scalar_repr_impl_h_

/* limbs of the secp256k1 order. */
#define secp256k1_n_0 ((uint32_t)0xd0364141ul)
#define secp256k1_n_1 ((uint32_t)0xbfd25e8cul)
#define secp256k1_n_2 ((uint32_t)0xaf48a03bul)
#define secp256k1_n_3 ((uint32_t)0xbaaedce6ul)
#define secp256k1_n_4 ((uint32_t)0xfffffffeul)
#define secp256k1_n_5 ((uint32_t)0xfffffffful)
#define secp256k1_n_6 ((uint32_t)0xfffffffful)
#define secp256k1_n_7 ((uint32_t)0xfffffffful)

/* limbs of 2^256 minus the secp256k1 order. */
#define secp256k1_n_c_0 (~secp256k1_n_0 + 1)
#define secp256k1_n_c_1 (~secp256k1_n_1)
#define secp256k1_n_c_2 (~secp256k1_n_2)
#define secp256k1_n_c_3 (~secp256k1_n_3)
#define secp256k1_n_c_4 (1)

/* limbs of half the secp256k1 order. */
#define secp256k1_n_h_0 ((uint32_t)0x681b20a0ul)
#define secp256k1_n_h_1 ((uint32_t)0xdfe92f46ul)
#define secp256k1_n_h_2 ((uint32_t)0x57a4501dul)
#define secp256k1_n_h_3 ((uint32_t)0x5d576e73ul)
#define secp256k1_n_h_4 ((uint32_t)0xfffffffful)
#define secp256k1_n_h_5 ((uint32_t)0xfffffffful)
#define secp256k1_n_h_6 ((uint32_t)0xfffffffful)
#define secp256k1_n_h_7 ((uint32_t)0x7ffffffful)

secp256k1_inline static void secp256k1_scalar_clear(secp256k1_scalar_t *r) {
    r->d[0] = 0;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;
    r->d[4] = 0;
    r->d[5] = 0;
    r->d[6] = 0;
    r->d[7] = 0;
}

secp256k1_inline static void secp256k1_scalar_set_int(secp256k1_scalar_t *r, unsigned int v) {
    r->d[0] = v;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;
    r->d[4] = 0;
    r->d[5] = 0;
    r->d[6] = 0;
    r->d[7] = 0;
}

secp256k1_inline static unsigned int secp256k1_scalar_get_bits(const secp256k1_scalar_t *a, unsigned int offset, unsigned int count) {
    verify_check((offset + count - 1) >> 5 == offset >> 5);
    return (a->d[offset >> 5] >> (offset & 0x1f)) & ((1 << count) - 1);
}

secp256k1_inline static unsigned int secp256k1_scalar_get_bits_var(const secp256k1_scalar_t *a, unsigned int offset, unsigned int count) {
    verify_check(count < 32);
    verify_check(offset + count <= 256);
    if ((offset + count - 1) >> 5 == offset >> 5) {
        return secp256k1_scalar_get_bits(a, offset, count);
    } else {
        verify_check((offset >> 5) + 1 < 8);
        return ((a->d[offset >> 5] >> (offset & 0x1f)) | (a->d[(offset >> 5) + 1] << (32 - (offset & 0x1f)))) & ((((uint32_t)1) << count) - 1);
    }
}

secp256k1_inline static int secp256k1_scalar_check_overflow(const secp256k1_scalar_t *a) {
    int yes = 0;
    int no = 0;
    no |= (a->d[7] < secp256k1_n_7); /* no need for a > check. */
    no |= (a->d[6] < secp256k1_n_6); /* no need for a > check. */
    no |= (a->d[5] < secp256k1_n_5); /* no need for a > check. */
    no |= (a->d[4] < secp256k1_n_4);
    yes |= (a->d[4] > secp256k1_n_4) & ~no;
    no |= (a->d[3] < secp256k1_n_3) & ~yes;
    yes |= (a->d[3] > secp256k1_n_3) & ~no;
    no |= (a->d[2] < secp256k1_n_2) & ~yes;
    yes |= (a->d[2] > secp256k1_n_2) & ~no;
    no |= (a->d[1] < secp256k1_n_1) & ~yes;
    yes |= (a->d[1] > secp256k1_n_1) & ~no;
    yes |= (a->d[0] >= secp256k1_n_0) & ~no;
    return yes;
}

secp256k1_inline static int secp256k1_scalar_reduce(secp256k1_scalar_t *r, uint32_t overflow) {
    uint64_t t;
    verify_check(overflow <= 1);
    t = (uint64_t)r->d[0] + overflow * secp256k1_n_c_0;
    r->d[0] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[1] + overflow * secp256k1_n_c_1;
    r->d[1] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[2] + overflow * secp256k1_n_c_2;
    r->d[2] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[3] + overflow * secp256k1_n_c_3;
    r->d[3] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[4] + overflow * secp256k1_n_c_4;
    r->d[4] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[5];
    r->d[5] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[6];
    r->d[6] = t & 0xfffffffful; t >>= 32;
    t += (uint64_t)r->d[7];
    r->d[7] = t & 0xfffffffful;
    return overflow;
}

static int secp256k1_scalar_add(secp256k1_scalar_t *r, const secp256k1_scalar_t *a, const secp256k1_scalar_t *b) {
    int overflow;
    uint64_t t = (uint64_t)a->d[0] + b->d[0];
    r->d[0] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[1] + b->d[1];
    r->d[1] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[2] + b->d[2];
    r->d[2] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[3] + b->d[3];
    r->d[3] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[4] + b->d[4];
    r->d[4] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[5] + b->d[5];
    r->d[5] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[6] + b->d[6];
    r->d[6] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)a->d[7] + b->d[7];
    r->d[7] = t & 0xffffffffull; t >>= 32;
    overflow = t + secp256k1_scalar_check_overflow(r);
    verify_check(overflow == 0 || overflow == 1);
    secp256k1_scalar_reduce(r, overflow);
    return overflow;
}

static void secp256k1_scalar_add_bit(secp256k1_scalar_t *r, unsigned int bit) {
    uint64_t t;
    verify_check(bit < 256);
    t = (uint64_t)r->d[0] + (((uint32_t)((bit >> 5) == 0)) << (bit & 0x1f));
    r->d[0] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[1] + (((uint32_t)((bit >> 5) == 1)) << (bit & 0x1f));
    r->d[1] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[2] + (((uint32_t)((bit >> 5) == 2)) << (bit & 0x1f));
    r->d[2] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[3] + (((uint32_t)((bit >> 5) == 3)) << (bit & 0x1f));
    r->d[3] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[4] + (((uint32_t)((bit >> 5) == 4)) << (bit & 0x1f));
    r->d[4] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[5] + (((uint32_t)((bit >> 5) == 5)) << (bit & 0x1f));
    r->d[5] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[6] + (((uint32_t)((bit >> 5) == 6)) << (bit & 0x1f));
    r->d[6] = t & 0xffffffffull; t >>= 32;
    t += (uint64_t)r->d[7] + (((uint32_t)((bit >> 5) == 7)) << (bit & 0x1f));
    r->d[7] = t & 0xffffffffull;
#ifdef verify
    verify_check((t >> 32) == 0);
    verify_check(secp256k1_scalar_check_overflow(r) == 0);
#endif
}

static void secp256k1_scalar_set_b32(secp256k1_scalar_t *r, const unsigned char *b32, int *overflow) {
    int over;
    r->d[0] = (uint32_t)b32[31] | (uint32_t)b32[30] << 8 | (uint32_t)b32[29] << 16 | (uint32_t)b32[28] << 24;
    r->d[1] = (uint32_t)b32[27] | (uint32_t)b32[26] << 8 | (uint32_t)b32[25] << 16 | (uint32_t)b32[24] << 24;
    r->d[2] = (uint32_t)b32[23] | (uint32_t)b32[22] << 8 | (uint32_t)b32[21] << 16 | (uint32_t)b32[20] << 24;
    r->d[3] = (uint32_t)b32[19] | (uint32_t)b32[18] << 8 | (uint32_t)b32[17] << 16 | (uint32_t)b32[16] << 24;
    r->d[4] = (uint32_t)b32[15] | (uint32_t)b32[14] << 8 | (uint32_t)b32[13] << 16 | (uint32_t)b32[12] << 24;
    r->d[5] = (uint32_t)b32[11] | (uint32_t)b32[10] << 8 | (uint32_t)b32[9] << 16 | (uint32_t)b32[8] << 24;
    r->d[6] = (uint32_t)b32[7] | (uint32_t)b32[6] << 8 | (uint32_t)b32[5] << 16 | (uint32_t)b32[4] << 24;
    r->d[7] = (uint32_t)b32[3] | (uint32_t)b32[2] << 8 | (uint32_t)b32[1] << 16 | (uint32_t)b32[0] << 24;
    over = secp256k1_scalar_reduce(r, secp256k1_scalar_check_overflow(r));
    if (overflow) {
        *overflow = over;
    }
}

static void secp256k1_scalar_get_b32(unsigned char *bin, const secp256k1_scalar_t* a) {
    bin[0] = a->d[7] >> 24; bin[1] = a->d[7] >> 16; bin[2] = a->d[7] >> 8; bin[3] = a->d[7];
    bin[4] = a->d[6] >> 24; bin[5] = a->d[6] >> 16; bin[6] = a->d[6] >> 8; bin[7] = a->d[6];
    bin[8] = a->d[5] >> 24; bin[9] = a->d[5] >> 16; bin[10] = a->d[5] >> 8; bin[11] = a->d[5];
    bin[12] = a->d[4] >> 24; bin[13] = a->d[4] >> 16; bin[14] = a->d[4] >> 8; bin[15] = a->d[4];
    bin[16] = a->d[3] >> 24; bin[17] = a->d[3] >> 16; bin[18] = a->d[3] >> 8; bin[19] = a->d[3];
    bin[20] = a->d[2] >> 24; bin[21] = a->d[2] >> 16; bin[22] = a->d[2] >> 8; bin[23] = a->d[2];
    bin[24] = a->d[1] >> 24; bin[25] = a->d[1] >> 16; bin[26] = a->d[1] >> 8; bin[27] = a->d[1];
    bin[28] = a->d[0] >> 24; bin[29] = a->d[0] >> 16; bin[30] = a->d[0] >> 8; bin[31] = a->d[0];
}

secp256k1_inline static int secp256k1_scalar_is_zero(const secp256k1_scalar_t *a) {
    return (a->d[0] | a->d[1] | a->d[2] | a->d[3] | a->d[4] | a->d[5] | a->d[6] | a->d[7]) == 0;
}

static void secp256k1_scalar_negate(secp256k1_scalar_t *r, const secp256k1_scalar_t *a) {
    uint32_t nonzero = 0xfffffffful * (secp256k1_scalar_is_zero(a) == 0);
    uint64_t t = (uint64_t)(~a->d[0]) + secp256k1_n_0 + 1;
    r->d[0] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[1]) + secp256k1_n_1;
    r->d[1] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[2]) + secp256k1_n_2;
    r->d[2] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[3]) + secp256k1_n_3;
    r->d[3] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[4]) + secp256k1_n_4;
    r->d[4] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[5]) + secp256k1_n_5;
    r->d[5] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[6]) + secp256k1_n_6;
    r->d[6] = t & nonzero; t >>= 32;
    t += (uint64_t)(~a->d[7]) + secp256k1_n_7;
    r->d[7] = t & nonzero;
}

secp256k1_inline static int secp256k1_scalar_is_one(const secp256k1_scalar_t *a) {
    return ((a->d[0] ^ 1) | a->d[1] | a->d[2] | a->d[3] | a->d[4] | a->d[5] | a->d[6] | a->d[7]) == 0;
}

static int secp256k1_scalar_is_high(const secp256k1_scalar_t *a) {
    int yes = 0;
    int no = 0;
    no |= (a->d[7] < secp256k1_n_h_7);
    yes |= (a->d[7] > secp256k1_n_h_7) & ~no;
    no |= (a->d[6] < secp256k1_n_h_6) & ~yes; /* no need for a > check. */
    no |= (a->d[5] < secp256k1_n_h_5) & ~yes; /* no need for a > check. */
    no |= (a->d[4] < secp256k1_n_h_4) & ~yes; /* no need for a > check. */
    no |= (a->d[3] < secp256k1_n_h_3) & ~yes;
    yes |= (a->d[3] > secp256k1_n_h_3) & ~no;
    no |= (a->d[2] < secp256k1_n_h_2) & ~yes;
    yes |= (a->d[2] > secp256k1_n_h_2) & ~no;
    no |= (a->d[1] < secp256k1_n_h_1) & ~yes;
    yes |= (a->d[1] > secp256k1_n_h_1) & ~no;
    yes |= (a->d[0] > secp256k1_n_h_0) & ~no;
    return yes;
}

/* inspired by the macros in openssl's crypto/bn/asm/x86_64-gcc.c. */

/** add a*b to the number defined by (c0,c1,c2). c2 must never overflow. */
#define muladd(a,b) { \
    uint32_t tl, th; \
    { \
        uint64_t t = (uint64_t)a * b; \
        th = t >> 32;         /* at most 0xfffffffe */ \
        tl = t; \
    } \
    c0 += tl;                 /* overflow is handled on the next line */ \
    th += (c0 < tl) ? 1 : 0;  /* at most 0xffffffff */ \
    c1 += th;                 /* overflow is handled on the next line */ \
    c2 += (c1 < th) ? 1 : 0;  /* never overflows by contract (verified in the next line) */ \
    verify_check((c1 >= th) || (c2 != 0)); \
}

/** add a*b to the number defined by (c0,c1). c1 must never overflow. */
#define muladd_fast(a,b) { \
    uint32_t tl, th; \
    { \
        uint64_t t = (uint64_t)a * b; \
        th = t >> 32;         /* at most 0xfffffffe */ \
        tl = t; \
    } \
    c0 += tl;                 /* overflow is handled on the next line */ \
    th += (c0 < tl) ? 1 : 0;  /* at most 0xffffffff */ \
    c1 += th;                 /* never overflows by contract (verified in the next line) */ \
    verify_check(c1 >= th); \
}

/** add 2*a*b to the number defined by (c0,c1,c2). c2 must never overflow. */
#define muladd2(a,b) { \
    uint32_t tl, th, th2, tl2; \
    { \
        uint64_t t = (uint64_t)a * b; \
        th = t >> 32;               /* at most 0xfffffffe */ \
        tl = t; \
    } \
    th2 = th + th;                  /* at most 0xfffffffe (in case th was 0x7fffffff) */ \
    c2 += (th2 < th) ? 1 : 0;       /* never overflows by contract (verified the next line) */ \
    verify_check((th2 >= th) || (c2 != 0)); \
    tl2 = tl + tl;                  /* at most 0xfffffffe (in case the lowest 63 bits of tl were 0x7fffffff) */ \
    th2 += (tl2 < tl) ? 1 : 0;      /* at most 0xffffffff */ \
    c0 += tl2;                      /* overflow is handled on the next line */ \
    th2 += (c0 < tl2) ? 1 : 0;      /* second overflow is handled on the next line */ \
    c2 += (c0 < tl2) & (th2 == 0);  /* never overflows by contract (verified the next line) */ \
    verify_check((c0 >= tl2) || (th2 != 0) || (c2 != 0)); \
    c1 += th2;                      /* overflow is handled on the next line */ \
    c2 += (c1 < th2) ? 1 : 0;       /* never overflows by contract (verified the next line) */ \
    verify_check((c1 >= th2) || (c2 != 0)); \
}

/** add a to the number defined by (c0,c1,c2). c2 must never overflow. */
#define sumadd(a) { \
    unsigned int over; \
    c0 += (a);                  /* overflow is handled on the next line */ \
    over = (c0 < (a)) ? 1 : 0; \
    c1 += over;                 /* overflow is handled on the next line */ \
    c2 += (c1 < over) ? 1 : 0;  /* never overflows by contract */ \
}

/** add a to the number defined by (c0,c1). c1 must never overflow, c2 must be zero. */
#define sumadd_fast(a) { \
    c0 += (a);                 /* overflow is handled on the next line */ \
    c1 += (c0 < (a)) ? 1 : 0;  /* never overflows by contract (verified the next line) */ \
    verify_check((c1 != 0) | (c0 >= (a))); \
    verify_check(c2 == 0); \
}

/** extract the lowest 32 bits of (c0,c1,c2) into n, and left shift the number 32 bits. */
#define extract(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = c2; \
    c2 = 0; \
}

/** extract the lowest 32 bits of (c0,c1,c2) into n, and left shift the number 32 bits. c2 is required to be zero. */
#define extract_fast(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = 0; \
    verify_check(c2 == 0); \
}

static void secp256k1_scalar_reduce_512(secp256k1_scalar_t *r, const uint32_t *l) {
    uint64_t c;
    uint32_t n0 = l[8], n1 = l[9], n2 = l[10], n3 = l[11], n4 = l[12], n5 = l[13], n6 = l[14], n7 = l[15];
    uint32_t m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12;
    uint32_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

    /* 96 bit accumulator. */
    uint32_t c0, c1, c2;

    /* reduce 512 bits into 385. */
    /* m[0..12] = l[0..7] + n[0..7] * secp256k1_n_c. */
    c0 = l[0]; c1 = 0; c2 = 0;
    muladd_fast(n0, secp256k1_n_c_0);
    extract_fast(m0);
    sumadd_fast(l[1]);
    muladd(n1, secp256k1_n_c_0);
    muladd(n0, secp256k1_n_c_1);
    extract(m1);
    sumadd(l[2]);
    muladd(n2, secp256k1_n_c_0);
    muladd(n1, secp256k1_n_c_1);
    muladd(n0, secp256k1_n_c_2);
    extract(m2);
    sumadd(l[3]);
    muladd(n3, secp256k1_n_c_0);
    muladd(n2, secp256k1_n_c_1);
    muladd(n1, secp256k1_n_c_2);
    muladd(n0, secp256k1_n_c_3);
    extract(m3);
    sumadd(l[4]);
    muladd(n4, secp256k1_n_c_0);
    muladd(n3, secp256k1_n_c_1);
    muladd(n2, secp256k1_n_c_2);
    muladd(n1, secp256k1_n_c_3);
    sumadd(n0);
    extract(m4);
    sumadd(l[5]);
    muladd(n5, secp256k1_n_c_0);
    muladd(n4, secp256k1_n_c_1);
    muladd(n3, secp256k1_n_c_2);
    muladd(n2, secp256k1_n_c_3);
    sumadd(n1);
    extract(m5);
    sumadd(l[6]);
    muladd(n6, secp256k1_n_c_0);
    muladd(n5, secp256k1_n_c_1);
    muladd(n4, secp256k1_n_c_2);
    muladd(n3, secp256k1_n_c_3);
    sumadd(n2);
    extract(m6);
    sumadd(l[7]);
    muladd(n7, secp256k1_n_c_0);
    muladd(n6, secp256k1_n_c_1);
    muladd(n5, secp256k1_n_c_2);
    muladd(n4, secp256k1_n_c_3);
    sumadd(n3);
    extract(m7);
    muladd(n7, secp256k1_n_c_1);
    muladd(n6, secp256k1_n_c_2);
    muladd(n5, secp256k1_n_c_3);
    sumadd(n4);
    extract(m8);
    muladd(n7, secp256k1_n_c_2);
    muladd(n6, secp256k1_n_c_3);
    sumadd(n5);
    extract(m9);
    muladd(n7, secp256k1_n_c_3);
    sumadd(n6);
    extract(m10);
    sumadd_fast(n7);
    extract_fast(m11);
    verify_check(c0 <= 1);
    m12 = c0;

    /* reduce 385 bits into 258. */
    /* p[0..8] = m[0..7] + m[8..12] * secp256k1_n_c. */
    c0 = m0; c1 = 0; c2 = 0;
    muladd_fast(m8, secp256k1_n_c_0);
    extract_fast(p0);
    sumadd_fast(m1);
    muladd(m9, secp256k1_n_c_0);
    muladd(m8, secp256k1_n_c_1);
    extract(p1);
    sumadd(m2);
    muladd(m10, secp256k1_n_c_0);
    muladd(m9, secp256k1_n_c_1);
    muladd(m8, secp256k1_n_c_2);
    extract(p2);
    sumadd(m3);
    muladd(m11, secp256k1_n_c_0);
    muladd(m10, secp256k1_n_c_1);
    muladd(m9, secp256k1_n_c_2);
    muladd(m8, secp256k1_n_c_3);
    extract(p3);
    sumadd(m4);
    muladd(m12, secp256k1_n_c_0);
    muladd(m11, secp256k1_n_c_1);
    muladd(m10, secp256k1_n_c_2);
    muladd(m9, secp256k1_n_c_3);
    sumadd(m8);
    extract(p4);
    sumadd(m5);
    muladd(m12, secp256k1_n_c_1);
    muladd(m11, secp256k1_n_c_2);
    muladd(m10, secp256k1_n_c_3);
    sumadd(m9);
    extract(p5);
    sumadd(m6);
    muladd(m12, secp256k1_n_c_2);
    muladd(m11, secp256k1_n_c_3);
    sumadd(m10);
    extract(p6);
    sumadd_fast(m7);
    muladd_fast(m12, secp256k1_n_c_3);
    sumadd_fast(m11);
    extract_fast(p7);
    p8 = c0 + m12;
    verify_check(p8 <= 2);

    /* reduce 258 bits into 256. */
    /* r[0..7] = p[0..7] + p[8] * secp256k1_n_c. */
    c = p0 + (uint64_t)secp256k1_n_c_0 * p8;
    r->d[0] = c & 0xfffffffful; c >>= 32;
    c += p1 + (uint64_t)secp256k1_n_c_1 * p8;
    r->d[1] = c & 0xfffffffful; c >>= 32;
    c += p2 + (uint64_t)secp256k1_n_c_2 * p8;
    r->d[2] = c & 0xfffffffful; c >>= 32;
    c += p3 + (uint64_t)secp256k1_n_c_3 * p8;
    r->d[3] = c & 0xfffffffful; c >>= 32;
    c += p4 + (uint64_t)p8;
    r->d[4] = c & 0xfffffffful; c >>= 32;
    c += p5;
    r->d[5] = c & 0xfffffffful; c >>= 32;
    c += p6;
    r->d[6] = c & 0xfffffffful; c >>= 32;
    c += p7;
    r->d[7] = c & 0xfffffffful; c >>= 32;

    /* final reduction of r. */
    secp256k1_scalar_reduce(r, c + secp256k1_scalar_check_overflow(r));
}

static void secp256k1_scalar_mul_512(uint32_t *l, const secp256k1_scalar_t *a, const secp256k1_scalar_t *b) {
    /* 96 bit accumulator. */
    uint32_t c0 = 0, c1 = 0, c2 = 0;

    /* l[0..15] = a[0..7] * b[0..7]. */
    muladd_fast(a->d[0], b->d[0]);
    extract_fast(l[0]);
    muladd(a->d[0], b->d[1]);
    muladd(a->d[1], b->d[0]);
    extract(l[1]);
    muladd(a->d[0], b->d[2]);
    muladd(a->d[1], b->d[1]);
    muladd(a->d[2], b->d[0]);
    extract(l[2]);
    muladd(a->d[0], b->d[3]);
    muladd(a->d[1], b->d[2]);
    muladd(a->d[2], b->d[1]);
    muladd(a->d[3], b->d[0]);
    extract(l[3]);
    muladd(a->d[0], b->d[4]);
    muladd(a->d[1], b->d[3]);
    muladd(a->d[2], b->d[2]);
    muladd(a->d[3], b->d[1]);
    muladd(a->d[4], b->d[0]);
    extract(l[4]);
    muladd(a->d[0], b->d[5]);
    muladd(a->d[1], b->d[4]);
    muladd(a->d[2], b->d[3]);
    muladd(a->d[3], b->d[2]);
    muladd(a->d[4], b->d[1]);
    muladd(a->d[5], b->d[0]);
    extract(l[5]);
    muladd(a->d[0], b->d[6]);
    muladd(a->d[1], b->d[5]);
    muladd(a->d[2], b->d[4]);
    muladd(a->d[3], b->d[3]);
    muladd(a->d[4], b->d[2]);
    muladd(a->d[5], b->d[1]);
    muladd(a->d[6], b->d[0]);
    extract(l[6]);
    muladd(a->d[0], b->d[7]);
    muladd(a->d[1], b->d[6]);
    muladd(a->d[2], b->d[5]);
    muladd(a->d[3], b->d[4]);
    muladd(a->d[4], b->d[3]);
    muladd(a->d[5], b->d[2]);
    muladd(a->d[6], b->d[1]);
    muladd(a->d[7], b->d[0]);
    extract(l[7]);
    muladd(a->d[1], b->d[7]);
    muladd(a->d[2], b->d[6]);
    muladd(a->d[3], b->d[5]);
    muladd(a->d[4], b->d[4]);
    muladd(a->d[5], b->d[3]);
    muladd(a->d[6], b->d[2]);
    muladd(a->d[7], b->d[1]);
    extract(l[8]);
    muladd(a->d[2], b->d[7]);
    muladd(a->d[3], b->d[6]);
    muladd(a->d[4], b->d[5]);
    muladd(a->d[5], b->d[4]);
    muladd(a->d[6], b->d[3]);
    muladd(a->d[7], b->d[2]);
    extract(l[9]);
    muladd(a->d[3], b->d[7]);
    muladd(a->d[4], b->d[6]);
    muladd(a->d[5], b->d[5]);
    muladd(a->d[6], b->d[4]);
    muladd(a->d[7], b->d[3]);
    extract(l[10]);
    muladd(a->d[4], b->d[7]);
    muladd(a->d[5], b->d[6]);
    muladd(a->d[6], b->d[5]);
    muladd(a->d[7], b->d[4]);
    extract(l[11]);
    muladd(a->d[5], b->d[7]);
    muladd(a->d[6], b->d[6]);
    muladd(a->d[7], b->d[5]);
    extract(l[12]);
    muladd(a->d[6], b->d[7]);
    muladd(a->d[7], b->d[6]);
    extract(l[13]);
    muladd_fast(a->d[7], b->d[7]);
    extract_fast(l[14]);
    verify_check(c1 == 0);
    l[15] = c0;
}

static void secp256k1_scalar_sqr_512(uint32_t *l, const secp256k1_scalar_t *a) {
    /* 96 bit accumulator. */
    uint32_t c0 = 0, c1 = 0, c2 = 0;

    /* l[0..15] = a[0..7]^2. */
    muladd_fast(a->d[0], a->d[0]);
    extract_fast(l[0]);
    muladd2(a->d[0], a->d[1]);
    extract(l[1]);
    muladd2(a->d[0], a->d[2]);
    muladd(a->d[1], a->d[1]);
    extract(l[2]);
    muladd2(a->d[0], a->d[3]);
    muladd2(a->d[1], a->d[2]);
    extract(l[3]);
    muladd2(a->d[0], a->d[4]);
    muladd2(a->d[1], a->d[3]);
    muladd(a->d[2], a->d[2]);
    extract(l[4]);
    muladd2(a->d[0], a->d[5]);
    muladd2(a->d[1], a->d[4]);
    muladd2(a->d[2], a->d[3]);
    extract(l[5]);
    muladd2(a->d[0], a->d[6]);
    muladd2(a->d[1], a->d[5]);
    muladd2(a->d[2], a->d[4]);
    muladd(a->d[3], a->d[3]);
    extract(l[6]);
    muladd2(a->d[0], a->d[7]);
    muladd2(a->d[1], a->d[6]);
    muladd2(a->d[2], a->d[5]);
    muladd2(a->d[3], a->d[4]);
    extract(l[7]);
    muladd2(a->d[1], a->d[7]);
    muladd2(a->d[2], a->d[6]);
    muladd2(a->d[3], a->d[5]);
    muladd(a->d[4], a->d[4]);
    extract(l[8]);
    muladd2(a->d[2], a->d[7]);
    muladd2(a->d[3], a->d[6]);
    muladd2(a->d[4], a->d[5]);
    extract(l[9]);
    muladd2(a->d[3], a->d[7]);
    muladd2(a->d[4], a->d[6]);
    muladd(a->d[5], a->d[5]);
    extract(l[10]);
    muladd2(a->d[4], a->d[7]);
    muladd2(a->d[5], a->d[6]);
    extract(l[11]);
    muladd2(a->d[5], a->d[7]);
    muladd(a->d[6], a->d[6]);
    extract(l[12]);
    muladd2(a->d[6], a->d[7]);
    extract(l[13]);
    muladd_fast(a->d[7], a->d[7]);
    extract_fast(l[14]);
    verify_check(c1 == 0);
    l[15] = c0;
}

#undef sumadd
#undef sumadd_fast
#undef muladd
#undef muladd_fast
#undef muladd2
#undef extract
#undef extract_fast

static void secp256k1_scalar_mul(secp256k1_scalar_t *r, const secp256k1_scalar_t *a, const secp256k1_scalar_t *b) {
    uint32_t l[16];
    secp256k1_scalar_mul_512(l, a, b);
    secp256k1_scalar_reduce_512(r, l);
}

static void secp256k1_scalar_sqr(secp256k1_scalar_t *r, const secp256k1_scalar_t *a) {
    uint32_t l[16];
    secp256k1_scalar_sqr_512(l, a);
    secp256k1_scalar_reduce_512(r, l);
}

#ifdef use_endomorphism
static void secp256k1_scalar_split_128(secp256k1_scalar_t *r1, secp256k1_scalar_t *r2, const secp256k1_scalar_t *a) {
    r1->d[0] = a->d[0];
    r1->d[1] = a->d[1];
    r1->d[2] = a->d[2];
    r1->d[3] = a->d[3];
    r1->d[4] = 0;
    r1->d[5] = 0;
    r1->d[6] = 0;
    r1->d[7] = 0;
    r2->d[0] = a->d[4];
    r2->d[1] = a->d[5];
    r2->d[2] = a->d[6];
    r2->d[3] = a->d[7];
    r2->d[4] = 0;
    r2->d[5] = 0;
    r2->d[6] = 0;
    r2->d[7] = 0;
}
#endif

secp256k1_inline static int secp256k1_scalar_eq(const secp256k1_scalar_t *a, const secp256k1_scalar_t *b) {
    return ((a->d[0] ^ b->d[0]) | (a->d[1] ^ b->d[1]) | (a->d[2] ^ b->d[2]) | (a->d[3] ^ b->d[3]) | (a->d[4] ^ b->d[4]) | (a->d[5] ^ b->d[5]) | (a->d[6] ^ b->d[6]) | (a->d[7] ^ b->d[7])) == 0;
}

secp256k1_inline static void secp256k1_scalar_mul_shift_var(secp256k1_scalar_t *r, const secp256k1_scalar_t *a, const secp256k1_scalar_t *b, unsigned int shift) {
    uint32_t l[16];
    unsigned int shiftlimbs;
    unsigned int shiftlow;
    unsigned int shifthigh;
    verify_check(shift >= 256);
    secp256k1_scalar_mul_512(l, a, b);
    shiftlimbs = shift >> 5;
    shiftlow = shift & 0x1f;
    shifthigh = 32 - shiftlow;
    r->d[0] = shift < 512 ? (l[0 + shiftlimbs] >> shiftlow | (shift < 480 && shiftlow ? (l[1 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[1] = shift < 480 ? (l[1 + shiftlimbs] >> shiftlow | (shift < 448 && shiftlow ? (l[2 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[2] = shift < 448 ? (l[2 + shiftlimbs] >> shiftlow | (shift < 416 && shiftlow ? (l[3 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[3] = shift < 416 ? (l[3 + shiftlimbs] >> shiftlow | (shift < 384 && shiftlow ? (l[4 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[4] = shift < 384 ? (l[4 + shiftlimbs] >> shiftlow | (shift < 352 && shiftlow ? (l[5 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[5] = shift < 352 ? (l[5 + shiftlimbs] >> shiftlow | (shift < 320 && shiftlow ? (l[6 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[6] = shift < 320 ? (l[6 + shiftlimbs] >> shiftlow | (shift < 288 && shiftlow ? (l[7 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[7] = shift < 288 ? (l[7 + shiftlimbs] >> shiftlow)  : 0;
    if ((l[(shift - 1) >> 5] >> ((shift - 1) & 0x1f)) & 1) {
        secp256k1_scalar_add_bit(r, 0);
    }
}

#endif
