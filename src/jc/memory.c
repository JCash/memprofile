#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <string.h> // strlen

#if defined(__APPLE__)
    #include <malloc/malloc.h>
#endif

#define FNNAME(_NAME) _NAME

#if defined(__APPLE__)
    #undef FNNAME
    #define FNNAME(_NAME) p##_NAME
    #define MEMUSED(_PTR) malloc_size(_PTR)

#elif defined(__linux__)
    #define MEMUSED(_PTR) malloc_usable_size(_PTR)

#elif defined(_MSC_VER)
    #define MEMUSED(_PTR) _msize(_PTR)
    // not supported yet, but still...
#endif

typedef volatile uint64_t uint64_atomic_t;

uint64_atomic_t g_TotalNumAllocations = 0;
uint64_atomic_t g_TotalMemAllocated = 0; 
uint64_atomic_t g_NumAllocations = 0;        // Current
uint64_atomic_t g_MemAllocated = 0;          // Current
uint64_atomic_t g_Initialized = 0;           // Set after the initalization of the profiler is done

#if defined(__GNUC__)
    #define ATOMICADD(_PTR, _VALUE)     __sync_fetch_and_add(_PTR, _VALUE)
    #define ATOMICSUB(_PTR, _VALUE)     __sync_fetch_and_sub(_PTR, _VALUE)
#else // windows
    #define ATOMICADD(_PTR, _VALUE)     InterlockedAdd64(_PTR, _VALUE)
    #define ATOMICSUB(_PTR, _VALUE)     InterlockedSub64(_PTR, _VALUE)
#endif

//#define DEBUG_ALLOCATIONS

#if defined(DEBUG_ALLOCATIONS)
#include <stdio.h>
#include <stdarg.h>
void debug(const char* format, ...)
{
    char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    write(STDERR_FILENO, buffer, strlen(buffer));
    fflush(stderr);
}
#else
    #define debug(...)
#endif

void* FNNAME(malloc)(size_t size)
{
    if (g_Initialized == 0)
        return malloc(size);
    if (size == 0)
        return 0;
    void* ptr = malloc(size);
    // The allocated size may be larger than the requested size
    // so in order to avoid over flow when subtracting later...
    size_t size2 = (size_t)MEMUSED(ptr);
    debug("malloc: %zu (%zu) %p\n", size2, size, ptr);
    size = size2;
    ATOMICADD(&g_TotalNumAllocations, 1);
    ATOMICADD(&g_NumAllocations, 1);
    ATOMICADD(&g_TotalMemAllocated, size);
    ATOMICADD(&g_MemAllocated, size);
    return ptr;
}

void* FNNAME(calloc)(size_t nmemb, size_t size)
{
    if (g_Initialized == 0)
        return calloc(nmemb, size);
    size *= nmemb;
    if (size == 0)
        return 0;
    void* ptr = malloc(size);
    memset(ptr, 0, size);
    // The allocated size may be larger than the requested size
    // so in order to avoid over flow when subtracting later...
    size_t size2 = (size_t)MEMUSED(ptr);
    debug("calloc: %zu (%zu) %p\n", size2, size, ptr);
    size = size2;
    ATOMICADD(&g_TotalNumAllocations, 1);
    ATOMICADD(&g_NumAllocations, 1);
    ATOMICADD(&g_TotalMemAllocated, size);
    ATOMICADD(&g_MemAllocated, size);
    return ptr;
}

void* FNNAME(realloc)(void* oldptr, size_t size)
{
    if (g_Initialized == 0)
        return realloc(oldptr, size);

    void* ptr = realloc(oldptr, size);
    // The allocated size may be larger than the requested size
    // so in order to avoid over flow when subtracting later...
    size = MEMUSED(ptr);

    size_t oldsize = MEMUSED(oldptr);
    debug("realloc: %zu (%zu) %p\n", size, oldsize, ptr);
    ATOMICADD(&g_TotalNumAllocations, 1);
    if(oldptr == 0)
        ATOMICADD(&g_NumAllocations, 1);

    ATOMICSUB(&g_TotalMemAllocated, oldsize);
    ATOMICSUB(&g_MemAllocated, oldsize);
    ATOMICADD(&g_TotalMemAllocated, size);
    ATOMICADD(&g_MemAllocated, size);
    return ptr;
}

void FNNAME(free)(void* ptr)
{
    if (g_Initialized == 0)
    {
        free(ptr);
        return;
    }
    if (!ptr)
        return;
    debug("Free: %zu %p\n", MEMUSED(ptr), ptr);
    ATOMICSUB(&g_NumAllocations, 1);
    ATOMICSUB(&g_MemAllocated, (uint64_t)MEMUSED(ptr));
    free(ptr);
}


#if defined(__APPLE__)
    // It seems that nowadays, you need both DYLD_INSERT_LIBRARIES and DYLD_INTERPOSE
    // https://stackoverflow.com/questions/34114587/dyld-library-path-dyld-insert-libraries-not-working
    #define DYLD_INTERPOSE(_replacment,_replacee) \
        __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
        __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

    DYLD_INTERPOSE(pcalloc, calloc);
    DYLD_INTERPOSE(pmalloc, malloc);
    DYLD_INTERPOSE(prealloc, realloc);
    DYLD_INTERPOSE(pfree, free);
#endif


typedef void* (*malloc_t)(size_t);

static __attribute__((constructor)) void Initialize(void)
{
    malloc_t real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");

    ATOMICADD(&g_Initialized, 1);
}

void __attribute__ ((destructor)) Finalize()
{
    // avoid any extra allocations due to the printf
    uint64_t total_num_allocations = g_TotalNumAllocations;
    uint64_t total_mem_allocated = g_TotalMemAllocated;
    uint64_t num_allocations = g_NumAllocations;
    uint64_t mem_allocated = g_MemAllocated;

    fprintf(stderr, "Memory used: \n");
    fprintf(stderr, "   Total:\t%llu bytes in %llu allocation(s)\n", total_mem_allocated, total_num_allocations);
    fprintf(stderr, "   At exit:\t%llu bytes in %llu allocation(s)\n", mem_allocated, num_allocations);
}
