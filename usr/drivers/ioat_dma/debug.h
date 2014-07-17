/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef XEON_PHI_DEBUG_H_
#define XEON_PHI_DEBUG_H_


/*
 * Debug output switches
 */
#define IODEBUG_ENABLED   1
#define IODEBUG_INT       1
#define IODEBUG_CHAN      1
#define IODEBUG_REQ     1
#define IODEBUG_SVC       1
#define IODEBUG_DEV       1
#define IODEBUG_DESC      1

/*
 * --------------------------------------------------------------------------
 * Debug output generation
 */
#if IODEBUG_ENABLED
#define IODEBUG(x...) debug_printf(x)
#else
#define IODEBUG(x... )
#endif
#if IODEBUG_INT
#define IOINT_DEBUG(x...) IODEBUG("[intr] " x)
#else
#define IOINT_DEBUG(x...)
#endif
#if IODEBUG_CHAN
#define IOCHAN_DEBUG(x...) IODEBUG("[chan.%04x] " x)
#else
#define IOCHAN_DEBUG(x...)
#endif
#if IODEBUG_REQ
#define IOREQ_DEBUG(x...) IODEBUG("[ req] " x)
#else
#define IOREQ_DEBUG(x...)
#endif
#if IODEBUG_SVC
#define IOSVC_DEBUG(x...) IODEBUG("[ svc] " x)
#else
#define IOSVC_DEBUG(x...)
#endif
#if IODEBUG_DEV
#define IODEV_DEBUG(x...) IODEBUG("[ dev] " x)
#else
#define IODEV_DEBUG(x...)
#endif
#if IODEBUG_DESC
#define IODESC_DEBUG(x...) IODEBUG("[desc] " x)
#else
#define IODESC_DEBUG(x...)
#endif

#endif /* XEON_PHI_DEBUG_H_ */
