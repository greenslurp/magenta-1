// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2015-2016 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <arch/arm64/mmu.h>
#include <kernel/vm/arch_vm_aspace.h>
#include <magenta/compiler.h>
#include <mxtl/canary.h>

class ArmArchVmAspace final : public ArchVmAspaceInterface {
public:
    ArmArchVmAspace() {}
    virtual ~ArmArchVmAspace();

    status_t Init(vaddr_t base, size_t size, uint mmu_flags) override;

    status_t Destroy() override;

    // main methods
    status_t Map(vaddr_t vaddr, paddr_t paddr, size_t count,
                 uint mmu_flags, size_t* mapped) override;

    status_t Unmap(vaddr_t vaddr, size_t count, size_t* unmapped) override;

    status_t Protect(vaddr_t vaddr, size_t count, uint mmu_flags) override;

    status_t Query(vaddr_t vaddr, paddr_t* paddr, uint* mmu_flags) override;

    vaddr_t PickSpot(vaddr_t base, uint prev_region_mmu_flags,
                     vaddr_t end, uint next_region_mmu_flags,
                     vaddr_t align, size_t size, uint mmu_flags) override;

    paddr_t arch_table_phys() const override { return tt_phys_; }

    static void ContextSwitch(ArmArchVmAspace* from, ArmArchVmAspace* to);

private:
    inline bool IsValidVaddr(vaddr_t vaddr) {
        return (vaddr >= base_ && vaddr <= base_ + size_ - 1);
    }

    // Page table management.
    volatile pte_t* GetPageTable(vaddr_t index, uint page_size_shift,
                                 volatile pte_t* page_table);

    status_t AllocPageTable(paddr_t* paddrp, uint page_size_shift);

    void FreePageTable(void* vaddr, paddr_t paddr, uint page_size_shift);

    ssize_t MapPageTable(vaddr_t vaddr_in, vaddr_t vaddr_rel_in,
                         paddr_t paddr_in, size_t size_in, pte_t attrs,
                         uint index_shift, uint page_size_shift,
                         volatile pte_t* page_table, uint asid);

    ssize_t UnmapPageTable(vaddr_t vaddr, vaddr_t vaddr_rel, size_t size,
                           uint index_shift, uint page_size_shift,
                           volatile pte_t* page_table, uint asid);

    int ProtectPageTable(vaddr_t vaddr_in, vaddr_t vaddr_rel_in, size_t size_in,
                         pte_t attrs, uint index_shift, uint page_size_shift,
                         volatile pte_t* page_table, uint asid);

    ssize_t MapPages(vaddr_t vaddr, paddr_t paddr, size_t size, pte_t attrs,
                     vaddr_t vaddr_base, uint top_size_shift, uint top_index_shift,
                     uint page_size_shift, volatile pte_t* top_page_table,
                     uint asid);

    ssize_t UnmapPages(vaddr_t vaddr, size_t size, vaddr_t vaddr_base,
                       uint top_size_shift, uint top_index_shift,
                       uint page_size_shift, volatile pte_t* top_page_table,
                       uint asid);

    status_t ProtectPages(vaddr_t vaddr, size_t size, pte_t attrs,
                          vaddr_t vaddr_base, uint top_size_shift,
                          uint top_index_shift, uint page_size_shift,
                          volatile pte_t* top_page_table, uint asid);

    mxtl::Canary<mxtl::magic("VAAS")> canary_;

    uint16_t asid_ = 0;

    // Pointer to the translation table.
    paddr_t tt_phys_ = 0;
    volatile pte_t* tt_virt_ = nullptr;

    // Upper bound of the number of pages allocated to back the translation
    // table.
    size_t pt_pages_ = 0;

    uint flags_ = 0;

    // Range of address space.
    vaddr_t base_ = 0;
    size_t size_ = 0;
};

using ArchVmAspace = ArmArchVmAspace;
