/* Copyright (c) Victor Roemer, 2013. All rights reserved.
 * See LICENSE in the root source directory for details. */

/*
 * Copyright (c) 1989, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by Paul Vixie.
 * See `bitstring.h' in FreeBSD/Apple/etc...
 */

#include "cdefs.h"

#ifndef BITMAP_H_QXJ7R9PF
#define BITMAP_H_QXJ7R9PF

typedef unsigned char bitmap_t;

/* byte of the bitmap bit is in */
static inline int bit_byte(int bit)
{
    return ((bit) >> 3);
}

/* mask for the bit within its byte */
static inline int bit_mask(int bit)
{
    return (1 << ((bit) & 7));
}

/* bytes in a bitmap of nbits bits */
static inline int bitmap_size(int nbits)
{
    #define _bitmap_size(nbits) \
        ((((nbits) - 1) >> 3) + 1)
 
    return _bitmap_size(nbits);
}

/* allocate a bitmap */
static inline bitmap_t *bit_alloc(int nbits)
{
    return (bitmap_t *)calloc(1, (size_t)bitmap_size(nbits) * sizeof(bitmap_t));
}

/* allocate a bitmap on the stack */
#define bit_decl(name, nbits) \
  (name)[_bitmap_size(nbits)]

/* is bit N of bitmap name set? */
static inline bool bit_test(bitmap_t * restrict name, int bit)
{
    return name[bit_byte(bit)] & bit_mask(bit);
}

/* set bit N of bitmap name */
static inline void bit_set(bitmap_t * restrict name, int bit)
{
    name[bit_byte(bit)] |= bit_mask(bit);
}

/* clear bit N of bitmap name */
static inline void bit_clear(bitmap_t * restrict name, int bit)
{
    name[bit_byte(bit)] &= ~bit_mask(bit);
}


/* clear bits start ... stop in bitmap */
static inline void bit_nclear(bitmap_t * restrict name, int start, int stop)
{
    register bitmap_t * restrict _name = name;
    register int _start = start, _stop = stop;
    register int _startbyte = bit_byte(_start);
    register int _stopbyte = bit_byte(_stop);
    if (_startbyte == _stopbyte) {
        _name[_startbyte] &= ((0xff >> (8 - (_start&0x7))) |
                              (0xff << ((_stop&0x7) + 1)));
    } else {
            _name[_startbyte] &= 0xff >> (8 - (_start&0x7));
            while (++_startbyte < _stopbyte)
                _name[_startbyte] = 0;
            _name[_stopbyte] &= 0xff << ((_stop&0x7) + 1);
    }
}

/* set bits start ... stop in bitmap */
static inline void bit_nset(bitmap_t * restrict name, int start, int stop)
{
    register bitmap_t * restrict _name = name;
    register int _start = start, _stop = stop;
    register int _startbyte = bit_byte(_start);
    register int _stopbyte = bit_byte(_stop);
    if (_startbyte == _stopbyte) {
        _name[_startbyte] |= ((0xff << (_start&0x7)) &
                             (0xff >> (7 - (_stop&0x7))));
    } else {
        _name[_startbyte] |= 0xff << ((_start)&0x7);
        while (++_startbyte < _stopbyte)
            _name[_startbyte] = 0xff;
        _name[_stopbyte] |= 0xff >> (7 - (_stop&0x7));
    }
}

/* find first bit clear in name */
//static inline void bit_ffc(bitmap_t * restrict name, int nbits, int * value)
static inline int bit_ffc(bitmap_t * restrict name, int nbits)
{
    register bitmap_t *_name = name;
    register int _byte, _nbits = nbits;
    register int _stopbyte = bit_byte(_nbits), _value = -1;
    for (_byte = 0; _byte < _stopbyte; ++_byte)
        if (_name[_byte] != 0xff) {
            _value = _byte << 3; 
            for (_stopbyte = _name[_byte]; (_stopbyte&0x1);
                ++_value, _stopbyte >>= 1);
            break;
        }
    //*(value) = _value;
    return _value;
}

/* find first bit set in name */
//static inline void bit_ffs(bitmap_t *name, int nbits, int * value)
static inline int bit_ffs(bitmap_t *name, int nbits)
{
    register bitmap_t *_name = name;
    register int _byte, _nbits = nbits;
    register int _stopbyte = bit_byte(_nbits), _value = -1;
    for (_byte = 0; _byte < _stopbyte; ++_byte)
        if (_name[_byte]) {
            _value = _byte << 3; 
            for (_stopbyte = _name[_byte]; !(_stopbyte&0x1);
                ++_value, _stopbyte >>= 1);
            break;
        }
    //*(value) = _value;
    return _value;
}

#endif /* !BITMAP_H_QXJ7R9PF */
