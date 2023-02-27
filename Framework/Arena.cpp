/*
* Barebones Arena allocator, for now this is not used
* as I did not implement any allocation/deallocation strategies yet.
*/

#include <Windows.h>
#include <cinttypes>

typedef struct Arena {
    void* start;
    size_t size;
    void* current;
} Arena;

int ArenaCreate(Arena* arena, size_t size) {

    if (arena == NULL) {
        return -1;
    }

    arena->start = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (arena->start == NULL) {
        return -2;
    }

    arena->size = size;
    arena->current = arena->start;

    return 0;
}

void* ArenaAlloc(Arena* arena, size_t size) {
    if (arena == NULL || size == 0) {
        return NULL;
    }

    //todo: implement a allocation strategy

    void* ptr = arena->current;
    size_t remaining = arena->size - (*(int64_t*)arena->current - *(int64_t*)arena->start);
    if (remaining < size) {
        return NULL;
    }

    (*(int64_t*)arena->current) += size;
    return ptr;
}

void* ArenaFree(Arena* arena, void* block, size_t size)
{
    if (size <= 0) return NULL;

    if (arena == NULL || block == NULL) {
        return 0;
    }

    //todo: implement a dealocation strategy
    return 0;
}

void Arena_Destroy(Arena* arena) {
    if (arena == NULL) {
        return;
    }

    VirtualFree(arena->start, 0, MEM_RELEASE);
}
