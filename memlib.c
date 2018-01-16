#include <stdio.h>
#include <stdlib.h>
#define MAX_HEAP 104857600 //最大堆空间大小为100M
static char *mem_heap;     //mem_heap和mem_brk之间的字节表示已分配的虚拟内存
static char *mem_brk;      //mem_brk之后表示未分配的虚拟内存
static char *mem_max_addr;
void memcost(long cost)
{
    int fd;
    char logcostbuf[50];
    (void)sprintf(logcostbuf, "memory cost:%ld字节\n",cost);
    if ((fd = open("memcost.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
    {
        (void)write(fd, logcostbuf, strlen(logcostbuf));
        (void)write(fd, "\n", 1);
        (void)close(fd);
    }
}
void mem_init(void)
{
    mem_heap = (char *)malloc(MAX_HEAP);
    mem_brk = (char *)mem_heap;
    mem_max_addr = (char *)(mem_heap + MAX_HEAP);
    printf("%ld", (mem_brk - mem_heap));
}

void *mem_sbrk(int incr) //请求额外的堆内存
{
    char *old_brk = mem_brk;
    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr))
    {
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1;
    }
    mem_brk += incr;
    memcost(mem_brk-mem_heap);
    return (void *)old_brk;
}