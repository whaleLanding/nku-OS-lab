#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#define N 3               // 等待椅子数量
#define CUSTOMER_COUNT 30 // 累计顾客数量

// 顾客结构体
typedef struct customer_st
{
    int id;
} CUSTOMER_ST;

// 理发店结构体,包含理发师状态、理发椅状态、等待椅子状态等信息
typedef struct barbershop_st
{
    int barber_status;              // 0: 睡觉, 1: 理发
    int barber_chair_status;        // 0: 空闲, 1: 占用
    int waiting_chairs_status[N];   // 0: 空闲, 1: 占用
    pthread_mutex_t mutex_chair;    // 维护临界区椅子的状态
    pthread_mutex_t mutex_haircut;  // 维护理发时理发椅和服务的顾客状态
    pthread_mutex_t mutex_customer; // 维护累计来的顾客数目
    sem_t customers_sem;            // 初始化为0
    sem_t barber_sem;               // 初始化为1
} BARBERSHOP_ST;

BARBERSHOP_ST barbershop;
int total_customers_came = 0; // 记录累计来的顾客数目
int total_customers_served = 0; // 记录已服务的顾客数目

// 打印理发店状态信息
void printBarbershopStatus()
{
    printf("\n");
    printf("理发店状态如下\n");
    printf("------------------------------------------\n");
    printf("理发师状态: %s\n", barbershop.barber_status == 0 ? "睡觉" : "理发");
    printf("等待椅子状态: ");
    for (int i = 0; i < N; i++)
    {
        printf("%s ", barbershop.waiting_chairs_status[i] == 0 ? "空闲" : "占用");
    }
    printf("\n");
    printf("------------------------------------------\n");
    printf("\n\n");
}

// 理发师线程函数
void *barber(void *arg)
{
    while (1)
    {
        /* 检查是否有人在暂坐椅子等待 */
        pthread_mutex_lock(&barbershop.mutex_chair);
        int waiting = 0; 
        for(int i =0;i < N; i++){
            if(barbershop.waiting_chairs_status[i] == 1){
                waiting = 1;
                break;
            }
        }
        pthread_mutex_unlock(&barbershop.mutex_chair);
        /* 检查是否已经累计来了30名顾客 */
        pthread_mutex_lock(&barbershop.mutex_customer);
        if (total_customers_came >= 31 && waiting == 0)
        {
            pthread_mutex_unlock(&barbershop.mutex_customer);
            printf("已经来了30名顾客,理发师进程结束\n");
            return NULL;
        }
        pthread_mutex_unlock(&barbershop.mutex_customer);
        /* 顾客信号量-1,如果为0,则一直等待 */
        printf("理发师等待顾客理发\n");
        sem_wait(&barbershop.customers_sem);
        /* 有顾客进入理发店并唤醒了理发师 */
        printf("理发师线程准备进入临界区\n");
        /* 维护理发时理发椅和正在理发的顾客状态 */
        pthread_mutex_lock(&barbershop.mutex_haircut);
        printf("理发师线程已进入临界区\n");
        // 随机生成理发时间
        int haircut_time = rand() % 5 + 1;
        printf("理发师开始理发,预计耗时 %d 秒\n", haircut_time);
        // 理发师开始为顾客理发
        barbershop.barber_status = 1;
        barbershop.barber_chair_status = 1;
        usleep(haircut_time * 1000000);
        // 理发结束
        barbershop.barber_status = 0;
        barbershop.barber_chair_status = 0;
        total_customers_served++;
        printf("理发师理发结束,耗时 %d 秒,已服务 %d 名顾客\n", haircut_time, total_customers_served);
        // 释放互斥锁,离开临界区,并唤醒可能等待的顾客
        pthread_mutex_unlock(&barbershop.mutex_haircut);
        printf("理发师离开临界区\n");
        /* 理发师信号量+1 */
        sem_post(&barbershop.barber_sem);

        printBarbershopStatus();
    }
    return NULL;
}

