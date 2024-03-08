#ifndef OS_H
#define OS_H

#include "general.h"
#include "memory_manager.h"

char* os_read_file_as_string(const char *filepath);
b32   os_write_file(const char *filepath, Memory_Manager *memory_manager);
void* os_allocate_memory(size_t size);
void  os_free_memory(void *buffer);

#endif // OS_H

