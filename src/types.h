/*
 * Copyright 2016,2017 Xiaomi Corporation. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 *
 * Authors:    Yu Bo <yubo@xiaomi.com>
 */
#ifndef __included_types_h__ 
#define __included_types_h__ 

typedef double f64;
typedef float f32;

typedef char i8;
typedef short i16;
typedef unsigned char u8;
typedef unsigned short u16;

#if defined(i386)
typedef int i32;
typedef long long i64;
typedef unsigned int u32;
typedef unsigned long long u64;
#elif defined(__x86_64__)
typedef int i32;
typedef long i64;
typedef unsigned int u32;
typedef unsigned long u64;
#define log2_uword_bits 6
#else
#error "can't define types"
#endif

/* Default to 32 bit machines with 32 bit addresses. */
#ifndef log2_uword_bits
#define log2_uword_bits 5
#endif

/* #ifdef's above define log2_uword_bits. */
#define uword_bits (1 << log2_uword_bits)

/* Word types. */
#if uword_bits == 64
/* 64 bit word machines. */
typedef i64 word;
typedef u64 uword;
#else
/* 32 bit word machines. */
typedef i32 word;
typedef u32 uword;
#endif


#endif
