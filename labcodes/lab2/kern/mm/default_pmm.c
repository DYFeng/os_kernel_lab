#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Min's chinese book "Data Structure -- C programming language"
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    // 初始化free_area
    list_init(&free_list);
    nr_free = 0;
}

void
print_free_area() {
    return;
    list_entry_t *le = &free_list;
    cprintf("======== free page link ========\n");
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        cprintf("head page: %d   length: %d\n", page2ppn(p), p->property);
    }
    cprintf("================================\n");
    int i = 0;
    while ((le = list_prev(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        cprintf("head page: %d   length: %d\n", page2ppn(p), p->property);
        if (i++ > 6) panic("sss\n");
    }
}

/**
 * @brief 把一个非保留的连续物理内存空间（BIOS告诉你的）记到账上
 * 在这之前，为了方便起见，我们把内存里所有物理内存（包括BIOS告诉你可用或者不可用）一次性都建立了页，就放在.bss段上面的内存（Page空间）
 * 在Page空间以下的都是保留内存
 * @param base 这个连续内存空间对应的第一个页，已经做过向上对齐了
 * @param n
 */
static void
default_init_memmap(struct Page *base, size_t n) {
   assert(n > 0);
   struct Page *p = base;
   for (; p != base + n; p++) {
       // 之前把所有的页都设置成“保留”了
       assert(PageReserved(p));
       // 清空flags（“是否保留”，“是否可分配”）和property（“此块内空闲页的个数”）
       p->flags = p->property = 0;
       // 设置为“非保留”，让其可以参与分配和回收
       ClearPageReserved(p);
       // 设置为非HeadPage”
       ClearPageProperty(p);
       set_page_ref(p, 0);
   }
   // 头一页（Head Page）需要记录更多属性
   base->property = n;    // 在此块内的空闲页的个数
   SetPageProperty(base); // 设置为HeadPage

    nr_free += n;
    list_add_before(&free_list, &(base->page_link));
}
//static void
//default_init_memmap(struct Page *base, size_t n) {
//    assert(n > 0);
//    struct Page *p = base;
//    for (; p != base + n; p++) {
//        // 之前把所有的页都设置成“保留”了
//        assert(PageReserved(p));
//        // 清空flags（“是否保留”，“是否可分配”）和property（“此块内空闲页的个数”）
//        p->flags = p->property = 0;
//        // 设置为“非保留”，让其可以参与分配和回收
//        ClearPageReserved(p);
//        // 设置为非HeadPage”
//        ClearPageProperty(p);
//        set_page_ref(p, 0);
//        list_add_before(&free_list, &(p->page_link));
//    }
//    // 头一页（Head Page）需要记录更多属性
//    base->property = n;    // 在此块内的空闲页的个数
//    SetPageProperty(base); // 设置为HeadPage
//    nr_free += n;
//}

/**
 * @brief 申请n个幀的连续空间
 * @param n
 * @return 成功返回这个连续空间的头幀，失败返回NULL
 */
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        // 遍历Headpage链表，看这块连续内存够不够大
        // 符合条件的把Headpage保存到page
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    // 找到符合条件的Headpage，否则返回NULL
    if (page != NULL) {
        // 如果这块内存区域的幀数，比我们申请的还多，那么我们就要把多余的给返回去
        if (page->property > n) {
            struct Page *p = page + n; // 后面多余部分的起始幀，这一幀将成为这块内存块的HeadPage
            p->property = page->property - n; // 往新HeadPage设置内存块的幀数
            SetPageProperty(p);
            // 往老的HeadPage写入被申请的内存块幀数，这个没什么意义，因为申请者肯定知道自己申请的是多大的内存
            //            page->property = n;
            assert(page2pa(page) < page2pa(p));
            list_add_after(&(page->page_link), &(p->page_link)); // 把新HeadPage加进链表
        }
        // 把Headpage脱离链表
        list_del(&(page->page_link));
        // 如果这块内存区域的幀数刚好就是我们需要申请的，那就全部拿去好了
        nr_free -= n;            // 空闲幀减少n个
        ClearPageProperty(page); // 标记这个内存块已经被分配出去了
    }
    return page;
}
//static struct Page *
//default_alloc_pages(size_t n) {
//    assert(n > 0);
//    if (n > nr_free) {
//        return NULL;
//    }
//    struct Page *page = NULL;
//    list_entry_t *le = &free_list;
//    while ((le = list_next(le)) != &free_list) {
//        // 遍历链表，查找Headpage节点，查看节点信息就知道这块连续内存够不够大
//        // 把符合条件的Headpage保存到page
//        struct Page *p = le2page(le, page_link);
//        if (p->property >= n) {
//            page = p;
//            break;
//        }
//    }
//
//    // 找到符合条件的Headpage，否则返回NULL
//    if (page != NULL) {
//        for (int i = 0; i < n; ++i) {
//            list_del(le);
//            struct Page *p = le2page(le, page_link);
//            ClearPageProperty(p); // 标记这个内存块已经被分配出去了
//            le = list_next(le);
//        }
//
//        if (page->property > n) {
//            struct Page *p = le2page(le, page_link); // 后面多余部分的起始幀，这一幀将成为这块内存块的HeadPage
//            p->property = page->property - n; // 往新HeadPage设置内存块的幀数
//            SetPageProperty(p);               // 设置为HeadPage
//        }
//        // 如果这块内存区域的幀数刚好就是我们需要申请的，那就全部拿去好了
//        nr_free -= n; // 空闲幀减少n个
//    }
//    return page;
//}

