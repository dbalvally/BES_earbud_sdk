#include "heap_api.h"
#include "hal_trace.h"

extern uint32_t __StackLimit;
extern uint32_t  __HeapLimit;

uint8_t *syspool_addr = NULL;
uint32_t syspool_size = 0;

static uint32_t syspoll_used = 0;
void syspool_init()
{
    syspool_addr = (uint8_t *)(&__HeapLimit);
    syspool_size = (uint8_t *)(&__StackLimit-128) - (uint8_t *)(&__HeapLimit);
    syspoll_used = 0;
    memset(syspool_addr,0,syspool_size);
    TRACE("syspool_init: 0x%x,0x%x",syspool_addr,syspool_size);
}

uint32_t syspool_free_size()
{
    return syspool_size - syspoll_used;
}

int syspool_get_buff(uint8_t **buff, uint32_t size)
{
    uint32_t buff_size_free;

    buff_size_free = syspool_free_size();

    if (size % 4){
        size = size + (4 - size % 4);
    }
    TRACE("[%s] size = %d , free size = %d", __func__, size, buff_size_free);
    if (size > buff_size_free)
    {
        ASSERT(size <= buff_size_free, "[%s] size = %d > free size = %d", __func__, size, buff_size_free);     
        return -1;
    }
       
    *buff = syspool_addr + syspoll_used;
    syspoll_used += size;
    return buff_size_free;
}

int syspool_get_available(uint8_t **buff)
{
    uint32_t buff_size_free;
    buff_size_free = syspool_free_size();

    TRACE("[%s] free size = %d", __func__, buff_size_free);
    if (buff_size_free < 8)
        return -1;
    if (buff != NULL)
    {
        *buff = syspool_addr + syspoll_used;
        syspoll_used += buff_size_free;
    }
    return buff_size_free;
}


