#ifndef _BITSTREAM_MSB_H
#define _BITSTREAM_MSB_H

#include "../streamtypes.h"

/* Simple bitreader for MPEG/standard bit style, in 'most significant byte' (MSB) format.
 * Example: 0x12345678 is read as 78,56,34,12 then each byte's bits.
 * Kept in .h since it's slightly faster (compiler can optimize statics better using default compile flags). */

typedef struct {
    uint8_t* buf;           /* buffer to read/write */
    size_t bufsize;         /* max size */
    size_t b_max;           /* max size in bits */
    uint32_t b_off;         /* current offset in bits inside buffer */
} bitstream_t;


static inline void bm_setup(bitstream_t* bs, uint8_t* buf, size_t bufsize) {
    bs->buf = buf;
    bs->bufsize = bufsize;
    bs->b_max = bufsize * 8;
    bs->b_off = 0;
}

static inline int bm_set(bitstream_t* bs, uint32_t b_off) {
    if (bs->b_off > bs->b_max)
        goto fail;

    bs->b_off = b_off;

    return 1;
fail:
    return 0;
}

static inline int bm_fill(bitstream_t* bs, uint32_t bytes) {
    if (bs->b_off > bs->b_max)
        goto fail;

    bs->bufsize += bytes;
    bs->b_max += bytes * 8;

    return 1;
fail:
    return 0;
}

static inline int bm_skip(bitstream_t* bs, uint32_t bits) {
    if (bs->b_off + bits > bs->b_max)
        goto fail;

    bs->b_off += bits;

    return 1;
fail:
    return 0;
}

static inline int bm_pos(bitstream_t* bs) {
    return bs->b_off;
}

/* Read bits (max 32) from buf and update the bit offset. Order is BE (MSB). */
static inline int bm_get(bitstream_t* ib, uint32_t bits, uint32_t* value) {
    uint32_t shift, pos, val;
    int i, bit_buf, bit_val;

    if (bits > 32 || ib->b_off + bits > ib->b_max)
        goto fail;

    pos = ib->b_off / 8;        /* byte offset */
    shift = ib->b_off % 8;      /* bit sub-offset */

#if 1 //naive approach
    val = 0;
    for (i = 0; i < bits; i++) {
        bit_buf = (1U << (8-1-shift)) & 0xFF;   /* bit check for buf */
        bit_val = (1U << (bits-1-i));           /* bit to set in value */

        if (ib->buf[pos] & bit_buf)             /* is bit in buf set? */
            val |= bit_val;                     /* set bit */

        shift++;
        if (shift % 8 == 0) {                   /* new byte starts */
            shift = 0;
            pos++;
        }
    }
#else //has bugs
    pos = ib->b_off / 8;        /* byte offset */
    shift = ib->b_off % 8;      /* bit sub-offset */
    uint32_t mask = MASK_TABLE[bits];    /* to remove upper in highest byte */

    int left = 0;
    if (bits == 0)
        val = 0;
    else 
        val = ib->buf[pos+0];
    left = 8 - (bits + shift);
    if (bits + shift > 8) {
        val = (val << 8u) | ib->buf[pos+1];
        left = 16 - (bits + shift);
        if (bits + shift > 16) {
            val = (val << 8u) | ib->buf[pos+2];
            left = 32 - (bits + shift);
            if (bits + shift > 24) {
                val = (val << 8u) | ib->buf[pos+3];
                left = 32 - (bits + shift);
                if (bits + shift > 32) {
                    val = (val << 8u) | ib->buf[pos+4]; /* upper bits are lost (shifting over 32) */ TO-DO
                    left = 40 - (bits + shift);
                }
            }
        }
    }
    val = ((val >> left) & mask);
#endif

    *value = val;
    ib->b_off += bits;
    return 1;
fail:
    //VGM_LOG("BITREADER: read fail\n");
    *value = 0;
    return 0;
}

/* Write bits (max 32) to buf and update the bit offset. Order is BE (MSB). */
static inline int bm_put(bitstream_t* ob, uint32_t bits, uint32_t value) {
    uint32_t shift, pos;
    int i, bit_val, bit_buf;

    if (bits > 32 || ob->b_off + bits > ob->b_max)
        goto fail;

    pos = ob->b_off / 8; /* byte offset */
    shift = ob->b_off % 8; /* bit sub-offset */

    for (i = 0; i < bits; i++) {
        bit_val = (1U << (bits-1-i));     /* bit check for value */
        bit_buf = (1U << (8-1-shift)) & 0xFF;   /* bit to set in buf */

        if (value & bit_val)                /* is bit in val set? */
            ob->buf[pos] |= bit_buf;        /* set bit */
        else
            ob->buf[pos] &= ~bit_buf;       /* unset bit */

        shift++;
        if (shift % 8 == 0) {               /* new byte starts */
            shift = 0;
            pos++;
        }
    }

    ob->b_off += bits;
    return 1;
fail:
    return 0;
}

#endif