/**
 * @brief 把一个n幀的连续空间内存块全部归还
 * @param base 这个连续空间的头幀
 * @param n
 */
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    // 把这个连续空间的全部幀设置一下
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    // 之前申请的时候，头幀记录的还是之前内存块的大小，现在归还的时候才把实际大小写入
    base->property = n;

    // 归还内存
    // 1. 先插入到合适的位置
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        // 从第一个链表节点遍历到最后一个
        p = le2page(le, page_link);
        if (p + p->property <= base) {
            // 先插入到链表，合并后面再说
            list_add_after(&(p->page_link), &(base->page_link));
            goto merge;
        }
    }
    // 如果遍历完都没找到合适的地方插入，说明你都在他们前面
    list_add_after(&(free_list), &(base->page_link));

    // 2. 再现有的内存块进行合并
merge:
    // 先当你是PageHead
    SetPageProperty(base);

    // 先看看能不能跟后面的合并，如果合并成功，base还是HeadPage
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        //                |现有内存块|
        // |需要合并的内存块|
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            SetPageProperty(base);
            list_del(&(p->page_link));
        }
    }

    //
    le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        // |现有内存块|
        //           |需要合并的内存块|
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
        }
    }

    nr_free += n;
}
//static void
//default_free_pages(struct Page *base, size_t n) {
//    assert(n > 0);
//    struct Page *p = base;
//    // 把这个连续空间的全部幀设置一下，设置为已经被释放
//    for (; p != base + n; p++) {
//        assert(!PageReserved(p) && !PageProperty(p));
//        p->flags = 0;
//        set_page_ref(p, 0);
//    }
//
//
//    list_entry_t *le = &free_list;
//    while ((le = list_next(le)) != &free_list) {
//        // 遍历链表，查找Headpage节点
//        struct Page *p = le2page(le, page_link);
//        if (PageProperty(p)) {
//
//            if (base + base->property == p) {
//                list_pop
//            }
//
//
//
//        }
//    }
//
//    // 之前申请的时候，头幀记录的还是之前内存块的大小，现在归还的时候才把实际大小写入
//    base->property = n;
//    // 设置base为头幀
//    list_entry_t *le = list_next(&free_list);
//    while (le != &free_list) {
//        p = le2page(le, page_link);
//        le = list_next(le);
//        if (base + base->property == p) {
//            base->property += p->property;
//            ClearPageProperty(p);
//            SetPageProperty(base);
//            list_add_before(&(p->page_link), &(base->page_link));
//            list_del(&(p->page_link));
//            break;
//        } else if (p + p->property == base) {
//            p->property += base->property;
//            ClearPageProperty(base);
//            break;
//            base = p;
//            list_del(&(p->page_link));
//        } else if (base + base->property < p) {
//            SetPageProperty(base);
//            list_add_before(&(p->page_link), &(base->page_link));
//            break;
//        }
//    }
//    if (le == &free_list) list_add(&free_list, &(base->page_link));
//    nr_free += n;
//}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

// 物理内存的管理其实很简单，首先你要明白你这个内存管理是为谁服务的，内存管理是为应用程序服务的，而不是为CPU服务的
// 你可以理解为他只是一个数据库，记录了哪段内存可用哪段不可用，注意这个可不可用是相对于应用程序而言的，对于CPU来说都是可以用的
// 所以他只是一个记账本，你银行里的钱其实只是数据库的一个值，不代表你真的把现金放进去
// 内存的申请和释放也只是记在账上，不是说真的把内存从物理上分一块给你，CPU也是不管的
const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

