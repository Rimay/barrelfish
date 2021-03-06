/**
 * \file PCI bus
 *
 * Virtual PCI bus implementation.
 */

/*
 * Copyright (c) 2009, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _PCI_H
#define _PCI_H

#include "lpc.h"

union pci_config_address_word {
    uint32_t raw;
    struct {
         uint32_t mbz : 2;
         uint32_t doubleword : 6;
         uint32_t fnct_nr : 3;
         uint32_t dev_nr : 5;
         uint32_t bus_nr : 8;
         uint32_t res : 7;
         uint32_t enable_conf_space_mapping : 1;
    } __attribute__ ((packed)) d;
} __attribute__ ((packed)) ;

struct pci_device;

typedef void (*pci_device_confspace_write)(struct pci_device *dev,
                                           union pci_config_address_word addr,
                                           enum opsize size, uint32_t val);

typedef void (*pci_device_confspace_read)(struct pci_device *dev,
                                          union pci_config_address_word addr,
                                          enum opsize size, uint32_t *val);

typedef void (*pci_device_mem_write)(struct pci_device *dev,
                                           uint32_t addr, int bar, uint32_t val);

typedef void (*pci_device_mem_read)(struct pci_device *dev,
										  uint32_t addr,
                                          int bar, uint32_t *val);

struct bar_info {
	void *vaddr;  // assigned by the device driver when calling map_device()
	genpaddr_t paddr; // physical base address of device
	size_t bytes;  //size of the bar
};

struct pci_device {
    pci_device_confspace_write  confspace_write;
    pci_device_confspace_read   confspace_read;
    pci_device_mem_read			mem_read;
    pci_device_mem_write		mem_write;
    struct bar_info				bars[6];
    uint8_t                     irq;
    struct lpc                  *lpc;
    void                        *state;
};

struct pci_bus {
    struct pci_device   *device[32];
};

struct pci {
    union pci_config_address_word       address;
    struct pci_bus                      *bus[256];
};

struct pci *pci_new(void);
int pci_handle_pio_write(struct pci *pci, uint16_t port, enum opsize size,
                         uint32_t val);
int pci_handle_pio_read(struct pci *pci, uint16_t port, enum opsize size,
                        uint32_t *val);

int pci_attach_device(struct pci *pci, uint8_t busnr, uint8_t devnr,
                      struct pci_device *device);

#endif
