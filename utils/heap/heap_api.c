#include "multi_heap.h"
#include "heap_api.h"
#include "string.h"
#if 0
extern uint32_t __HeapBase;
extern uint32_t  __HeapLimit;
static heap_handle_t global_heap = NULL;

void *malloc(size_t size)
{
    if (global_heap == NULL)
        global_heap = heap_register((void *)&__HeapBase,(&__HeapLimit - &__HeapBase));
    return heap_malloc(global_heap,size);
}

void free(void *ptr)
{
    heap_free(global_heap,ptr);
}

void *calloc(size_t nmemb, size_t size)
{
    void *ptr = malloc(nmemb*size);
    if (ptr != NULL)
        memset(ptr,0,nmemb*size);
    return ptr;
}
void *realloc(void *ptr, size_t size)
{
    return heap_realloc(global_heap,ptr,size);
}
#endif

void heap_memory_info(heap_handle_t heap,
                    size_t *total,
                    size_t *used,
                    size_t *max_used)
{
    multi_heap_info_t info;
    heap_get_info(heap,&info);
    if (total != NULL)
        *total = info.total_bytes;
    if (used  != NULL)
        *used = info.total_allocated_bytes;
    if (max_used != NULL)
        *max_used = info.total_bytes - info.minimum_free_bytes;
}

static heap_handle_t g_med_heap = NULL;

void med_heap_init(void *begin_addr, size_t size)
{
    memset(begin_addr,0,size);
    g_med_heap = heap_register(begin_addr,size);
}

void *med_malloc(size_t size)
{
    return heap_malloc(g_med_heap,size);
}

void med_free(void *p)
{
    heap_free(g_med_heap,p);
}

void *med_calloc(size_t nmemb, size_t size)
{
    void *ptr = med_malloc(nmemb*size);
    if (ptr != NULL)
        memset(ptr,0,nmemb*size);
    return ptr;
}

void *med_realloc(void *ptr, size_t size)
{
    return heap_realloc(g_med_heap,ptr,size);
}

void med_memory_info(size_t *total,
                    size_t *used,
                    size_t *max_used)
{
    heap_memory_info(g_med_heap,total,used,max_used);
}

