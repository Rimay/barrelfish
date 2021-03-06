/*
 * Copyright (c) 2015, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <barrelfish/barrelfish.h>
#include <barrelfish_kpi/paging_target.h>
#include <barrelfish/sys_debug.h>

#define NONNULL

/// All flags valid for page access protection from user-space
#define X86_64_PTABLE_ACCESS_MASK \
    (X86_64_PTABLE_EXECUTE_DISABLE | X86_64_PTABLE_USER_SUPERVISOR | \
     X86_64_PTABLE_READ_WRITE)

/// All arch-specific flags valid to be set from user-space
#define X86_64_PTABLE_FLAGS_MASK                                        \
    (X86_64_PTABLE_GLOBAL_PAGE | X86_64_PTABLE_ATTR_INDEX |             \
     X86_64_PTABLE_DIRTY | X86_64_PTABLE_ACCESSED |                     \
     X86_64_PTABLE_CACHE_DISABLED | X86_64_PTABLE_WRITE_THROUGH)

/// Mask out all arch-specific flags except those valid from user-space
#define X86_64_PTABLE_FLAGS(flags)     (flags & X86_64_PTABLE_FLAGS_MASK)

/// Mask out all flags except those for access protection
#define X86_64_PTABLE_ACCESS(flags)    (flags & X86_64_PTABLE_ACCESS_MASK)

/** True if page entry is present in memory */
#define X86_64_IS_PRESENT(entry)                        \
    ((*(uint64_t *)(entry)) & X86_64_PTABLE_PRESENT)

/**
 * Absolute size of virtual address space. This is 48-bit on x86-64
 * currently, which equals 256 TBytes and allows for 512 PML4 slots,
 * each of which can map 512 GBytes.
 */
#define X86_64_VADDR_SPACE_BITS 48
#define X86_64_VADDR_SPACE_SIZE        ((genpaddr_t)1 << X86_64_VADDR_SPACE_BITS)

/**
 * Absolute size of physical address space. This is also 48-bit.
 */
#define X86_64_PADDR_SPACE_BITS 48
#define X86_64_PADDR_SPACE_SIZE        ((genpaddr_t)1 << X86_64_PADDR_SPACE_BITS)

#define X86_64_PHYSADDR_BITS X86_64_PADDR_SPACE_BITS
#define X86_64_PAGING_ENTRY_SIZE 64
#define X86_64_PAGING_AVAIL2_BITS 11
#define X86_64_PAGING_FLAG_BITS 12
#define X86_64_PAGING_LARGE_FLAGE_BITS 21
#define X86_64_PAGING_RESERVED_BITS \
                (X86_64_PAGING_ENTRY_SIZE - X86_64_PHYSADDR_BITS - \
                 X86_64_PAGING_AVAIL2_BITS - 1)
#define X86_64_PAGING_LARGE_BASE_BITS \
                (X86_64_PHYSADDR_BITS - X86_64_PAGING_LARGE_FLAGE_BITS)
#define X86_64_PAGING_BASE_BASE_BITS \
                (X86_64_PHYSADDR_BITS - X86_64_PAGING_FLAG_BITS)

/** \brief Issue WBINVD instruction, invalidating all caches */
static inline void wbinvd(void)
{
    __asm volatile("wbinvd" ::: "memory");
}

static inline void do_full_tlb_flush(void) {
    // XXX: FIXME: Going to reload cr3 to flush the entire TLB.
    // This is inefficient.
    // The current implementation is also not multicore safe.
    // We should only invalidate the affected entry using invlpg
    // and figure out which remote tlbs to flush.
    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3,%0" : "=a" (cr3) : );
    __asm__ __volatile__("mov %0,%%cr3" :  : "a" (cr3));
}

