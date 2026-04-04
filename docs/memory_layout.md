# memory layout

## virtual address space

```
0xFFFFFFFFFFFFFFFF  +-----------------------+
                    |  (unmapped)           |
0xFFFFFFFF80200000  +-----------------------+
                    |  kernel heap          |  grows upward, managed by heap.c
0xFFFFFFFF80100000  +-----------------------+
                    |  kernel image         |  .text, .rodata, .data, .bss
0xFFFFFFFF80000000  +-----------------------+  <- kernel virtual base (-2GB)
                    |  (unmapped gap)       |
0xFFFF800100000000  +-----------------------+
                    |  HHDM                 |  full physical memory mapped 1:1
0xFFFF800000000000  +-----------------------+  <- HHDM base (from limine)
                    |  (non-canonical)      |
0x0000800000000000  +-----------------------+  <- end of userspace
                    |  userspace stack      |  grows downward from top
0x00007FFFFFFFFFFF  |                       |
                    |  (unmapped)           |
0x0000100000000000  +-----------------------+
                    |  userspace heap       |  brk/mmap region
0x0000000010000000  +-----------------------+
                    |  userspace ELF        |  loaded by elf_loader.c
0x0000000000400000  +-----------------------+
                    |  null guard page      |
0x0000000000000000  +-----------------------+
```

## physical memory

the PMM manages all physical memory as 4KB frames. the bitmap is stored in the first usable region large enough to hold it. the HHDM allows the kernel to access any physical address by adding the HHDM offset:

```c
void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}
```

## page table layout

nv0ken uses standard x86_64 4-level paging with 4KB pages.

```
CR3 → PML4 (512 entries × 8 bytes = 4KB)
         └→ PDPT (512 entries × 8 bytes = 4KB)
               └→ PD   (512 entries × 8 bytes = 4KB)
                    └→ PT   (512 entries × 8 bytes = 4KB)
                          └→ 4KB physical page
```

each virtual address is decoded as:

```
bits 63:48  sign extension (must match bit 47)
bits 47:39  PML4 index (9 bits)
bits 38:30  PDPT index (9 bits)
bits 29:21  PD index   (9 bits)
bits 20:12  PT index   (9 bits)
bits 11:0   page offset (12 bits)
```

## kernel linker sections

```
.limine_requests    limine request structs (scanned by bootloader)
.text               executable code
.rodata             read-only data
.data               initialised data
.bss                zero-initialised data (zeroed by boot_entry.asm)
```

the linker script places `.limine_requests` first so limine can scan from the kernel load address.

## per-process address space

each process gets its own PML4. the upper half (kernel) entries are shared — a pointer to the kernel PML4 entries is copied into every new process PML4 on creation. the lower half (userspace) is private. `vmm_clone` deep-copies the lower half page tables when forking.

## stack layout

```
kernel stack    16KB per thread, allocated from kernel heap
user stack      starts at 0x00007FFFFFFFE000, grows downward
                initial RSP is set by elf_loader.c before jumping to _start
```

## memory map types (from limine)

```
0  USABLE                    available for PMM
1  RESERVED                  do not use
2  ACPI_RECLAIMABLE          can reclaim after ACPI init
3  ACPI_NVS                  must preserve
4  BAD_MEMORY                do not use
5  BOOTLOADER_RECLAIMABLE    can reclaim after boot
6  KERNEL_AND_MODULES        kernel image + initrd
7  FRAMEBUFFER               do not map as normal memory
```

PMM only adds frames of type USABLE to the free bitmap. type 5 (BOOTLOADER_RECLAIMABLE) is added after boot is complete.