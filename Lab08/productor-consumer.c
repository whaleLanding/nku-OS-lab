#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#define N 4  // 共享缓冲区大小
#define K 0 // 生产和消费的商品总数
#define I 2  // 生产者数量
#define J 3  // 消费者数量
#define DATA_SIZE 256

typedef struct item_st
{
    int id;
    time_t timestamp_p;
    int producer_id;
    unsigned long checksum;
    unsigned long data[DATA_SIZE];
} ITEM_ST;

typedef struct buffer_st
{
    int id;
    int isempty;
    ITEM_ST item;
    struct buffer_st *nextbuf;
} BUFFER_ST;

BUFFER_ST buffer[N];
int in = 0;
int out = 0;
int produced_count = 0;
int consumed_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t production = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t consumption = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t area = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print = PTHREAD_MUTEX_INITIALIZER;

int prepared_consumer = 0;

sem_t full;
sem_t empty;

// 打印商品信息
void printProductionInfo(int type, int state, int id, ITEM_ST item)
{
    if (state == 1)
    {
        if (type == 1)
        {
            printf("生产者 %d 生产了一件商品,生产的商品信息如下\n", id);
        }
        else
        {
            printf("消费者 %d 消费完了刚取出的商品,消费的商品信息如下\n", id);
        }
        // 时间格式转换
        char time_buffer[80];
        strftime(time_buffer, sizeof(time_buffer), "%Y年%m月%d日 %H时%M分%S秒", localtime(&item.timestamp_p));
        printf("-------------------------------------------------\n");
        printf("商品编号\t|%d\t\n",item.id);
        printf("-------------------------------------------------\n");
        printf("商品生产时间\t|%s\t\n",time_buffer);
        printf("-------------------------------------------------\n");
        printf("商品校验码\t|%lu\t\n",item.checksum);
        printf("-------------------------------------------------\n");
        printf("商品详细信息\t|");
        for (int i = 1; i <= DATA_SIZE/8; i++)
        {
            printf("%lu", item.data[i]);
            if(i%32==0) printf("\n");
        }
        printf("-------------------------------------------------\n\n");
    }
    else
    {
        if (type == 1)
        {
            printf("生产者 %d 将刚生产完的商品 %d 放入缓冲区\n", id, item.id);
        }
        else
        {
            printf("消费者 %d 从缓冲区取出商品 %d \n", id, item.id);
        }
    }
}

void *producer(void *arg)
{
    int producer_num = *((int *)arg);
    while (1)
    {
        /* 生产者生产商品过程 */
        pthread_mutex_lock(&production);
        if (produced_count >= K) /* 生产数量已经达标 */
        {
            printf("生产者 %d 工作完成\n", producer_num);
            pthread_mutex_unlock(&production);
            return NULL;
        }
        int id = ++produced_count; // 生产数量不达标，生产新的商品
        pthread_mutex_unlock(&production);

        /* 生产者开始生产商品 */
        ITEM_ST item;
        item.id = id;
        item.timestamp_p = time(NULL);
        item.producer_id = producer_num;
        item.checksum = rand();
        for (int j = 0; j < DATA_SIZE; j++)
        {
            item.data[j] = rand() % 10;
        }

        /* 打印生产的商品信息 */
        pthread_mutex_lock(&print);
        printProductionInfo(1, 1, producer_num, item); 
        pthread_mutex_unlock(&print);

        /* 检验是否生产的商品的能够被消费*/
        if(J==0&&((in+1)%N==out)){
            printf("由于没有消费者且缓冲区已满无法存放新的商品,生产者 %d 结束工作\n", producer_num);
            return NULL;

        }
        /* 生产者准备进入临界区，等待检验缓冲区是否已满，并竞争互斥锁 */
        printf("生产者 %d 正在等待检验缓冲区是否已满，准备进入临界区\n", producer_num);
        sem_wait(&empty); // 生产者在此等待检验缓冲区是否已满
        pthread_mutex_lock(&mutex); // 生产者在此等待获得进入临界资源的锁
        printf("生产者 %d 获得了互斥锁,已进入临界区\n", producer_num);

        /* 生产者将生产的商品放入缓冲区 */
        buffer[in].item = item;
        buffer[in].isempty = 0;
        in = (in + 1) % N;
        
        /* 打印生产者放入商品的信息*/
        pthread_mutex_lock(&print);
        printProductionInfo(1, 0, producer_num, item);
        pthread_mutex_unlock(&print); 

        /* 生产者释放互斥锁，并增加临界区资源，唤醒一个等待着的消费者线程*/
        pthread_mutex_unlock(&mutex); 
        sem_post(&full);  
        printf("生产者 %d 释放了临界区的锁，已离开临界区\n\n", producer_num);

        // 随机等待一段时间
        int sleep_time = rand() % 3 + 1;
        usleep(sleep_time * 1000000);
    }
    return NULL;
}

