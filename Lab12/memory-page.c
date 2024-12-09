#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PAGE_SIZE 10
#define MEMORY_BLOCKS 4
#define PAGE_NUM 32
#define INSTRUCTION_NUM 320

// 页面结构体,记录页面相关信息
typedef struct Page
{
    int pageNumber; // 页号
    int inMemory;   // 是否调入内存
    int lastUsed;   // 距上次访问未被访问的次数
} Page;

// 页表结构体,记录页表相关信息
typedef struct PageTable
{
    Page pages[PAGE_NUM];
} PageTable;

// 内存块结构体,记录内存块中的页面号
typedef struct MemoryBlock
{
    int pageNumber;
} MemoryBlock;

// 初始化页表,将所有页面标记为不在内存
void initPageTable(PageTable *pageTable)
{
    for (int i = 0; i < PAGE_NUM; i++)
    {
        pageTable->pages[i].pageNumber = i;
        pageTable->pages[i].inMemory = 0;
        pageTable->pages[i].lastUsed = 0;
    }
}

// 初始化内存块,将内存块中的页面号设为 -1（表示为空）
void initMemoryBlocks(MemoryBlock *memoryBlocks)
{
    for (int i = 0; i < MEMORY_BLOCKS; i++)
        memoryBlocks[i].pageNumber = -1;
}

// 根据指令地址获取页号
int getPageNumber(int instructionAddress)
{
    return instructionAddress / PAGE_SIZE;
}

// 根据指令地址获取页内偏移量
int getOffset(int instructionAddress)
{
    return instructionAddress % PAGE_SIZE;
}

// 查找页面是否在内存中,若在则返回所在内存块的索引,否则返回 -1
int findPageInMemory(PageTable pageTable, MemoryBlock memoryBlocks[], int pageNumber)
{
    for (int i = 0; i < MEMORY_BLOCKS; i++)
    {
        if (memoryBlocks[i].pageNumber == pageNumber)
            return i;
    }
    return -1;
}

// OPT算法进行页面置换
void optPageReplace(PageTable *pageTable, MemoryBlock *memoryBlocks, int pageNumber, int currentInstrcution)
{
    int farthest = -1;                      // 最久使用时间间隔
    int replaceIndex = -1;                  // 要替换页面所在的块号
    for (int i = 0; i < MEMORY_BLOCKS; i++) // 遍历得到每个块内页面下次被访问的时间间隔
    {
        int currentPageNumber = memoryBlocks[i].pageNumber;
        int j = currentInstrcution;
        while (j < INSTRUCTION_NUM)
        {
            if (getPageNumber(j) == currentPageNumber) {
                break;
            }
            j++;
        }
        if (j > farthest)
        {
            farthest = j;
            replaceIndex = i;
        }
    }
    memoryBlocks[replaceIndex].pageNumber = pageNumber;
}

// FIFO算法进行页面置换
void fifoPageReplace(MemoryBlock *memoryBlocks, int pageNumber, int *fifoIndex)
{
    memoryBlocks[*fifoIndex].pageNumber = pageNumber;
    *fifoIndex = (*fifoIndex + 1) % MEMORY_BLOCKS; // 更新先进先出索引
}

// LRU算法进行页面置换
void lruPageReplace(PageTable *pageTable, MemoryBlock *memoryBlocks, int pageNumber)
{
    int leastRecentlyUsed = 0;
    int currentTime = 320;
    // 获得最近最久使用的页面号
    for (int i = 0; i < MEMORY_BLOCKS; i++)
    {
        if (pageTable->pages[memoryBlocks[i].pageNumber].lastUsed < currentTime)
        {
            currentTime = pageTable->pages[memoryBlocks[i].pageNumber].lastUsed;
            leastRecentlyUsed = i;
        }
    }
    memoryBlocks[leastRecentlyUsed].pageNumber = pageNumber;
}

