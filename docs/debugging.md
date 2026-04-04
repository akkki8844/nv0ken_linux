# debugging nv0ken_linux

## serial output

every `kprintf` call writes to both the framebuffer and COM1 serial. when running in QEMU with `-serial stdio`, all kernel log output appears in your terminal. this is the fastest debug loop — no need to attach GDB for most issues.

```bash
bash tools/run_qemu.sh 2>&1 | tee kernel.log
```

## GDB + QEMU

### launch QEMU in debug mode

```bash
bash tools/run_qemu_debug.sh
```

this adds `-s -S` to the QEMU flags. `-S` freezes the CPU at startup so you can attach GDB before anything runs.

### attach GDB

in a second terminal from the project root:

```bash
gdb -x tools/gdbinit
```

`tools/gdbinit` contains:

```
set architecture i386:x86-64
file build/kernel.elf
target remote localhost:1234
break kmain
continue
```

### useful GDB commands

```
(gdb) break kmain              # break at kernel entry
(gdb) break kprintf            # break every time kernel prints
(gdb) break panic              # catch kernel panics
(gdb) continue                 # run until next breakpoint
(gdb) step                     # step one C line (into functions)
(gdb) next                     # step one C line (over functions)
(gdb) stepi                    # step one instruction
(gdb) info registers           # dump all registers
(gdb) info registers rsp rip   # specific registers
(gdb) x/20gx $rsp              # dump 20 quad-words from stack
(gdb) x/20i $rip               # disassemble 20 instructions at RIP
(gdb) print variable           # print a C variable by name
(gdb) bt                       # backtrace (call stack)
(gdb) list                     # show source around current line
(gdb) layout src               # split view with source
(gdb) layout asm               # split view with assembly
(gdb) layout regs              # show registers panel
```

### breaking on a page fault

```
(gdb) break isr_handler
(gdb) commands
> info registers
> x/20gx $rsp
> bt
> continue
> end
```

## kernel panic

when `panic()` is called the kernel prints a message and halts. the message format is:

```
PANIC: <message>
  file: kernel/mm/pmm.c  line: 47
  rip:  0xFFFFFFFF80001234
  rsp:  0xFFFFFFFF80010FF0
```

the RIP value can be resolved to a symbol:

```bash
nm build/kernel.elf | sort | grep -A1 "80001"
```

or use `addr2line`:

```bash
addr2line -e build/kernel.elf -f 0xFFFFFFFF80001234
```

## disassembly

dump the full kernel disassembly:

```bash
bash tools/objdump_kernel.sh > objdump.txt
grep -n "kmain" objdump.txt
```

dump just one function:

```bash
objdump -d build/kernel.elf | grep -A 40 "<kmain>:"
```

## symbol table

list all kernel symbols sorted by address:

```bash
bash tools/symbols.sh | head -50
```

find where a crash address lands:

```bash
nm build/kernel.elf | sort | awk -v addr=0x80001234 \
    '$1 <= addr {last=$0} END {print last}'
```

## QEMU monitor

press `Ctrl+Alt+2` in the QEMU window to enter the QEMU monitor.

useful monitor commands:

```
(qemu) info mem          # dump page table mappings
(qemu) info registers    # CPU registers
(qemu) info pic          # PIC IRQ state
(qemu) xp /20gx 0x1000  # examine physical memory
(qemu) x /20gx 0xFFFFFFFF80000000  # examine virtual memory
(qemu) stop              # freeze CPU (equivalent to GDB break)
(qemu) cont              # resume
(qemu) quit              # kill QEMU
```

press `Ctrl+Alt+1` to return to the display.

## common issues

**triple fault on boot** — usually a bad GDT or IDT setup. attach GDB, break at `gdt_init` and `idt_init`, step through carefully.

**page fault immediately after paging enabled** — CR3 points to wrong PML4, or kernel not mapped in higher half. check `paging.c` and the linker script.

**kprintf produces garbage** — framebuffer pitch or bpp wrong. print the limine framebuffer fields over serial before initialising the FB driver.

**QEMU hangs with black screen** — boot_entry.asm is not being reached. check that `kernel.elf` is being included in the ISO and the limine.conf path is correct.

**scheduler crashes** — stack overflow or corrupt TSS. check that the kernel stack size is at least 16KB and the TSS RSP0 is set correctly.