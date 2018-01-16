#include <stdio.h>
#include "mmaptest.c"
#define BUFSIZE 8096
#define MAXMMAP 4
#define MIN(x, y) ((x) < (y) ? (x) : (y))
typedef struct xnode
{
    char buffer[BUFSIZE + 1];
    long ret;
    struct xnode *next;
} xnode;
typedef struct filenode
{
    char buffer[BUFSIZE + 1];
    long len;
    int deep;
    int count;
    long ret;
    struct filenode *next;
    struct xnode *xnext;
} filenode;
typedef struct
{
    pthread_mutex_t mutex;
    filenode *front;
    filenode *rear;
    int length;
    int flag;
} filelink;
int Enfilelink_m(filelink *f)
{
    filenode *s;
    s = (filenode *)mm_malloc(sizeof(filenode));
    s->deep = 0;
    s->len = 0;
    s->ret = 0;
    s->count = 0;
    s->xnext = NULL;
    f->rear->next = s;
    f->rear = s;
    f->length++;
}
int initfilelink(filelink *f)
{
    mem_init(); //初始化模型
    mm_init();  //初始化分配器
    f->front = f->rear = (filenode *)mm_malloc(sizeof(filenode));
    pthread_mutex_init(&(f->mutex), NULL);
    f->front->next = NULL;
    f->length = 0;
    f->flag = 0;
    while (f->length < MAXMMAP)
        Enfilelink_m(f);
    return 1;
}
xnode *Enxnode(filenode *s, long ret)
{
    if (s->deep == 0)
    {
        xnode *p = (xnode *)mm_malloc(sizeof(xnode));
        s->xnext = p;
        p->ret = ret;
        p->next = NULL;
        s->deep++;
        return p;
    }
    else
    {
        int d;
        d = s->deep;
        xnode *p;
        p = s->xnext;
        while (--d)
        {
            p = p->next;
        }
        xnode *q = (xnode *)mm_malloc(sizeof(xnode));
        p->next = q;
        p = q;
        p->ret = ret;
        p->next = NULL;
        s->deep++;
        return p;
    }
}
int freenode(filenode *s)
{ //
    xnode *l = NULL;
    xnode *p = s->xnext;
    while (p)
    {
        l = p;
        p = p->next;
        mm_free(l);
        l = NULL;
    }
    s->xnext = NULL;
    s->deep = 0;
    s->len = 0;
    s->ret = 0;
    return 1;
}
filenode *replace(filelink *f)
{
    filenode *s;
    filenode *p;
    s = f->front->next;
    p = s;
    int recount;
    while (s->next != NULL)
    {
        if (p->count > s->count)
        {
            p = s;
        }
        if (p->count == s->count)
        {
            recount++;
            if (recount == MAXMMAP)
            {
                s = f->front;
                int i = f->flag;
                while (i >= 0)
                {
                    s = s->next;
                    i--;
                }
                f->flag = (f->flag++) % MAXMMAP;
                freenode(s); //调用freenode，释放空间
                return s;    //返回被释放的结点，用于存入要换入的结点
            }
        }
        s = s->next;
    }
    p->count = 0;
    freenode(p);
    return p;
}
filenode *search_m(filelink *f, long len)
{
    filenode *s;
    s = f->front->next;
    while (s->len > 0)
    {
        if (s->len == len)
        {
            s->count++;
            return s;
        }
        else
        {
            s = s->next;
            if (s == NULL)
            {
                s = replace(f);
                s->count++;
                return s;
            }
        }
    }
    s->count++;
    return s;
}
int Enfilelink(filenode *s, char *buffer, long len, long ret)
{
    if (s->ret == 8096)
    {
        xnode *p = Enxnode(s, ret);
        memcpy(p->buffer, buffer, ret);
        return 1;
    }
    else
    {
        memcpy(s->buffer, buffer, ret);
        s->ret = ret;
        s->len = len;
        return 1;
    }
}