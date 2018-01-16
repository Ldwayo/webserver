#include <stdio.h>
#include <errno.h>
#include "memlib.c"
#include <stddef.h>
static void *extend_heap(size_t words);    //拓展堆的可用空间，返回原堆顶地址(mem_brk),失败返回NULL
static void *coalesce(void *bp);           //并合bp指向块的前后块,返回并合后的块指针
static void *find_fit(size_t size);        //寻找第一个空间大于size的空闲块，返回其地址,未找到时，返回NULL
static void place(void *bp, size_t asize); //分割find_fit返回的块，创建块结构

//宏定义
#define WSIZE 4     //字的大小
#define DSIZE 8     //双字大小
#define CHUNKSIZE (1 << 12)     //每次拓展堆的大小

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) //将块大小和标志位整合到一个字中，用于存放到头部和脚部

#define GET(p) (*(unsigned int *)(p))              //返回p引用的字
#define PUT(p, val) (*(unsigned int *)(p) = (val)) //将val压入p指向的字

#define GET_SIZE(p) (GET(p) & ~0x7) //返回头或尾部的高29位,即该块的大小
#define GET_ALLOC(p) (GET(p) & 0x1) //返回标志位（与运算）
                                                     //bp块指针指向第一个有效载荷字节
#define HDRP(bp) ((char *)bp - WSIZE)                //返回块的头部，（块指针bp）block pointer
#define FTRP(bp) ((char *)bp + GET_SIZE(bp) - DSIZE) //返回块的尾部，双字对齐，所以是DSIZE

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)bp - WSIZE)) //返回当前块的下一块的块指针
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))   //返回当前块的上一块的块指针

static void *heap_listp; //指向第一个块(序言块)

//接口
int mm_init(void) //初始化，成功返回0，失败返回-1
{
    //mem_init();
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(8, 1));     //序言块头部
    PUT(heap_listp + 2 * WSIZE, PACK(8, 1)); //序言块尾部
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1)); //结尾块
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) //1024
        return -1;
    return 0;
}

static void *extend_heap(size_t words) //拓展堆的可用空间，返回原堆顶地址(mem_brk),失败返回NULL
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; //保持双字对齐
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));       //设置结尾块的头部

    return (void *)bp;
}
void mm_free(void *bp) //释放bp指向块的内存
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp); //合并前后块
}

void *mm_malloc(size_t size) //分配size字节大小的块，返回指向块的指针
{
    size_t asize;       //调整过的size大小
    size_t extendsize;  //扩展的大小
    void *bp;

    if (size == 0)
        return NULL;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)         //寻找合适的块
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);         //如果找不到合适的块，就扩展一个extendsize大小的块
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

static void *coalesce(void *bp)                         //合并bp指向块的前后块,返回合并后的块指针
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //上一块是否分配
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //下一块是否分配
    size_t size;

    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *find_fit(size_t asize) //寻找第一个空间大于size的空闲块，返回其地址,未找到时，返回NULL
{                                   //首次适应算法
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp))&&(asize<=GET_SIZE(HDRP(bp)))) //返回第一块未分配且空间大于size的空闲块
            return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize) //分割find_fit返回的块，创建块结构
{
    size_t bsize = GET_SIZE(HDRP(bp));
    if ((bsize - asize) >= (2 * DSIZE)) //最小块为16字节,分割块
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(bsize - asize, 0));
        PUT(FTRP(bp), PACK(bsize - asize, 0));
    }
    else //不用分割
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}