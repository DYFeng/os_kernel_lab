#include <mmu.h>
#include <memlayout.h>

// 输入分页后地址，输出分页前地址
#define REALLOC(x) (x - KERNBASE)

.text
.globl kern_entry
kern_entry:
    # reload temperate gdt (second time) to remap all physical memory
    # virtual_addr 0~4G=linear_addr&physical_addr -KERNBASE~4G-KERNBASE
    # 根据临时段表重新暂时建立好新的段空间，为进行分页做好准备
    lgdt REALLOC(__gdtdesc)
    movl $KERNEL_DS, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    ljmp $KERNEL_CS, $relocated

relocated:
    # 因为kern_init是C函数了，所以要设置棧顶棧底位置
    # set ebp, esp
    movl $0x0, %ebp
    # the kernel stack region is from bootstack -- bootstacktop,
    # the kernel stack size is KSTACKSIZE (8KB)defined in memlayout.h
    movl $bootstacktop, %esp
    # now kernel stack is ready , call the first C function
    call kern_init

# should never get here
spin:
    jmp spin

# 这里很巧妙地设置棧的位置，他直接在二进制程序里“placehold”棧底地址bootstacktop，还有棧最大只能生长到bootstack位置
# 当二进制文件放到内存时，这空间地址就确定了
.data
.align PGSIZE
    .globl bootstack
bootstack:
    .space KSTACKSIZE
    .globl bootstacktop
bootstacktop:

.align 4
__gdt:
    SEG_NULL

    # 之前bootload的gdt表是从0~0xFFFFFFFF，现在改成了0x40000000~0xFFFFFFFF(1GB~4GB)
    # 这里其实我有疑惑的，我的物理内存明明就没有1GB这么多，你映射到这么高的地方我怎么用
    # 后面又发现上面的relocated=0xc0100019，`ljmp $KERNEL_CS, $relocated`就是跳到0x40000000+0xc0100019=0x100100019的地方执行
    # 又由于我们是32位cpu，所以这个加法其实是会溢出的，其实0x40000000+0xc0100019=0x00100019
    # 这时候就清晰了，我们的elf文件，硬编码了起始地址是3GB，因为我们希望在打开页机制后，把内核空间映射到(3GB~4GB)
    # 但是此时页机制还没打开，所以线性空间跟物理空间是对等映射的，但物理内存可能没有这么大，3GB后面这段空间可能是没有物理内存对应的
    # 这时候设置段基址为-3GB，就可以令到逻辑偏移的3GB~4GB对应到物理内存0GB~1GB（把3GB变0GB），例如我jmp 第二段:0x10偏移，其实是跳到了物理内存0x10处

    # -KERNBASE=0-0xC0000000=0x40000000=-3GB
    SEG_ASM(STA_X | STA_R, - KERNBASE, 0xFFFFFFFF)      # code segment
    SEG_ASM(STA_W, - KERNBASE, 0xFFFFFFFF)              # data segment
__gdtdesc:
    .word 0x17                                          # sizeof(__gdt) - 1
    .long REALLOC(__gdt)

