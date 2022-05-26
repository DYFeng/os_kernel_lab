#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* global segment number */
#define SEG_KTEXT   1
#define SEG_KDATA   2
#define SEG_UTEXT   3
#define SEG_UDATA   4
#define SEG_TSS     5

/* global descrptor numbers */
#define GD_KTEXT    ((SEG_KTEXT) << 3)      // kernel text
#define GD_KDATA    ((SEG_KDATA) << 3)      // kernel data
#define GD_UTEXT    ((SEG_UTEXT) << 3)      // user text
#define GD_UDATA    ((SEG_UDATA) << 3)      // user data
#define GD_TSS      ((SEG_TSS) << 3)        // task segment selector

#define DPL_KERNEL  (0)
#define DPL_USER    (3)

#define KERNEL_CS   ((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS   ((GD_KDATA) | DPL_KERNEL)
#define USER_CS     ((GD_UTEXT) | DPL_USER)
#define USER_DS     ((GD_UDATA) | DPL_USER)

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * */

/* All physical memory mapped at this address */
#define KERNBASE 0xC0000000
#define KMEMSIZE 0x38000000 // the maximum amount of physical memory, 896MB
#define KERNTOP (KERNBASE + KMEMSIZE)

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT                 0xFAC00000

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

typedef uintptr_t pte_t;
typedef uintptr_t pde_t;

// some constants for bios interrupt 15h AX = 0xE820
#define E820MAX             20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

struct e820map {
    int nr_map;
    struct {
        uint64_t addr; // 系统内存块基地址
        uint64_t size; // 系统内存大小(单位为byte)
        uint32_t type; // 内存类型
                       // 01h    memory, available to OS
                       // 02h    reserved, not available (e.g. system ROM, memory-mapped device)
                       // 03h    ACPI Reclaim Memory (usable by OS after reading ACPI tables)
                       // 04h    ACPI NVS Memory (OS is required to save this memory between NVS sessions)
                       // other  not defined yet -- treat as Reserved

        // __attribute__ ((packed)) 的作用就是告诉编译器取消结构在编译过程中的优化对齐
        // 按照实际占用字节数进行对齐，是GCC特有的语法
        // 因为之前BIOS内存探测后放到内存的结构就是没有对齐的，所以我们也不应该对齐
    } __attribute__((packed)) map[E820MAX]; // 这是定义了一个E820MAX=20大的数组
};

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as phyical address.
 * */
/**
 * @brief 表示一物理幀
 */
struct Page {
    // 可能会有多个线性地址的页对应这一物理幀
    int ref;                        // page frame's reference counter

    // 我们要注意第0bit和第1bit，需要两个属性才能描述一幀
    // 第0bit（reserved位）:
    // 第0bit等于1，说明是被保留的幀，不能放到空闲页链表中，即这样的幀不是空闲幀，不能动态分配与释放。
    // 比如目前内核代码占用的空间就属于这样“被保留”的幀。
    // 第0bit等于1，则是可以被分配和释放的
    //
    // 第1bit（property位）
    // 第1bit==1：此Page是Head Page（头一页），并且这个内存块可用
    // 第1bit==0：
    //  - 此Page不是Head Page（头一页）
    //  - 此Page是Head Page（头一页），说明这个内存块已经被分配出去了，不能被再二次分配。注意只是不能被分配，但是可以被引用
    uint32_t flags;                 // array of flags that describe the status of the page frame

    // 这个属性只会被一个Page用到。这个Page比较特殊，是这个连续内存空闲块地址最小的一页（即头一页， Head Page）。
    // 连续内存空闲块利用这个页的成员变量property来记录在此块内的空闲页的个数。
    // 这里取的名字property也不是很直观，原因与上面类似，在不同的页分配算法中，property有不同的含义。
    unsigned int property;          // the num of free block, used in first fit pm manager

    // 需要依附的链表节点
    list_entry_t page_link;         // free list link
};

/* Flags describing the status of a page frame */
#define PG_reserved \
    0 // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0
#define PG_property \
    1 // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and
      // can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this
      // Page and the memory block is alloced. Or this Page isn't the head page.

// 注意了，下面一堆宏都是设置Page.flags的，并不是设置Page.property的
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

// convert list entry to page
#define le2page(le, member) to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;         // the list header
    // 注意这个并不是记录链表有多少个元素，而是记录有多少页在链表里，可能一个链表节点就代表了很多页
    unsigned int nr_free;           // # of free pages in this free list
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

