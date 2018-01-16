/* queue status and conditional variable*/
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
int init_taskqueue(taskqueue *tq)
{
    pthread_mutex_init(&(tq->mutex),NULL);
    tq->front = tq->rear = (task *)malloc(sizeof(task));
    tq->front->next = NULL;
    pthread_mutex_init(&(tq->has_jobs->mutex),NULL);
    pthread_cond_init(&(tq->has_jobs->cond),NULL);
    tq->has_jobs->status = false;
    tq->len = 0;
    return OK;
}
struct threadpool *initTheadPool(int num_threads) /*线程池初始化函数*/
{
    threadpool *pool; //创建线程池空间
    pool = (threadpool *)malloc(sizeof(struct threadpool));
    pool->num_threads = 0;
    pool->num_working = 0;
    pthread_mutex_init(&(pool_p->thcount_lock), NULL);//初始化互斥量和条件变量
    pthread_cond_init(&pool_p->threads_all_idle, NULL);
    //****需实现*****
    init_taskqueue(&pool->queue);//初始化任务队列
    pool->threads = (struct thread **)malloc(num_threads * sizeof(struct * thread));//创建线程数组
    for (int i = 0; i < num_threads; ++i)//创建线程
    {
        create_thread(pool, pool->thread[i], i); //i 为线程 id,
    }
    //等等所有的线程创建完毕,在每个线程运行函数中将进行 pool->num_threads++ 操作
    //因此,此处为忙等待,直到所有的线程创建完毕,并马上运行阻塞代码时才返回。
    while (pool->num_threads != num_threads)
    {
        
    }
    return pool;
}
int push_taskqueue(taskqueue *tq,task *curtask)
{
    pthread_mutex_lock(&tq->mutex);
    tq->rear->next = curtask;
    tq->rear = curtask;
    tq->rear->function = &web;
    tq->rear->arg = (void *)param;
    tq->len++;
    pthread_mutex_unlock(&tq->mutex);
    tq->has_jobs->status = true;
    return OK;
}
/*向线程池中添加任务*/
void addTask2ThreadPool(threadpool *pool, task *curtask)
{
    //将任务加入队列
    //****需实现*****
    push_taskqueue(&pool->queue, curtask);
}
/*等待当前任务全部运行完*/
void waitThreadPool(threadpool *pool)
{
    pthread_mutex_lock(&pool->thcount_lock);
    while (pool->queue->len || pool->num_working)
    {
        pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    pthread_mutex_unlock(&pool->thcount_lock);
}
int destory_taskqueue(taskqueue *tq)
{
    taskqueue *p;
    if(tq->rear==tq->front)
    {
        return OK;
    }
    else
    {
        for(;tq->front!=NULL;free(p))
        {
            p = tq->front;
            tq->front = tq->front->next;
        }
        tq->len = 0;
    }
}
void destoryThreadPool(threadpool *pool)/*销毁线程池*/
{
    //如果当前任务队列中有任务,需等待任务队列为空,并且运行线程执行完任务后
    //pthread_mutex_lock(&pool->thcount_lock);
    while(pool->queue->has_jobs->status)
    {

    }
    pthread_cond_broadcast(&pool->threads_all_idle);
    //pthread_mutex_unlock(&pool->thcount_lock);
    for(int i = 0;i<pool->num_threads;i++)
    {
        pthread_join(i,NULL);
    }
        //销毁任务队列
        //****需实现*****
        destory_taskqueue(&pool->queue);
    //销毁线程指针数组,并释放所有为线程池分配的内存
    pthread_mutex_destroy(&(pool->thcount_lock));
    pthread_cond_destroy(&(pool->threads_all_idle));
    free(pool->threads);
    pool->is_alive = false;
    /*free(pool);
    pool = NULL;*/

}

int getNumofThreadWorking(threadpool *pool)/*获得当前线程池中正在运行线程的数量*/
{
    return pool->num_working;
}

int create_thread(struct threadpool *pool, struct thread **pthread, int id)/*创建线程*/
{
    
    *pthread = (struct thread *)malloc(sizeof(struct thread));//为 thread 分配内存空间
    if (pthread == NULL)
    {
        error("creat_thread(): Could not allocate memory for thread\n");
        return -1;
    }
    
    (*pthread)->pool = pool;//设置这个 thread 的属性
    (*pthread)->id = id;
    //创建线程
    pthread_create(&(*pthread)->pthread, NULL, (void *)thread_do, (*pthread));
    pthread_detach((*pthread)->pthread);
    return 0;
}
struct task take_taskqueue(taskqueue *tq)
{
    task *t;
    if(tq->rear == tq>front)
    {
        tq->has_jobs->status = false;
        return NULL;
    }
    else
    {
        t = tq->front->next;
        tq->front->next=t->next;
        if(tq->rear==t)
        {
            tq->rear=tq->front;
        }
        tq->len++;
        return t;
    }
}
void *thread_do(struct thread *pthread)/*线程运行的逻辑函数*/
{
    
    char thread_name[128] = {0};/* 设置线程名字 */
    sprintf(thread_name, "thread-pool-%d", pthread->id);
    prctl(PR_SET_NAME, thread_name);
    
    threadpool *pool = pthread->pool;/* 获得线程池*/
    /* 在线程池初始化时,用于已经创建线程的计数,执行 pool->num_threads++ */
    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads++;
    pthread_mutex_unlock(&pool->thcount_lock);
        /*线程一直循环往复运行,直到 pool->is_alive 变为 false*/
    while (pool->is_alive)
    {
        /*如果任务队列中还有任务,则继续运行,否则阻塞*/
        if(pool->queue->has_jobs->status)
        {
            pthread_cond_wait(&(pool->queue->has_jobs->cond),&(pool->queue->has_jobs->mutex));
        }
        if (pool->is_alive)
        {
            /*执行到此位置,表明线程在工作,需要对工作线程数量进行计数*/
            //pool->num_working++
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_working++;
            pthread_mutex_unlock(&pool->thcount_lock);
                /* 从任务队列的队首提取任务,并执行*/
                void (*func)(void *);
            void *arg;
            //take_taskqueue 从任务队列头部提取任务,并在队列中删除此任务
            //****需实现 take_taskqueue*****
            task *curtask = take_taskqueue(&pool->queue);
            if (curtask)
            {
                func = curtask->function;
                arg = curtask->arg;
                //执行任务
                func(arg);
                //释放任务
                free(curtask);
            }
            /*执行到此位置,表明线程已经将任务执行完成,需更改工作线程数量*/
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_working--;
            pthread_mutex_unlock(&pool->thcount_lock);
            //此处还需注意,当工作线程数量为 0,表示任务全部完成,要让阻塞在 waitThreadPool 函
            //数上的线程继续运行
            if(pool->num_working==0)
            {
                pthread_cond_broadcast(&pool->threads_all_idle);
            }
        }
    }
    /*运行到此位置表明,线程将要退出,需更改当前线程池中的线程数量*/
    //pool->num_threads--
    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads--;
    pthread_mutex_unlock(&pool->thcount_lock);
    return NULL;
}