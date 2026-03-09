#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define MAX_MEM 1024  // Total memory size
#define MIN_BLOCK 128 // Minimum block size limit

typedef struct Block
{
    int size;           // Block ka size
    struct Block *next; // Pointer to the next free block
} Block;

// free_lists[11] manages blocks from 2^0 to 2^10.
// Index 7 = 128, 8 = 256, 9 = 512, 10 = 1024
Block *free_lists[11];
void *base_addr; // Global starting address of our memory pool


void split(int order)
{
    // Agar block 128 (order 7) se chota hai ya list khali hai toh split nahi hoga
    if (order <= 7 || free_lists[order] == NULL)
        return;

    Block *big = free_lists[order]; // Bada block list se nikala
    free_lists[order] = big->next;

    int small_size = big->size / 2;                      // Size aadha kiya
    Block *buddy1 = big;                                 // Pehla buddy wahi shuru hoga jahan bada block tha
    Block *buddy2 = (Block *)((char *)big + small_size); // Dusra buddy small_size door shuru hoga

    buddy1->size = small_size;
    buddy2->size = small_size;

    // Dono buddies ko nichli (order-1) free list mein joda
    buddy1->next = buddy2;
    buddy2->next = free_lists[order - 1];
    free_lists[order - 1] = buddy1;

    printf("Split %dKB into two %dKB buddies\n", small_size * 2, small_size);
}

void *allocate(int request)
{
    // Request ko next power of 2 mein badla
    int size;
    if (request <= 128)
        size = 128;
    else if (request <= 256)
        size = 256;
    else if (request <= 512)
        size = 512;
    else if (request <= 1024)
        size = 1024;
    else
    {
        printf("Request too large!\n");
        return NULL;
    }

    int order = (int)log2(size);

    // Sahi size ka block dhoondo, nahi mile toh bade ko split karo
    for (int i = order; i <= 10; i++)
    {
        if (free_lists[i] != NULL)
        {
            for (int j = i; j > order; j--)
            {
                split(j);
            }

            Block *allocated = free_lists[order];
            free_lists[order] = allocated->next;
            printf("Allocated %dKB at address %p\n", size, (void *)allocated);
            return (void *)allocated;
        }
    }

    printf("Out of memory!\n");
    return NULL;
}


void deallocate(void *addr, int size)
{
    int order = (int)log2(size);

    // XOR MAGIC: Buddy ka relative address calculate karna
    uintptr_t rel_addr = (uintptr_t)addr - (uintptr_t)base_addr;
    void *buddy_addr = (void *)((rel_addr ^ size) + (uintptr_t)base_addr);

    Block *temp = free_lists[order];
    Block *prev = NULL;
    int found = 0;

    // Check karo kya buddy free list mein hai?
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
        printf("Buddy found! Merging two %dKB blocks...\n", size);

        // Buddy ko free list se hatao kyunki ab wo merge ho raha hai
        if (prev == NULL)
            free_lists[order] = temp->next;
        else
            prev->next = temp->next;

        // Dono mein se jo address pehle aata hai, wahan se naya bada block shuru hoga
        void *merged_addr = (addr < buddy_addr) ? addr : buddy_addr;
        deallocate(merged_addr, size * 2); // Recursive merge
    }
    else
    {
        // Buddy busy hai ya max size pahunch gaye, toh block ko free list mein daal do
        Block *b = (Block *)addr;
        b->size = size;
        b->next = free_lists[order];
        free_lists[order] = b;
        printf("Free block of %dKB added back to list.\n", size);
    }
}

int main()
{
    // Poori 1024KB memory OS se allocate ki
    base_addr = malloc(MAX_MEM);
    if (base_addr == NULL)
    {
        printf("Failed to allocate initial memory!\n");
        return 1;
    }

    // Initial state: Ek poora 1024KB ka free block
    Block *initial = (Block *)base_addr;
    initial->size = 1024;
    initial->next = NULL;
    free_lists[10] = initial;

    printf("--- Buddy System Initialized (1024KB) ---\n\n");

    // Test Case: 210KB request (Should get 256KB)
    void *p1 = allocate(210);

    printf("\n--- Starting Deallocation ---\n");
    deallocate(p1, 256); // Wapas merge ho kar 1024 ban jayega

    // Cleanup
    free(base_addr);
    return 0;
}