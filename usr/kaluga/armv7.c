/**
 * \file
 * \brief ARMv7 arch specfic code
 */

/*
 * Copyright (c) 2013, 2016 ETH Zurich.
 * Copyright (c) 2015, Hewlett Packard Enterprise Development LP.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <hw_records_arch.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish_kpi/platform.h>
#include <if/monitor_blocking_defs.h>
#include <maps/vexpress_map.h>
#include <barrelfish/sys_debug.h>

#include <skb/skb.h>
#include <octopus/getset.h>

#include "kaluga.h"

static void start_driverdomain(char* module_name, char* record) {
    struct module_info* mi = find_module("driverdomain");
    struct driver_argument arg;
    init_driver_argument(&arg);
    arg.module_name = module_name;
    if (mi != NULL) {
        errval_t err = mi->start_function(0, mi, record, &arg);
        assert(err_is_ok(err));
    } else {
        debug_printf("driverdomain not found!\n");
    }
}

static errval_t omap44xx_startup(void)
{
    errval_t err;

    err = init_device_caps_manager();
    assert(err_is_ok(err));

    //start_driverdomain("pl390_dist", "pl1390_dist {}");
    start_driverdomain("fdif", "fdif {}");
    start_driverdomain("sdma", "sdma {}");
    start_driverdomain("mmchs", "mmchs { dep1: 'cm2', dep2: 'twl6030' }");

    struct module_info* mi = find_module("prcm");
    if (mi != NULL) {
        err = mi->start_function(0, mi, "hw.arm.omap44xx.prcm {}", NULL);
        assert(err_is_ok(err));
    }

    mi = find_module("serial");
    if (mi != NULL) {
        err = mi->start_function(0, mi, "hw.arm.omap44xx.uart {}", NULL);
        assert(err_is_ok(err));
    }

    mi = find_module("usb_manager");
    if (mi != NULL) {
#define USB_ARM_EHCI_IRQ 109
        char *buf = malloc(255);
        uint8_t offset = 0;
        mi->cmdargs = buf;
        mi->argc = 3;
        mi->argv[0] = mi->cmdargs + 0;

        snprintf(buf + offset, 255 - offset, "ehci\0");
        offset += strlen(mi->argv[0]) + 1;
        mi->argv[1] = mi->cmdargs + offset;
        snprintf(buf + offset, 255 - offset, "%u\0", 0xC00);
        offset += strlen(mi->argv[1]) + 1;
        mi->argv[2] = mi->cmdargs + offset;
        snprintf(buf+offset, 255-offset, "%u\0", USB_ARM_EHCI_IRQ);

        // XXX Use customized start function or add to module info
        err = mi->start_function(0, mi, "hw.arm.omap44xx.usb {}", NULL);
        assert(err_is_ok(err));
        free(buf);
    }
    return SYS_ERR_OK;
}

/**
 * \brief Starts the gic distributor driver on every core
 *
 */
static errval_t start_gic_dist(coreid_t where){
    errval_t err;

    /* Prepare module and cap */
    struct module_info *mi = find_module("driverdomain_pl390");
    if (mi == NULL) {
        debug_printf("Binary driverdomain_pl390 not found!\n");
        return KALUGA_ERR_MODULE_NOT_FOUND;
    }
    struct capref dist_reg;
    err = get_device_cap(VEXPRESS_MAP_GIC_DIST, VEXPRESS_MAP_GIC_DIST_SIZE,
            &dist_reg);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "Get gic_dist device cap\n");
        return err;
    }
    struct driver_argument arg;
    init_driver_argument(&arg);
    arg.module_name = "pl390_dist";
    struct capref dst = {
        .cnode = arg.argnode_ref,
        .slot = 0
    };
    err = cap_copy(dst, dist_reg);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "cap_copy\n");
        return err;
    }

    /* Create Octopus records for the known cores. */
    char oct_key[128];
    snprintf(oct_key, sizeof(oct_key), "hw.arm.gic.dist.%d {}", where); 
    err = mi->start_function(where, mi, oct_key, &arg);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "couldnt start gic dist on core=%d\n", where);
    }
    return err;
}

static errval_t start_serial(char *name, coreid_t where){
    errval_t err;

    /* Prepare module and cap */
    struct module_info *mi = find_module("driverdomain");
    if (mi == NULL) {
        debug_printf("Binary driverdomain not found!\n");
        return KALUGA_ERR_MODULE_NOT_FOUND;
    }

    // TODO Add caps for all cases
    if(strcmp(name, "serial_kernel") == 0) {
        // No caps needed
    } else {
       assert(!"NYI"); 
    }

    struct driver_argument arg;
    init_driver_argument(&arg);
    arg.module_name = name;
    struct capref irq_src;
    err = slot_alloc(&irq_src);
    assert(err_is_ok(err));

    err = sys_debug_create_irq_src_cap(irq_src, 37, 37);
    assert(err_is_ok(err));

    struct capref irq_dst = {
        .cnode = arg.argnode_ref,
        .slot = 0
    };
    err = cap_copy(irq_dst, irq_src);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "cap_copy\n");
        return err;
    }

    err = mi->start_function(where, mi, "hw.arm.vexpress.uart", &arg);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "couldnt start gic dist on core=%d\n", where);
    }
    return err;
}

