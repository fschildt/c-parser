#include "os.h"

#ifdef OS_LINUX

#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>

void *os_allocate_memory(size_t size)
{
    void *memory = malloc(size);
    return memory;
}

void os_free_memory(void *memory)
{
    free(memory);
}

char *os_read_file_as_string(const char *filepath)
{
    int file_descriptor = open(pathname, O_RDONLY, 0);
    if (file_descriptor == -1)
    {
        printf("error: failed to open %s for reading\n", pathname);
        return 0;
    }

    struct stat file_status;
    if (fstat(file_descriptor, &file_status) == -1)
    {
        printf("error: fstat failed on %s\n", pathname);
        return 0;
    }

    char *file_as_string = (char*)os_allocate_memory(file_status.st_size + 1);
    if (!file_as_string)
    {
        printf("error: out of memory\n");
        return 0;
    }

    ssize_t file_size_read = read(file_descriptor, file_as_string, file_status.st_size);
    if (file_size_read != file_status.st_size)
    {
        printf("error: only %ld/%ld bytes read of %s\n", file_size_read, file_status.st_size, pathname);
        close(file_descriptor);
        free(file_as_string);
        return 0;
    }
    file_as_string[file_size_read] = '\0';

    close(file_descriptor);

    return file_as_string;
}

b32 os_write_file(const char *filepath, void *buffer, size_t size)
{
    int file_descriptor = open(pathname, O_WRONLY);
    if (file_descriptor == -1)
    {
        printf("error: failed to open %s for writing\n", pathname);
        return 0;
    }

    ssize_t written = write(file_descriptor, buffer, size);
    if (written != size)
    {
        printf("error: only %ld/%d bytes written to %s\n", written, size, pathname);
        return 0;
    }

    return 1;
}

#else // no supported os specified

#include <stdlib.h>
#include <stdio.h>

void *os_allocate_memory(size_t size)
{
    void *memory = malloc(size);
    return memory;
}

void os_free_memory(void *memory)
{
    free(memory);
}

char *os_read_file_as_string(const char *filepath)
{
    FILE *fd = fopen(filepath, "r");
    if (!fd)
    {
        printf("error: file could not be opened for reading\n");
        return 0;
    }

    int seeked = fseek(fd, 0L, SEEK_END);
    int bytes  = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    if (seeked == -1 || bytes == -1)
    {
        fclose(fd);
        printf("error: file could not be read\n");
        return 0;
    }

    char *buffer = os_allocate_memory(bytes + 1);
    if (!buffer)
    {
        fclose(fd);
        printf("error: out of memory while reading file\n");
        return 0;
    }

    size_t read = fread(buffer, 1, bytes, fd);
    if (read != bytes)
    {
        fclose(fd);
        printf("error: file read unsuccessful\n");
        return 0;
    }

    buffer[bytes] = '\0';

    fclose(fd);
    return buffer;
}

b32 os_write_file(const char *filepath, Memory_Manager *memory_manager)
{
    FILE *fd = fopen(filepath, "w");
    if (!fd)
    {
        printf("error: %s could not be opened for writing\n", filepath);
        return false;
    }

    i32 max_slot_index = memory_manager->curr_slot_index;
    for (i32 i = 0; i <= max_slot_index; i++)
    {
        Memory_Slot *slot = &memory_manager->slots[i];
        size_t written = fwrite(slot->memory, 1, slot->size_used, fd);
        if (written != slot->size_used)
        {
            printf("error: invalid count of bytes written\n");
            return false;
        }
    }
    fclose(fd);
    return true;
}

#endif
