/**
 * \file
 * \brief AMD performance monitoring infrastructure.
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arch/x86/perfmon_amd.h>
#include "cpuid_dev.h"
#include "ia32_dev.h"

static struct cpuid_t mycpuid;
static struct ia32_t ia32_0, ia32_1, ia32_2, ia32_3;

void perfmon_amd_init(void)
{
    cpuid_initialize(&mycpuid);
    ia32_initialize(&ia32_0);
    ia32_initialize(&ia32_1);
    ia32_initialize(&ia32_2);
    ia32_initialize(&ia32_3);
}

bool perfmon_amd_supported(void)
{
    union {
	char str[4];
	uint32_t w;
    } vendor;

    // XXX: Klunky but the only way I got this to work...
    vendor.w = cpuid_vendor0_rd(&mycpuid);
    if(strncmp(vendor.str, "Auth", 4)) {
	return false;
    }
    vendor.w = cpuid_vendor1_rd(&mycpuid);
    if(strncmp(vendor.str, "enti", 4)) {
	return false;
    }
    vendor.w = cpuid_vendor2_rd(&mycpuid);
    if(strncmp(vendor.str, "cAMD", 4)) {
	return false;
    }

    return true;
}

void perfmon_amd_measure_stop(uint8_t idx)
{
    ia32_amd_perfevtsel_t sel = {
	.en = 0
    };
    switch(idx) {
    case 0:
        ia32_amd_perfevtsel0_wr(&ia32_0, sel);
        break;
    case 1:
        ia32_amd_perfevtsel1_wr(&ia32_1, sel);
        break;
    case 2:
        ia32_amd_perfevtsel2_wr(&ia32_2, sel);
        break;
    case 3:
        ia32_amd_perfevtsel3_wr(&ia32_3, sel);
        break;
    }
}

void perfmon_amd_measure_start(uint16_t event, uint8_t umask, bool os, 
                               uint8_t idx, bool intr)
{
    ia32_amd_perfevtsel_t sel = {
	.evsel = event & 0xff,
	.umask = umask,
	.usr = 1,
	.os = os ? 1 : 0,
        .intr = intr ? 1 : 0,
	.en = 1,
	.evsel_hi = (event >> 8) & 0xf
    };
    switch(idx) {
    case 0:
        ia32_amd_perfevtsel0_wr(&ia32_0, sel);
        break;
    case 1:
        ia32_amd_perfevtsel1_wr(&ia32_1, sel);
        break;
    case 2:
        ia32_amd_perfevtsel2_wr(&ia32_2, sel);
        break;
    case 3:
        ia32_amd_perfevtsel3_wr(&ia32_3, sel);
        break;
    }
}

uint64_t perfmon_amd_measure_read(uint8_t idx)
{
    switch(idx) {
    case 0:
        return ia32_perfctr0_rd(&ia32_0);
    case 1:
        return ia32_perfctr1_rd(&ia32_1);
    case 2:
        return ia32_perfctr2_rd(&ia32_2);	    
    case 3:
        return ia32_perfctr3_rd(&ia32_3);	    
    }
    return 0;
}


void perfmon_amd_measure_write(uint64_t val, uint8_t idx)
{
    switch(idx) {
    case 0:
        ia32_perfctr0_wr(&ia32_0, val);
        break;
    case 1:
        ia32_perfctr1_wr(&ia32_1, val);
        break;
    case 2:
        ia32_perfctr2_wr(&ia32_2, val);
        break;
    case 3:
        ia32_perfctr3_wr(&ia32_3, val);
        break;
    }
}