static errval_t vexpress_startup(void)
{
    errval_t err;
    err = init_device_caps_manager();
    assert(err_is_ok(err));

    err = start_gic_dist(0);
    assert(err_is_ok(err));

    err = start_serial("serial_kernel", 0);
    assert(err_is_ok(err));

    return SYS_ERR_OK;
}

static errval_t zynq7_startup(void)
{
    errval_t err;

    // Since we don't seem to be able to boot cores on the Zynq yet,
    // we just set all_spawnds_up here. -SG,2016-11-10.
    err = oct_set("all_spawnds_up { iref: 0 }");
    assert(err_is_ok(err));

    /* There's nothing special to do for Zynq (yet). */
    return SYS_ERR_OK;
}

errval_t arch_startup(char * add_device_db_file)
{
    errval_t err = SYS_ERR_OK;

    KALUGA_DEBUG("Kaluga running on ARMv7.\n");

    err = skb_client_connect();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Connect to SKB.");
    }

    // Make sure the driver db is loaded
    err = skb_execute("[device_db].");
    if (err_is_fail(err)) {
        USER_PANIC_SKB_ERR(err, "Device DB not loaded.");
    }
    if(add_device_db_file != NULL){
        err = skb_execute_query("[%s].", add_device_db_file);
        if(err_is_fail(err)){
            USER_PANIC_SKB_ERR(err, "Additional device db file %s not loaded.",
                               add_device_db_file);
        }
    }

    err = skb_execute_query("decoding_net(N),load_net(N).");
    if(err_is_fail(err)){
        debug_printf("############ No decoding net loaded, continue without. ############\n");
        //DEBUG_SKB_ERR(err, "No decoding netloaded.");
    } else {
        err = skb_execute_query("decoding_net_meta(M),load_net(M).");
        if(err_is_fail(err)){
            DEBUG_SKB_ERR(err, "No decoding net metadata loaded.");
        }

        err = skb_execute_query("decoding_net_irq(N),load_net(N).");
        if(err_is_fail(err)){
            DEBUG_SKB_ERR(err, "No irq decoding net loaded.");
        }
        printf("Decoding net irq successfully loaded!\n");

        err = skb_execute_query("decoding_net_irq_meta(M),load_net(M).");
        if(err_is_fail(err)){
            DEBUG_SKB_ERR(err, "No irq decoding net metadata loaded.");
        }
    }


    struct monitor_blocking_binding *m = get_monitor_blocking_binding();
    assert(m != NULL);

    uint32_t arch, platform;
    err = m->rpc_tx_vtbl.get_platform(m, &arch, &platform);
    assert(err_is_ok(err));
    assert(arch == PI_ARCH_ARMV7A);

    uint8_t buf[PI_ARCH_INFO_SIZE];

    struct arch_info_armv7 *arch_info= (struct arch_info_armv7 *)buf;
    size_t buflen;
    err = m->rpc_tx_vtbl.get_platform_arch(m, buf, &buflen);
    assert(buflen == sizeof(struct arch_info_armv7));

    /* Query the SKB for the available cores on this platform - we can't
     * generally discover this on ARMv7. */
    err= skb_execute_query("arm_mpids(L),write(L).");
    if (err_is_fail(err)) {
        USER_PANIC_SKB_ERR(err, "Finding cores.");
    }

    /* Create Octopus records for the known cores. */
    debug_printf("CPU driver reports %u core(s).\n", arch_info->ncores);
    int mpidr_raw;
    struct list_parser_status skb_list;
    skb_read_list_init(&skb_list);
    uint32_t bf_core_id= 0;
    while(skb_read_list(&skb_list, "mpid(%d)", &mpidr_raw)) {
        oct_set(HW_PROCESSOR_ARM_RECORD_FORMAT,
                bf_core_id, 1, bf_core_id, (uint32_t)mpidr_raw, CPU_ARM7);
        bf_core_id++;
    }

    KALUGA_DEBUG("Kaluga: watch_for_cores\n");

    err = watch_for_cores();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Watching cores.");
    }

    KALUGA_DEBUG("Kaluga: wait_for_all_spawnds\n");

    err = wait_for_all_spawnds();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Unable to wait for spawnds failed.");
    }


    switch(platform) {
        case PI_PLATFORM_OMAP44XX:
            debug_printf("Kaluga running on Pandaboard\n");
            return omap44xx_startup();
        case PI_PLATFORM_VEXPRESS:
            debug_printf("Kaluga running on VExpressEMM\n");
            return vexpress_startup();
        case PI_PLATFORM_ZYNQ7:
            debug_printf("Kaluga running on a Zynq7000\n");
            return zynq7_startup();
    }

    return KALUGA_ERR_UNKNOWN_PLATFORM;
}
