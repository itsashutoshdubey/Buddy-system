#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define MAX_MEM 1024  // Total memory size
#define MIN_BLOCK 128 // Isse chota tukda nahi hoga

typedef struct Block {
    int size;           // Block ki size
    struct Block* next; // Agle khali block ka pata
} Block;

Block* free_lists[11]; // Index 7=128, 8=256, 9=512, 10=1024
void* base_addr;       // Poori memory ka starting point


void split(int order) {
    if (order <= 7 || free_lists[order] == NULL) return;

    Block* big = free_lists[order]; // Bada block uthaya
    free_lists[order] = big->next;  // List se hata diya

    int small_size = big->size / 2; // Size aadha kiya
    Block* buddy1 = big;            // Pehla dabba wahi shuru hoga
    Block* buddy2 = (Block*)((char*)big + small_size); // Dusra dabba size dur shuru hoga

    buddy1->size = small_size;
    buddy2->size = small_size;

    // Dono ko choti list mein daal diya
    buddy1->next = buddy2;
    buddy2->next = free_lists[order - 1];
    free_lists[order - 1] = buddy1;
    printf("Split %dKB into two %dKB buddies\n", big->size * 2, small_size);
}

void* allocate(int request) {
    // Request ko next power of 2 mein badla (e.g., 200 -> 256)
    int size = (request <= 128) ? 128 : (request <= 256) ? 256 : (request <= 512) ? 512 : 1024;
    int order = (int)log2(size);

    for (int i = order; i <= 10; i++) {
        if (free_lists[i] != NULL) {
            for (int j = i; j > order; j--) split(j); // Zarurat padne par split karo
            
            Block* allocated = free_lists[order];
            free_lists[order] = allocated->next;
            return (void*)allocated;
        }
    }
    printf("Out of memory!\n");
    return NULL;
}


void deallocate(void* addr, int size) {
    int order = (int)log2(size);
    
    // XOR: Buddy ka relative address nikalna
    uintptr_t rel_addr = (uintptr_t)addr - (uintptr_t)base_addr;
    void* buddy_addr = (void*)((rel_addr ^ size) + (uintptr_t)base_addr);

    Block* temp = free_lists[order];
    Block* prev = NULL;
    int found = 0;

    while (temp != NULL) {
        if (temp == (Block*)buddy_addr) { found = 1; break; } // Buddy mil gaya!
        prev = temp; temp = temp->next;
    }

    if (found && size < MAX_MEM) {
        printf("Buddy found! Merging %dKB blocks...\n", size);
        if (prev == NULL) free_lists[order] = temp->next;
        else prev->next = temp->next;

        void* merged_addr = (addr < buddy_addr) ? addr : buddy_addr;
        deallocate(merged_addr, size * 2); // Bada block banakar merge karte jao
    } else {
        Block* b = (Block*)addr;
        b->size = size;
        b->next = free_lists[order];
        free_lists[order] = b;
        printf("Free block of %dKB added back to list.\n", size);
    }
}

int main() {
    base_addr = malloc(MAX_MEM); // Memory khareedi
    Block* initial = (Block*)base_addr;
    initial->size = 1024;
    initial->next = NULL;
    free_lists[10] = initial; // 1024KB wale list mein daala

    void* p1 = allocate(210); // 256KB milega
    printf("\nStarting Deallocation\n");
    deallocate(p1, 256);      // Wapas merge ho jayega
    return 0;
}