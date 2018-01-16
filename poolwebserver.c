#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/prctl.h>
#define OK 1
#define VERSION 23
#define BUFSIZE 8096
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif
typedef struct staconv
{
    pthread_mutex_t mutex;
    pthread_cond_t cond; /*用于阻塞和唤醒线程池中线程*/
    int status;          /*表示任务队列状态:false 为无任务;true 为有任务*/
} staconv;
typedef struct task /*Task*/
{
    struct task *next;           /* 指向下一任务 */
    void (*function)(void *arg); /* 函数指针*/
    void *arg;                   /* 函数参数指针 */
} task;
/*Task Queue*/
typedef struct twebaskqueue
{
    pthread_mutex_t mutex; /* 用于互斥读写任务队列 */
    task *front;           /* 指向队首*/
    task *rear;            /* 指向队尾*/
    staconv *has_jobs;     /* 根据状态,阻塞线程 */
    int len;               /* 队列中任务个数*/
} taskqueue;
/* Thread */
typedef struct thread
{
    int id;            /* 线程 id*/
    pthread_t pthread; /* 封装的 POSIX 线程*/
    struct threadpool* pool;/* 与线程池绑定*/
} thread;
/*Thread Pool*/
typedef struct threadpool
{
    thread **threads;/* 线程指针数组*/
    volatile int num_threads;/* 线程池中线程数量*/
    volatile int num_working;/* 目前正在工作的线程个数*/
    pthread_mutex_t thcount_lock;/* 线程池锁用于修改上面两个变量 */
    pthread_cond_t threads_all_idle;/* 用于销毁线程的条件变量*/
    taskqueue queue; /* 任务队列*/
    volatile bool is_alive;/* 表示线程池是否还存活*/
} threadpool;
struct
{
    char *ext;
    char *filetype;
} extensions[] = {
    {"gif", "image/gif"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"ico", "image/ico"},
    {"zip", "image/zip"},
    {"gz", "image/gz"},
    {"tar", "image/tar"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {0, 0}};
typedef struct
{
    int hit;
    int fd;
} webparam;
void logger(int type, char *s1, char *s2,int socket_fd)
{
    int fd;
    char logbuffer[BUFSIZE * 2];
    switch (type)
    {
    case ERROR:
        (void)sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2,errno, getpid());
        break;
    case FORBIDDEN:
        (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
        (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2);
        break;
    case NOTFOUND:
        (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
        (void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2);
        break;
    case LOG: 
        (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
        }
    /* No checks here, nothing can be done with a failure anyway */
    if ((fd = open("nweb.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
    {
        (void)write(fd, logbuffer, strlen(logbuffer));
        (void)write(fd, "\n", 1);
        (void)close(fd);
    }
}
/* this is a web thread, so we can exit on errors */
void web(void *data)
{
    struct stat statbuff;
    int fd;
    int hit;
    int j, file_fd, buflen;
    long i, ret, len;
    char *fstr;
    char buffer[BUFSIZE + 1]; /* static so zero filled */
    webparam *param = (webparam *)data;
    fd = param->fd;
    hit = param->hit;
    ret = read(fd, buffer, BUFSIZE); /* read web request in one go */
    if (ret == 0 || ret == -1)
    { /* read failure stop now */
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    }
    else
    {
        if (ret > 0 && ret < BUFSIZE) /* return code is valid chars */
            buffer[ret] = 0;
        /* terminate the buffer */
        else
            buffer[0] = 0;
        for (i = 0; i < ret; i++) /* remove cf and lf characters */
            if (buffer[i] == '\r' || buffer[i] == '\n')
                buffer[i] = '*';
        logger(LOG, "request", buffer, hit);
        if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4))
        {
            logger(FORBIDDEN, "only simple get operation supported", buffer, fd);
        }
        for (i = 4; i < BUFSIZE; i++)
        { /* null terminate after the second space to ignore extra stuff */
            if (buffer[i] == ' ')
            { /* string is "get url " +lots of other stuff */
                buffer[i] = 0;
                break;
            }
        }
        for (j = 0; j < i - 1; j++) /* check for illegal parent directory use .. */
            if (buffer[j] == '.' && buffer[j + 1] == '.')
            {
                logger(FORBIDDEN, "parent directory (..) path names not supported", buffer,fd);
            }
        if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) /* convert no filename to
                                                                                        index file */
            (void)strcpy(buffer, "GET /11901.html");
        /* work out the file type and check we support it */
        buflen = strlen(buffer);
        fstr = (char *)0;
        for (i = 0; extensions[i].ext != 0; i++)
        {
            len = strlen(extensions[i].ext);
            if (!strncmp(&buffer[buflen - len], extensions[i].ext, len))
            {
                fstr = extensions[i].filetype;
                break;
            }
        }
        if (fstr == 0)
            logger(FORBIDDEN, "file extension type not supported", buffer,fd);
    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1)
    { /* open the file for reading */
        logger(NOTFOUND, "failed to open file", &buffer[5],fd);
    }
    logger(LOG, "send", &buffer[5],hit);
    fstat(file_fd,&statbuff);
    len = statbuff.st_size;
    (void)sprintf(buffer,"http/1.1 200 ok\nserver: nweb/%d.0\ncontent-length: %ld\nconnection: close\ncontent-type: %s\n\n", VERSION, len, fstr); /* header + a blank line */
    logger(LOG,"header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));
    /* send file in 8kb block - last block may be smaller */
    while ( (ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd, buffer, ret);
    }
    usleep(10000);/*在 socket 通道关闭前,留出一段信息发送的时间*/
    close(file_fd);
    }
    close(fd);
    //释放内存
    free(param);
}
int init_taskqueue(taskqueue *tq)
{
    tq->front = tq->rear = (task *)malloc(sizeof(task));
    tq->has_jobs = (staconv *)malloc(sizeof(staconv));
    tq->front->next = NULL;
    tq->len = 0;
    tq->has_jobs->status = false;
    pthread_mutex_init(&(tq->mutex),NULL);
    pthread_mutex_init(&(tq->has_jobs->mutex),NULL);
    pthread_cond_init(&(tq->has_jobs->cond),NULL);
    return OK;
}
struct task *take_taskqueue(taskqueue *tq)
{
    pthread_mutex_lock(&tq->mutex);
    task *t = tq->front->next;
    tq->front->next=t->next;
    if(tq->rear==t)
    {
        tq->rear=tq->front;
    }
    tq->len--;
    if(tq->len==0)
    {
        tq->has_jobs->status = false;
    }
    pthread_mutex_unlock(&tq->mutex);
    return t;
}
void waitThreadPool(threadpool *pool)
{
    pthread_mutex_lock(&pool->thcount_lock);
    while (pool->queue.len || pool->num_working)
    {
        pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    pthread_mutex_unlock(&pool->thcount_lock);
}
int getNumofThreadWorking(threadpool *pool)/*获得当前线程池中正在运行线程的数量*/
{
    return pool->num_working;
}
void *thread_do(struct thread *pthread)/*线程运行的逻辑函数*/
{
    char thread_name[128];/* 设置线程名字 */
    sprintf(thread_name, "thread-pool-%d", pthread->id);
    prctl(PR_SET_NAME, thread_name);
    threadpool *pool = pthread->pool;   /* 获得线程池*/
    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads++;
    pthread_mutex_unlock(&pool->thcount_lock);
    while (pool->is_alive)
    {
        /*如果任务队列中还有任务,则继续运行,否则阻塞*/
        pthread_mutex_lock(&(pool->queue.has_jobs->mutex));
        while(pool->queue.len==0||pool->queue.has_jobs->status==false)
        {
            pthread_cond_wait(&(pool->queue.has_jobs->cond),&(pool->queue.has_jobs->mutex));
        }
        if (pool->is_alive)
        {
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_working++;
            pthread_mutex_unlock(&pool->thcount_lock);            
            task *curtask = take_taskqueue(&pool->queue);
            pthread_mutex_unlock(&(pool->queue.has_jobs->mutex));  
            if (curtask)
            {
                void (*func)(void *);
                void *arg;
                func = curtask->function;
                arg = curtask->arg;
                func(arg);
                free(curtask);
            }
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_working--;
            pthread_mutex_unlock(&pool->thcount_lock);
            //此处还需注意,当工作线程数量为 0,表示任务全部完成,要让阻塞在 waitThreadPool 函
            //数上的线程继续运行
        }
    }
    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads--;
    pthread_mutex_unlock(&pool->thcount_lock);
    return NULL;
}
int create_thread(struct threadpool *pool, struct thread **pthread, int id)/*创建线程*/
{
    
    *pthread = (struct thread *)malloc(sizeof(struct thread));//为 thread 分配内存空间
    (*pthread)->pool = pool;//设置这个 thread 的属性
    (*pthread)->id = id;
    pthread_create(&((*pthread)->pthread), NULL, (void *)thread_do, (*pthread));
    pthread_detach((*pthread)->pthread);
    return 0;
}
struct threadpool *initTheadPool(int num_threads) /*线程池初始化函数*/
{
    threadpool *pool = (threadpool *)malloc(sizeof(struct threadpool)); //创建线程池空间
    pool->num_threads = 0;
    pool->num_working = 0;
    pool->is_alive = true;
    pthread_mutex_init(&(pool->thcount_lock), NULL);//初始化互斥量和条件变量
    pthread_cond_init(&(pool->threads_all_idle), NULL);
    init_taskqueue(&(pool->queue));//初始化任务队列
    pool->threads = (struct thread **)malloc(num_threads * sizeof(struct thread));//创建线程数组
    for (int i = 0; i < num_threads; ++i)//创建线程
    {
        create_thread(pool, &(pool->threads[i]), i); //i 为线程 id,
    }
    while (pool->num_threads != num_threads)
    {
        
    }
    return pool;
}
int push_taskqueue(taskqueue *tq,webparam *param)
{   
    task *curtask = (task *)malloc(sizeof(task));
    curtask->function = &web;
    curtask->arg = param;
    curtask->next = NULL;
    tq->rear->next = curtask;
    tq->rear = curtask;
    tq->len++;
    tq->has_jobs->status = true;
    return OK;
}
/*向线程池中添加任务*/
void addTask2ThreadPool(threadpool *pool,webparam *param)
{
    pthread_mutex_lock(&(pool->queue.mutex));
    push_taskqueue(&pool->queue,param);
    pthread_cond_signal(&(pool->queue.has_jobs->cond));
    pthread_mutex_unlock(&(pool->queue.mutex));
}
int main(int argc, char **argv)
{
    int i, port, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr;  /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?"))
    {
        (void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
                     "\tnweb is a small and very safe mini web server\n"
                     "\tnweb only servers out file/web pages with extensions named below\n"
                     "\t and only from the named directory or its sub-directories.\n"
                     "\tThere is no fancy features = safe and secure.\n\n"
                     "\tExample: nweb 8181 /home/nwebdir &\n\n"
                     "\tOnly Supports:",
                     VERSION);
        for (i = 0; extensions[i].ext != 0; i++)
            (void)printf(" %s", extensions[i].ext);
        (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                     "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                     "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }
    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6))
    {
        (void)printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
        exit(3);
    }
    if (chdir(argv[2]) == -1)
    {
        (void)printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }
    if (fork() != 0)
      return 0;                  /* parent returns OK to shell */
    (void)signal(SIGCLD, SIG_IGN); /* ignore child death */
    (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
    for (i = 0; i < 32; i++)
        (void)close(i);
    /* close open files */
    (void)setpgrp();
    /* break away from process group */
    logger(LOG, "nweb starting", argv[1],getpid());
    /* setup the network socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "system call", "socket", 0);
    port = atoi(argv[1]);
    if (port < 0 || port > 60000)
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    threadpool *pool = initTheadPool(200);  //初始化线程池
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        logger(ERROR, "system call", "bind",0);
    if (listen(listenfd, 64) < 0)
        logger(ERROR, "system call", "listen",0);
    for (hit = 1;; hit++)
    {
        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
            logger(ERROR, "system call", "accept",0);
        webparam *param = malloc(sizeof(webparam));
        param->hit = hit;
        param->fd = socketfd;
        addTask2ThreadPool(pool,param);  //任务进队
    }
}