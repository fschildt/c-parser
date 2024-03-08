#include "memory_manager.h"
#include "os.h"
#include <stdlib.h>
#include <stdio.h>

void* memory_slot_get_memory(Memory_Slot *slot, size_t size)
{
    void *memory = 0;
    if (size + slot->size_used <= slot->size_total)
    {
        memory = slot->memory + slot->size_used;
        slot->size_used += size;
    }
    return memory;
}

void memory_slot_init(Memory_Slot *slot, size_t memory_size) {
    slot->size_used = 0;
    slot->size_total = memory_size;
    slot->memory = os_allocate_memory(memory_size);
    if (!slot->memory)
    {
        printf("error: out of memory\n");
        exit(EXIT_FAILURE);
    }
}

void* memory_manager_alloc(Memory_Manager *manager, size_t size)
{
    Memory_Slot *slot = &manager->slots[manager->curr_slot_index];

    void *memory = memory_slot_get_memory(slot, size);
    if (!memory)
    {
        if (manager->curr_slot_index >= MEMORY_SLOT_COUNT)
        {
            printf("error: out of memory slots\n");
            exit(EXIT_FAILURE);
        }

        size_t next_slot_size = slot->size_total << 1;
        while (next_slot_size < size)
        {
            next_slot_size <<= 1;
        }

        memory_slot_init(slot+1, next_slot_size);
        memory = memory_slot_get_memory(slot+1, size);
        manager->curr_slot_index++;
    }
    return memory;
}

void memory_manager_init(Memory_Manager *manager, size_t first_slot_size)
{
    manager->curr_slot_index = 0;
    memory_slot_init(&manager->slots[0], first_slot_size);
}