union x86_64_pdir_entry {
    uint64_t raw;
    struct {
        uint64_t        present         :1;
        uint64_t        read_write      :1;
        uint64_t        user_supervisor :1;
        uint64_t        write_through   :1;
        uint64_t        cache_disabled  :1;
        uint64_t        accessed        :1;
        uint64_t        reserved        :3;
        uint64_t        available       :3;
        uint64_t        base_addr       :28;
        uint64_t        reserved2       :12;
        uint64_t        available2      :11;
        uint64_t        execute_disable :1;
    } d;
};
union x86_64_ptable_entry {
    uint64_t raw;
    struct {
        uint64_t        present         :1;
        uint64_t        read_write      :1;
        uint64_t        user_supervisor :1;
        uint64_t        write_through   :1;
        uint64_t        cache_disabled  :1;
        uint64_t        accessed        :1;
        uint64_t        dirty           :1;
        uint64_t        always1         :1;
        uint64_t        global          :1;
        uint64_t        available       :3;
        uint64_t        attr_index      :1;
        uint64_t        reserved        :17;
        uint64_t        base_addr       :10;
        uint64_t        reserved2       :12;
        uint64_t        available2      :11;
        uint64_t        execute_disable :1;
    } huge;
    struct {
        uint64_t        present         :1;
        uint64_t        read_write      :1;
        uint64_t        user_supervisor :1;
        uint64_t        write_through   :1;
        uint64_t        cache_disabled  :1;
        uint64_t        accessed        :1;
        uint64_t        dirty           :1;
        uint64_t        always1         :1;
        uint64_t        global          :1;
        uint64_t        available       :3;
        uint64_t        attr_index      :1;
        uint64_t        reserved        :8;
        uint64_t        base_addr       :X86_64_PAGING_LARGE_BASE_BITS;
        uint64_t        reserved2       :X86_64_PAGING_RESERVED_BITS;
        uint64_t        available2      :11;
        uint64_t        execute_disable :1;
    } large;
    struct {
        uint64_t        present         :1;
        uint64_t        read_write      :1;
        uint64_t        user_supervisor :1;
        uint64_t        write_through   :1;
        uint64_t        cache_disabled  :1;
        uint64_t        accessed        :1;
        uint64_t        dirty           :1;
        uint64_t        attr_index      :1;
        uint64_t        global          :1;
        uint64_t        available       :3;
        uint64_t        base_addr       :X86_64_PAGING_BASE_BASE_BITS;
        uint64_t        reserved2       :X86_64_PAGING_RESERVED_BITS;
        uint64_t        available2      :11;
        uint64_t        execute_disable :1;
    } base;
};

/**
 * \brief Mask out page attributes.
 *
 * Masks out all attributes and access rights from 'attr' according to
 * 'mask'. This is architecture-specific. On x86-64, except for the
 * execute disable attribute, rights are given by setting a
 * corresponding bit. Thus, setting that bit within 'mask' to zero,
 * masks out the right. For the execute disable bit, the right is
 * masked out when the bit is set, so the mask works the other way
 * around in this case: When the bit is set in 'mask', but not set in
 * 'attr', it will be set in the return value, so mask-out behavior is
 * preserved.
 *
 * \param attr  The page attributes to mask.
 * \param mask  Mask for the page attributes.
 *
 * \return Masked version of 'attr'.
 */
static inline uint64_t paging_x86_64_mask_attrs(uint64_t attr, uint64_t mask)
{
    // First, mask out all "bit-sets-enabled" attributes
    attr &= mask | X86_64_PTABLE_EXECUTE_DISABLE;

    // Now, mask out all "bit-sets-disabled" attributes
    attr |= mask & X86_64_PTABLE_EXECUTE_DISABLE;

    return attr;
}

/** * \brief Maps from page directory entry to page directory/table.
 *
 * Maps page directory or table, based at 'base', from page directory entry
 * pointed to by 'entry'.
 *
 * \param entry Pointer to page directory entry to point from.
 * \param base  Base virtual address of page directory/table to point to.
 */
static inline void paging_x86_64_map_table(union x86_64_pdir_entry *entry,
                                           lpaddr_t base)
{
    union x86_64_pdir_entry tmp;
    tmp.raw = X86_64_PTABLE_CLEAR;

    tmp.d.present = 1;
    tmp.d.read_write = 1;
    tmp.d.user_supervisor = 1;
    tmp.d.base_addr = base >> 12;

    *entry = tmp;
}

/**
 * Flags conversion
 */
static inline paging_x86_64_flags_t vregion_to_pmap(vregion_flags_t vregion_flags)
{
    paging_x86_64_flags_t pmap_flags =
        PTABLE_USER_SUPERVISOR | PTABLE_EXECUTE_DISABLE;

    if (!(vregion_flags & VREGION_FLAGS_GUARD)) {
        if (vregion_flags & VREGION_FLAGS_WRITE) {
            pmap_flags |= PTABLE_READ_WRITE;
        }
        if (vregion_flags & VREGION_FLAGS_EXECUTE) {
            pmap_flags &= ~PTABLE_EXECUTE_DISABLE;
        }
        if (vregion_flags & VREGION_FLAGS_NOCACHE) {
            pmap_flags |= PTABLE_CACHE_DISABLED;
        }
    }

    paging_x86_64_flags_t flags = X86_64_PTABLE_USER_SUPERVISOR
                                | X86_64_PTABLE_READ_WRITE
                                | X86_64_PTABLE_EXECUTE_DISABLE;
    // Mask with provided access rights mask
    flags = paging_x86_64_mask_attrs(flags, X86_64_PTABLE_ACCESS(pmap_flags));
    // Add additional arch-specific flags
    flags |= X86_64_PTABLE_FLAGS(flags);
    // Unconditionally mark the page present
    flags |= X86_64_PTABLE_PRESENT;

    return flags;
}

