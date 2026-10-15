// As funções do NitroSDK e da NitroSystem que o código do jogo chama.
// Só as declarações, com os tipos da API pública: as funções já estão no jogo
// (ligadas a partir do fonte, veja docs/BUILD.md).
#ifndef NITRO_H
#define NITRO_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef int BOOL;
typedef s32 fx32;

#define TRUE 1
#define FALSE 0

typedef enum {
    OS_ARENA_MAIN = 0
} OSArenaId;
typedef int OSHeapHandle;

typedef struct NNSFndAllocator {
    void* pFunc;
    void* pHeap;
    u32 heapParam1;
    u32 heapParam2;
} NNSFndAllocator;
typedef void* NNSFndHeapHandle;

extern "C" {
fx32 FX_Div(fx32 numer, fx32 denom);

void OS_Init(void);
void* OS_GetArenaLo(OSArenaId id);
void* OS_GetArenaHi(OSArenaId id);
void OS_SetArenaLo(OSArenaId id, void* newLo);
void* OS_AllocFromArenaHi(OSArenaId id, u32 size, u32 align);
void* OS_InitAlloc(OSArenaId id, void* arenaStart, void* arenaEnd, int maxHeaps);
OSHeapHandle OS_CreateHeap(OSArenaId id, void* start, void* end);
OSHeapHandle OS_SetCurrentHeap(OSArenaId id, OSHeapHandle heap);
void* OS_AllocFromHeap(OSArenaId id, OSHeapHandle heap, u32 size);
void OS_FreeToHeap(OSArenaId id, OSHeapHandle heap, void* ptr);

NNSFndHeapHandle NNS_FndCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag);
void NNS_FndInitAllocatorForExpHeap(NNSFndAllocator* pAllocator, NNSFndHeapHandle heap, int alignment);
}

#endif