// 顾客线程函数
void *customer(void *arg)
{  
    CUSTOMER_ST *customer_info = (CUSTOMER_ST *)arg;
    int thread_num = customer_info->id;
    while (1)
    {
        // 随机生成顾客到达理发店的时间
        usleep(rand() % 10000000);
        /* 维护顾客id */
        pthread_mutex_lock(&barbershop.mutex_customer);
        /* 顾客到达 */
        total_customers_came++;
        /* 如果是第31位顾客,则线程退出 */
        if (total_customers_came >= 31)
        {
            printf("累计来了30名顾客,线程 %d 结束\n", thread_num);
            pthread_mutex_unlock(&barbershop.mutex_customer);
            return NULL;
        }
        /* 记录到达的顾客id */
        int customer_id = total_customers_came;
        printf("第 %d 位顾客到达\n", customer_id);
        pthread_mutex_unlock(&barbershop.mutex_customer);

        // 检查理发店状态
        if (barbershop.barber_status == 0) // 如果理发师在休息,则唤醒
        {
            /* 理发师信号量-1,若为0,则一直等待 */
            sem_wait(&barbershop.barber_sem); 
            printf("顾客 %d 叫醒了理发师\n", customer_id);
            /* 顾客信号量+1 */
            sem_post(&barbershop.customers_sem);
        }
        else
        {
            /* 维护等待座位的状态 */
            pthread_mutex_lock(&barbershop.mutex_chair);
            // 检查是否有空的等待椅子
            int i = 0;
            for (; i < N; i++)
            {
                if (barbershop.waiting_chairs_status[i] == 0) // 当前椅子为空
                {   
                    printf("顾客 %d 进入了临界区,坐在暂坐椅子 %d 上等待\n", customer_id, i+1);
                    barbershop.waiting_chairs_status[i] = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&barbershop.mutex_chair);
            if (i == N) // 当前没有空椅子
            {
                printf("顾客 %d 因没有空的等待椅子而离开\n", customer_id);
            }
            else // 当前有空椅子,且顾客坐在第position张椅子等待
            {
                /* 等待理发师的信号量大于0 */
                sem_wait(&barbershop.barber_sem);
                /* 释放占用的椅子 */
                pthread_mutex_lock(&barbershop.mutex_chair);
                printf("理发师空闲,顾客 %d 离开了临界区(暂坐椅子),准备理发\n", customer_id);
                barbershop.waiting_chairs_status[i] = 0;
                pthread_mutex_unlock(&barbershop.mutex_chair);
                /* 将需要服务的顾客信号量+1 */
                sem_post(&barbershop.customers_sem);
            }
        }
    }
    // 打印理发店状态信息
    printBarbershopStatus();

    return NULL;
}

int main()
{
    // 初始化理发店状态
    barbershop.barber_status = 0;
    barbershop.barber_chair_status = 0;
    for (int i = 0; i < N; i++)
    {
        barbershop.waiting_chairs_status[i] = 0;
    }

    // 初始化互斥锁和信号量
    pthread_mutex_init(&barbershop.mutex_chair, NULL);
    pthread_mutex_init(&barbershop.mutex_haircut, NULL);
    // 初始化顾客数目为0
    sem_init(&barbershop.customers_sem, 0, 0);
    // 初始化理发师数目为1
    sem_init(&barbershop.barber_sem, 0, 1);

    // 创建理发师线程
    pthread_t barber_thread;
    pthread_create(&barber_thread, NULL, barber, NULL);

    // 创建顾客线程数组
    pthread_t customer_threads[5];
    CUSTOMER_ST customer_info[5];

    // 创建顾客线程并传入顾客信息
    for (int i = 0; i < 5; i++)
    {
        customer_info[i].id = i + 1;
        pthread_create(&customer_threads[i], NULL, customer, &customer_info[i]);
    }

    // 等待理发师线程结束
    pthread_join(barber_thread, NULL);

    // 等待顾客线程结束
    for (int i = 0; i < 5; i++)
    {
        pthread_join(customer_threads[i], NULL);
    }

    // 销毁互斥锁和信号量
    pthread_mutex_destroy(&barbershop.mutex_chair);
    pthread_mutex_destroy(&barbershop.mutex_haircut);
    sem_destroy(&barbershop.customers_sem);
    sem_destroy(&barbershop.barber_sem);

    return 0;
}