void *consumer(void *arg)
{
    int consumer_num = *((int *)arg);
    while (1)
    {
        /* 检验消费者是否有资格进入缓冲区 */
        pthread_mutex_lock(&area);
        prepared_consumer++; // 准备进入缓冲区的消费者个数
        if (prepared_consumer > K - consumed_count) // 剩余可消费商品数小于准备进入缓冲区的消费者数
        {
            printf("消费者%d工作完成\n", consumer_num);
            pthread_mutex_unlock(&area);
            return NULL;
        }
        pthread_mutex_unlock(&area);

        /* 消费者准备进入临界区，先判断缓冲区是否有资源，再竞争互斥锁*/
        printf("消费者 %d 正在等待检验缓冲区是否有资源，准备进入临界区\n", consumer_num);
        sem_wait(&full); 
        pthread_mutex_lock(&mutex); 
        printf("消费者 %d 获得了互斥锁,已进入临界区\n", consumer_num);

        /* 消费者从缓冲区取出商品*/
        ITEM_ST item;
        item = buffer[out].item;
        buffer[out].isempty = 1;
        out = (out + 1) % N;

        /*打印消费者取出商品的信息*/
        pthread_mutex_lock(&print);
        printProductionInfo(0, 0, consumer_num, item);
        pthread_mutex_unlock(&print);

        /* 消费者离开临界区，释放占有的锁，并且增加缓冲区空位，从而唤醒等待着的生产者线程*/
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);             
        printf("消费者 %d 释放了临界区的锁，已离开临界区\n\n", consumer_num);

        /* 消费者消费商品，商品作为共享资源需要互斥访问 */
        pthread_mutex_lock(&consumption);
        consumed_count++;
        prepared_consumer--; // 消费者消费完商品，需要释放缓冲区位置
        pthread_mutex_unlock(&consumption);

        /* 打印消费者消费商品信息 */
        pthread_mutex_lock(&print);
        printProductionInfo(0, 1, consumer_num, item);
        pthread_mutex_unlock(&print); 

        /* 随机等待一段时间 */
        int sleep_time = rand() % 3 + 1;
        usleep(sleep_time * 1000000);
    }
    return NULL;
}

int main()
{
    printf("本次生产者个数: %d\n消费者个数: %d\n生产商品总数: %d\n缓冲区大小: %d\n", I, J, K, N);
    printf("----------------------实验开始----------------------\n\n\n\n");
    
    srand((unsigned int)time(NULL));
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, N);

    pthread_t producers[I];
    pthread_t consumers[J];
    int producer_ids[I];
    int consumer_ids[J];

    // 初始化缓冲区为空
    for (int i = 0; i < N; i++)
    {
        buffer[i].isempty = 1;
    }
    // 判断是否能够正常生产
    if(I == 0){
            printf("由于生产者个数为0,无法生产产品,实验结束\n");
            return 0;
    }

    for (int i = 0; i < I; i++)
    {
        producer_ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &producer_ids[i]);
    }

    for (int i = 0; i < J; i++)
    {
        consumer_ids[i] = i;
        pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]);
    }

    for (int i = 0; i < I; i++)
    {
        pthread_join(producers[i], NULL);
        printf("生产者线程%d已经结束\n", i);
    }

    for (int i = 0; i < J; i++)
    {
        pthread_join(consumers[i], NULL);
        printf("消费者线程%d已经结束\n", i);
    }

    sem_destroy(&full);
    sem_destroy(&empty);
    pthread_mutex_destroy(&mutex);

    return 0;
}