/**
 * \brief Maps a huge page.
 *
 * From huge page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param base          Physical address to map to (will be page-aligned).
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_map_huge(union x86_64_ptable_entry *entry,
                                           lpaddr_t base, uint64_t bitmap)
{
    union x86_64_ptable_entry tmp;
    tmp.raw = X86_64_PTABLE_CLEAR;

    tmp.huge.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.huge.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.huge.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.huge.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.huge.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.huge.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.huge.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.huge.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;
    tmp.huge.always1 = 1;
    tmp.huge.base_addr = base >> X86_64_HUGE_PAGE_BITS;

    *entry = tmp;
}

/**
 * \brief Maps a large page.
 *
 * From large page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param base          Physical address to map to (will be page-aligned).
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_map_large(union x86_64_ptable_entry *entry,
                                           lpaddr_t base, uint64_t bitmap)
{
    union x86_64_ptable_entry tmp;
    tmp.raw = X86_64_PTABLE_CLEAR;

    tmp.large.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.large.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.large.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.large.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.large.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.large.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.large.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.large.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;
    tmp.large.always1 = 1;
    tmp.large.base_addr = base >> 21;

    *entry = tmp;
}

/**
 * \brief Maps a normal (small) page.
 *
 * From small page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param base          Physical address to map to (will be page-aligned).
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_map(union x86_64_ptable_entry * NONNULL entry,
                                     lpaddr_t base, uint64_t bitmap)
{
    union x86_64_ptable_entry tmp;
    tmp.raw = X86_64_PTABLE_CLEAR;

    tmp.base.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.base.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.base.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.base.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.base.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.base.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.base.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.base.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;
    tmp.base.base_addr = base >> 12;

    *entry = tmp;
}


/**
 * \brief Modify flags of a huge page.
 *
 * From small page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_modify_flags_huge(union x86_64_ptable_entry * entry,
                                                   uint64_t bitmap)
{
    union x86_64_ptable_entry tmp = *entry;

    tmp.huge.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.huge.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.huge.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.huge.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.huge.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.huge.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.huge.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.huge.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;
    tmp.huge.always1 = 1;

    *entry = tmp;
}


/**
 * \brief Modify flags of a large page.
 *
 * From small page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_modify_flags_large(union x86_64_ptable_entry *entry,
                                              uint64_t bitmap)
{
    union x86_64_ptable_entry tmp = *entry;

    tmp.large.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.large.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.large.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.large.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.large.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.large.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.large.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.large.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;
    tmp.large.always1 = 1;

    *entry = tmp;
}

/**
 * \brief Modify flags of a normal (small) page.
 *
 * From small page table entry, pointed to by 'entry', maps physical address
 * 'base' with page attribute bitmap 'bitmap'.
 *
 * \param entry         Pointer to page table entry to map from.
 * \param bitmap        Bitmap to apply to page attributes.
 */
static inline void paging_x86_64_modify_flags(union x86_64_ptable_entry * NONNULL entry,
                                              uint64_t bitmap)
{
    union x86_64_ptable_entry tmp = *entry;

    tmp.base.present = bitmap & X86_64_PTABLE_PRESENT ? 1 : 0;
    tmp.base.read_write = bitmap & X86_64_PTABLE_READ_WRITE ? 1 : 0;
    tmp.base.user_supervisor = bitmap & X86_64_PTABLE_USER_SUPERVISOR ? 1 : 0;
    tmp.base.write_through = bitmap & X86_64_PTABLE_WRITE_THROUGH ? 1 : 0;
    tmp.base.cache_disabled = bitmap & X86_64_PTABLE_CACHE_DISABLED ? 1 : 0;
    tmp.base.attr_index = bitmap & X86_64_PTABLE_ATTR_INDEX ? 1 : 0;
    tmp.base.global = bitmap & X86_64_PTABLE_GLOBAL_PAGE ? 1 : 0;
    tmp.base.execute_disable = bitmap & X86_64_PTABLE_EXECUTE_DISABLE ? 1 : 0;

    *entry = tmp;
}

static inline void paging_unmap(union x86_64_ptable_entry * NONNULL entry)
{
    entry->raw = X86_64_PTABLE_CLEAR;
}

errval_t install_user_managed_pdpt(genvaddr_t *base, void **ptable);
