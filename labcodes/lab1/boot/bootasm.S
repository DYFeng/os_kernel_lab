# 汇编文件也能用include，原理跟c是一样的，只是做了一个内容替换而已
#include <asm.h>

# 汇编里的后缀l,w,b分别表示4byte，2byte，1byte，例如movl，movw，movb等


# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8                     # kernel code segment selector
.set PROT_MODE_DSEG,        0x10                    # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
# 汇编代码里面的这些标签，其实代表了一个内存地址，可你的代码开头假如放在了不同的内存地址，这些标签代表的内存地址也不一样
# 所以我们在编译的时候，就需要明确地告诉编译器，我这个汇编代码在内存里的起始地址是多少，这样子我这些标签的真实内存地址才可以相应计算出来，固化到二进制文件里
# 这里，在编译的时候就告诉编译器，start这个标签在内存里地址是0:7c00，你给我计算下面其他标签的实际内存地址吧
.globl start
start:
# .code16 表示编译时，切换到16位模式，下面的代码编译成机器码都是16位模式的
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
    # 使能A20，这样访问1MB内存以上时，就不会发生“回卷”，但是目前还是实模式下
    # 使能的方法是，通过往键盘的P2端口写入特殊指令即可
seta20.1:
    # 读状态寄存器0x64，如果第1bit（0x2）等于1，说明输入寄存器有数据，此时我们不能写入，跳转回seta20.1（死循环）
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1
    # 如果输入寄存器没有数据了，我们才可以继续写入
    # 我们要告诉键盘，我要对你写入数据了（向0x64发送0xd1命令）
    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    # 跟上面一样，等待输入数据缓冲区为空
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2
    # 往输入数据缓冲区写入0xdf = 11011111
    # 不明白为什么cpu也要重置
    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    lgdt gdtdesc
    # 通过设置cr0寄存器的第0位为1，开启32位模式
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
    ljmp $PROT_MODE_CSEG, $protcseg

# .code32 表示编译时，切换到16位模式，下面的代码编译成机器码都是32位模式的
.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    # bootmain 是C函数了，在进入之前，栈其实是没有的，所以我们要设置一个栈底ebp和栈顶esp的初始值
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # If bootmain returns (it shouldn't), loop.
    # 死循环
spin:
    jmp spin

# Bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    # 这里的gdt有三段
    # 第一段是空的段描述符，也就是全部置0
    SEG_NULLASM                                     # null seg
    # 第二段，可以执行（STA_X），可读（STA_R），从0x0开始，大小为4G（0xffffffff）
    # 在gdt表中的偏移为 PROT_MODE_CSEG
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    # 第三段，可以写入（STA_W），应该也可以读取，但不可执行，从0x0开始，大小为4G（0xffffffff）
    # 在gdt表中的偏移为 PROT_MODE_DSEG
    # 发现第二段和第三段声明的是相同的地址空间
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

# gdt desc就是gdt本身的属性，他的内容是gdt
gdtdesc:
    # gdt的总大小（以byte为单位），gdt里面有很多的segment，一个segment是8byte（64bit），我们有三个segment，8*3-1=23=0x17，为什么要减1呢
    # .word表示两个byte，最大为65535，但是跟8除不尽，所以65535其实表示gtd大小为65535+1=65536，那就跟8除得尽了
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt
