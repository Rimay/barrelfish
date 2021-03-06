/**
 * \file
 * \brief Standard headers for kernel code.
 *
 * All C source in the kernel should include this file first.
 * This file should contain only definitions and prototypes that are
 * required for the majority of kernel code.
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __KERNEL_H
#define __KERNEL_H

#include <assert.h>
#include <stddef.h>
#include <stdio.h> // printf for debug
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <barrelfish_kpi/types.h>
#include <barrelfish_kpi/cpu.h>
#include <barrelfish_kpi/registers_arch.h>
#include <errors/errno.h>
#include <bitmacros.h>
#include <debug.h>
#include <offsets.h> /* XXX */
#include <schedule.h>
#include <logging.h>

extern const char *kernel_command_line;
extern coreid_t my_core_id;

bool arch_core_is_bsp(void);

/*
 * Utility macros.
 * FIXME: should we move these elsewhere?
 */

/** Macro to return the number of entries in a statically-allocated array. */
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

/// Round up n to the next multiple of size
#define ROUND_UP(n, size)           ((((n) + (size) - 1)) & (~((size) - 1)))

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

/**
 * \brief Align block at base_addr with size n to power of two
 * alignable at its size
 *
 * Compute the highest exponent x of n so that 2^x \< n _and_
 * the base_addr is aligned at its size.
 * For example: n = 20, base_addr = 4 => size can be 1, 2 or 4.
 * Biggest possible block is 4 => x = 2
 *
 * \param n         Size of the block to split in bytes
 * \param base_addr Base address of the block to split
 *
 * \return Highest exponent (bits) for the blocksize to use next
 */

/// Computes the floor of log_2 of the given number
static inline uint8_t log2flr(uintptr_t num)
{
    uint8_t l = 0;
    uintptr_t n;
    for (n = num; n > 1; n >>= 1, l++);
    return l;
}

/// Computes the ceiling of log_2 of the given number
static inline uint8_t log2cl(uintptr_t num)
{
    uint8_t l = log2flr(num);
    if (num == ((uintptr_t)1) << l) { /* fencepost case */
        return l;
    } else {
        return l + 1;
    }
}

static inline int bitaddralign(size_t n, lpaddr_t base_addr)
{
    int exponent = sizeof(size_t) * NBBY - 1;

    if(n == 0) {
        return 0;
    }

    while ((exponent > 0) && ((base_addr % (1UL << exponent)) != 0)){
        exponent--;
    }
    return((1UL << exponent) > n ? log2flr(n) : exponent);
}

void wait_cycles(uint64_t duration);
void kernel_startup_early(void);
void kernel_startup(void) __attribute__ ((noreturn));

/**
 * kernel timeslice in system ticks
 */
extern systime_t kernel_timeslice;

/**
 * command-line option for kernel timeslice in milliseconds
 */
extern unsigned int config_timeslice;

/**
 * variable for gating timer interrupts.
 */
extern bool kernel_ticks_enabled;


extern lvaddr_t kernel_trace_buf;

extern struct capability monitor_ep;

/*
 * Variant based on Padraig Brady's implementation
 * http://www.pixelbeat.org/programming/gcc/static_assert.html
 */
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)

/* These can't be used after statements in c89. */
#ifdef __COUNTER__
  /* Microsoft compiler. */
  #define STATIC_ASSERT(e,m) \
    enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(!!(e)) }
#else
  /* This can't be used twice on the same line so ensure if using in headers
   * that the headers are not included twice (by wrapping in #ifndef...#endif)
   * Note it doesn't cause an issue when used on same line of separate modules
   * compiled with gcc -combine -fwhole-program.  */
  #define STATIC_ASSERT(e,m) \
    enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }
#endif

#define STATIC_ASSERT_SIZEOF(tname,n)                   \
    STATIC_ASSERT(sizeof(tname) == n,                   \
        ASSERT_CONCAT("Size mismatch:", tname)          \
        )

#define sa_offsetof(x,y) ((size_t)(((void*)&(((x*)0)->y)) - (void*)(x*)0))

#define STATIC_ASSERT_OFFSETOF(tname, field, offset)    \
    STATIC_ASSERT(sa_offsetof(tname, field) == offset,    \
        ASSERT_CONCAT("Offset mismatch:", field)        \
        )

#endif // __KERNEL_H
