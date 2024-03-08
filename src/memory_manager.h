#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "general.h"

#define MEMORY_SLOT_COUNT 12

typedef struct {
    size_t size_total;
    size_t size_used;
    void *memory;
} Memory_Slot;

typedef struct {
    size_t curr_slot_index;
    Memory_Slot slots[MEMORY_SLOT_COUNT];
} Memory_Manager;

void  memory_manager_init(Memory_Manager *manager, size_t first_slot_size);
void* memory_manager_alloc(Memory_Manager *manager, size_t size);

#endif // MEMORY_MANAGER_H