/*
生成指令访问次序
1. 50%为逆序
2. 25%在低地址
3. 25%在高地址
*/
void generateInstructionOrder(int instructionOrder[])
{
    srand((unsigned int)time(NULL));
    int m = rand() % INSTRUCTION_NUM;
    int index = 0;
    instructionOrder[index++] = m;
    instructionOrder[index++] = m + 1;
    while (index < INSTRUCTION_NUM)
    {
        int m1 = rand() % m;
        instructionOrder[index++] = m1;
        instructionOrder[index++] = m1 + 1;
        int m2 = rand() % (INSTRUCTION_NUM - (m1 + 2)) + (m1 + 2);
        instructionOrder[index++] = m2;
        instructionOrder[index++] = m2 + 1;
    }
}

// 模拟请求调页过程
void simulateRequestPaging(PageTable *pageTable, MemoryBlock *memoryBlocks, int algorithm, int instructionOrder[], int *pageFaults)
{
    int fifoIndex = 0;
    int currentTime = 0; // 最近最久使用时间
    for (int i = 0; i < INSTRUCTION_NUM; i++)
    {
        int pageNumber = getPageNumber(instructionOrder[i]);                      // 获取页号
        int offset = getOffset(instructionOrder[i]);                              // 获取页内偏移量
        int memoryIndex = findPageInMemory(*pageTable, memoryBlocks, pageNumber); // 获取页面所在内存块号
        if (memoryIndex != -1)
            printf("指令地址%4d 在内存中,物理地址:块号%2d ,偏移量%3d \n", instructionOrder[i], memoryIndex, offset);
        else
        {
            (*pageFaults)++; // 缺页次数加一
            switch (algorithm)
            {
            case 0:
                optPageReplace(pageTable, memoryBlocks, pageNumber, i);
                break;
            case 1:
                fifoPageReplace(memoryBlocks, pageNumber, &fifoIndex);
                break;
            case 2:
                lruPageReplace(pageTable, memoryBlocks, pageNumber);
                break;
            default:
                printf("当前算法不存在\n");
                break;
            }
            pageTable->pages[pageNumber].inMemory = 1; // 将新页面标记为在内存
            printf("指令地址%4d 不在内存中,发生缺页,已调入内存\n", instructionOrder[i]);
        }
        pageTable->pages[pageNumber].lastUsed = currentTime++; // 更新最近最久使用次数
    }
}

void printPageFaultRate(int pageFaults, int algorithm)
{
    double pageFaultRate = (double)pageFaults / INSTRUCTION_NUM; // 计算缺页率
    double hitRate = 1 - pageFaultRate;                          // 计算命中率
    switch (algorithm)
    {
    case 0:
        printf("使用OPT最佳置换算法:");
        break;
    case 1:
        printf("使用FIFO先进先出算法:");
        break;
    case 2:
        printf("使用LRU最近最久使用算法:");
        break;
    default:
        break;
    }
    printf("缺页率:%.2f,命中率:%.2f\n", pageFaultRate, hitRate); // 打印结果
}

int main()
{
    PageTable pageTable;
    MemoryBlock memoryBlocks[MEMORY_BLOCKS];
    int instructionOrder[INSTRUCTION_NUM];
    int pageFaults[3] = {0, 0, 0}; // 缺页次数

    generateInstructionOrder(instructionOrder); // 生成随机指令序列

    // OPT算法模拟
    initPageTable(&pageTable); // 初始化页表
    initMemoryBlocks(memoryBlocks); // 初始化内存块
    simulateRequestPaging(&pageTable, memoryBlocks, 0, instructionOrder, &pageFaults[0]);
    printPageFaultRate(pageFaults[0], 0);
    
    // FIFO算法模拟
    initPageTable(&pageTable); // 初始化页表
    initMemoryBlocks(memoryBlocks); // 初始化内存块
    simulateRequestPaging(&pageTable, memoryBlocks, 1, instructionOrder, &pageFaults[1]);
    printPageFaultRate(pageFaults[1], 1);

    // LRU算法模拟
    initPageTable(&pageTable); // 初始化页表
    initMemoryBlocks(memoryBlocks); // 初始化内存块
    simulateRequestPaging(&pageTable, memoryBlocks, 2, instructionOrder, &pageFaults[2]);
    printPageFaultRate(pageFaults[2], 2);

    return 0;
}