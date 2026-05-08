#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#define MAX_MEM 1024
#define MIN_BLOCK 128
#define CANARY_MAGIC 0xDEADC0DE // security
// GLOBAL
void *base_addr;
//  Thread-Safety
pthread_mutex_t allocator_lock = PTHREAD_MUTEX_INITIALIZER;


typedef struct Block
{
    uint32_t canary; // For Buffer Overflow detection
    int size;
    struct Block *next;
} __attribute__((aligned(8))) Block;

Block *free_lists[11];


int get_order(int size)
{
    int order = 0;
    while ((1 << order) < size)
        order++;
    return order;
}


void split(int order)
{
    if (order <= 7 || free_lists[order] == NULL)
        return;

    Block *big = free_lists[order];
    free_lists[order] = big->next;

    int small_size = (1 << (order - 1)); //bitwise division by 2
    Block *buddy1 = big;
    Block *buddy2 = (Block *)((char *)big + small_size);

    buddy1->size = small_size;
    buddy2->size = small_size;

    buddy1->next = buddy2;
    buddy2->next = free_lists[order - 1];
    free_lists[order - 1] = buddy1;
}

void *allocate(int request)
{
    
    pthread_mutex_lock(&allocator_lock);

    //   8-byte Alignment 
    int aligned_req = (request + 7) & ~7;

    //  Rounding up to next power of 2
    int size = MIN_BLOCK;
    while (size < aligned_req)
        size <<= 1;

    int order = get_order(size);

    for (int i = order; i <= 10; i++)
    {
        if (free_lists[i] != NULL)
        {
            for (int j = i; j > order; j--)
                split(j);

            Block *allocated = free_lists[order];
            free_lists[order] = allocated->next;

            
            allocated->canary = CANARY_MAGIC;

            pthread_mutex_unlock(&allocator_lock);
            return (void *)allocated;
        }
    }

    pthread_mutex_unlock(&allocator_lock);
    return NULL;
}

//internal function for merging
void deallocate_internal(void *addr, int size)
{
    Block *block = (Block *)addr;

   
    if (block->canary != CANARY_MAGIC)
    {
        printf("CRITICAL: Memory corruption detected at %p!\n", addr);
        return;
    }

    int order = get_order(size);
    uintptr_t rel_addr = (uintptr_t)addr - (uintptr_t)base_addr;


    void *buddy_addr = (void *)((rel_addr ^ size) + (uintptr_t)base_addr);

    Block *temp = free_lists[order];
    Block *prev = NULL;
    int found = 0;

    while (temp != NULL)
    {
        if (temp == (Block *)buddy_addr)
        {
            found = 1;
            break;
        }
        prev = temp;
        temp = temp->next;
    }

    if (found && size < MAX_MEM)
    {
        if (prev == NULL)
            free_lists[order] = temp->next;
        else
            prev->next = temp->next;

        void *merged_addr = (addr < buddy_addr) ? addr : buddy_addr;
        deallocate_internal(merged_addr, size << 1); // Bitwise Multiplication (* 2)
    }
    else
    {
        block->next = free_lists[order];
        free_lists[order] = block;
    }
}


void deallocate(void *addr, int size)
{
    pthread_mutex_lock(&allocator_lock);
    deallocate_internal(addr, size);
    pthread_mutex_unlock(&allocator_lock);
}

int main()
{
    base_addr = malloc(MAX_MEM);
    if (!base_addr)
        return 1;

   
    Block *initial = (Block *)base_addr;
    initial->size = 1024;
    initial->canary = CANARY_MAGIC;
    free_lists[10] = initial;

    printf("Buddy System Initialized with Thread-Safety & Canaries.\n");

    void *p1 = allocate(200);
    if (p1)
        deallocate(p1, 256);

    free(base_addr);
    return 0;
}