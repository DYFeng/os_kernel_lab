#include <defs.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE        512
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space

/* waitdisk - wait for disk ready */
static void
waitdisk(void) {
    // 0x1F7: 状态和命令寄存器。操作时先给命令，再读取，如果不是忙状态就从0x1f0端口读数据
    // 读取0x1F7上的高两位 xx00 0000，只有xx=01时（0100 0000），硬盘才算准备好
    while ((inb(0x1F7) & 0xC0) != 0x40) /* do nothing */
        ;
}

/* readsect - read a single sector at @secno into @dst */
/**
 * @brief 从硬盘读取扇区到 @dst
 *
 * 1. 等待磁盘准备好
 * 2. 发出读取扇区的命令
 * 3. 等待磁盘准备好
 * 4. 把磁盘扇区数据读到指定内存
 * @param dst
 * @param secno
 */
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    // 1. 等待磁盘准备好
    waitdisk();

    // 0x1f2	要读写的扇区数，每次读写前，你需要表明你要读写几个扇区。最小是1个扇区，一个扇区512byte
    // 0x1f3	如果是LBA模式，就是LBA参数的0-7位
    // 0x1f4	如果是LBA模式，就是LBA参数的8-15位
    // 0x1f5	如果是LBA模式，就是LBA参数的16-23位
    // 0x1f6	第0~3位：如果是LBA模式就是24-27位 第4位：为0主盘；为1从盘
    // 0x1f7	状态和命令寄存器。操作时先给命令，再读取，如果不是忙状态就从0x1f0端口读数据

    // 只读写一个扇区
    outb(0x1F2, 1); // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20); // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    // 0x1f0	读数据，当0x1f7不为忙状态时，可以读。
    insl(0x1F0, dst, SECTSIZE / 4);
}

/**
 * @brief 从硬盘中读取数据
 * @param va 读取的数据放置在内存哪里
 * @param count 要读取多少byte
 * @param offset 在第一个扇区后面，从多少byte处读起。注意offset=0并不是读取第一个扇区，而是第二个。
 */
static void
readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count;

    // 硬盘只能一个个扇区的读，假如你想要从扇区中间的数据开始读起，那你得把这整个扇区都读取，然后从中间挑
    // 例如我们想要从offset=520开始读起，一个扇区是512大小，所以得把第二个扇区全部读出来才行，也是从512读起，而不是你期望的520
    // 找到离offset最近的一个扇区，从这个扇区开始读起，读取的起点为va
    // round down to sector boundary
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno++) {
        readsect((void *)va, secno);
    }
}

/* bootmain - the entry of bootloader */
/**
 * @brief 此时已经是保护模式下的
 */
void
bootmain(void) {
    // read the 1st page off disk
    // 从硬盘中从第二个扇区（包括）开始，读取八个扇区的内容到内存ELFHDR
    // ELFHDR就在硬盘上的第二个扇区
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // elfhdr这个结构体的命名很奇怪，要有个e_的前缀，跟实验书里的不一样
    // is this a valid ELF?
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // 取出program header的开头地址
    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    // 从program header的数量可以计算出结尾地址
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph++) {
        // va指示了这个program segment应该放到内存哪里，所以我们要把他从硬盘（从第二个扇区开始偏移ph->p_offset）读到内存ph->p_va
        // p_offset是相对于ELFHDR的offset
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
    // 这种写法应该来源于 MIT 的 xv6 ，在这里可以看到，解析 ELF
    // 失败后使用汇编代码进行了一样的操作。在注释中提到了它是针对于 Bochs 模拟器起作用的，作用大概是进行调试。
    // Bochs模拟器是一个 i386 模拟器
    // 在手册中（https://bochs.sourceforge.io/doc/docbook/development/debugger-advanced.html）找到了有关 0x8A00
    // 端口的相关使用方法，也介绍了写入 0x8A00,0x8AE0 各是什么含义，有兴趣的话可以通过 Bochs 模拟器运行 uCore
    // 看看会如何。 因此结论是，这两行代码对于 Qemu 来说并没有用(从 Qemu 源代码中也未找到对应的 PIO 映射)，它是在 Bochs
    // 中调试用的。
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

