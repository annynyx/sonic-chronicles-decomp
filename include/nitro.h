// As funções do NitroSDK e da NitroSystem que o código do jogo chama.
// Só as declarações, com os tipos da API pública: as funções já estão no jogo
// (ligadas a partir do fonte, veja docs/BUILD.md).
#ifndef NITRO_H
#define NITRO_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef int BOOL;
typedef s32 fx32;
typedef long long s64;

#define TRUE 1
#define FALSE 0

typedef enum {
    OS_ARENA_MAIN = 0
} OSArenaId;
typedef int OSHeapHandle;

typedef enum {
    FS_SEEK_SET = 0,
    FS_SEEK_CUR = 1,
    FS_SEEK_END = 2
} FSSeekFileMode;

// FSFile: 0x48 bytes, como no NitroSDK (fs/file.h). prop e arg são unions: o
// compilador as copia em bloco (ldm/stm), e o jogo mostra isso ao copiar um FSFile.
typedef struct FSFileLink {
    struct FSFile* prev;
    struct FSFile* next;
} FSFileLink;

typedef struct OSThreadQueue {
    void* head;
    void* tail;
} OSThreadQueue;

typedef struct FSFile {
    FSFileLink link;          // +0x00
    void* arc;                // +0x08
    u32 stat;                 // +0x0c: bit 0 = ocupado (leitura assíncrona em andamento)
    u32 command;              // +0x10
    u32 error;                // +0x14: 0 = FS_RESULT_SUCCESS
    OSThreadQueue queue[1];   // +0x18
    union {
        struct {
            u32 own_id;       // +0x20
            u32 top;          // +0x24: começo do arquivo na ROM
            u32 bottom;       // +0x28: fim do arquivo na ROM
            u32 pos;          // +0x2c
        } file;
    } prop;
    union {
        u32 data[6];          // +0x30: os argumentos do comando em andamento
    } arg;
} FSFile;

typedef struct OSOwnerInfo {
    u8 language;          // 0 japonês, 1 inglês, 2 francês, 3 alemão, 4 italiano, 5 espanhol
    u8 favoriteColor;
    u8 birthday[2];
    u16 nickName[11];
    u16 nickNameLength;
    u16 comment[27];
    u16 commentLength;
} OSOwnerInfo;

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

void OS_Sleep(u32 msec);
void OS_GetOwnerInfo(OSOwnerInfo* info);
void DC_FlushRange(const void* startAddr, u32 nBytes);
void MI_CpuCopy8(const void* src, void* dest, u32 size);
void MI_UncompressLZ8(const void* srcp, void* destp);
void MI_UncompressRL8(const void* srcp, void* destp);
void MI_UncompressHuffman(const void* srcp, void* destp);

void FS_Init(u32 default_dma_no);
BOOL FS_IsAvailable(void);
void FS_End(void);
u32 FS_SetDefaultDMA(u32 dma_no);
u32 FS_TryLoadTable(void* p_mem, u32 size);
void FS_InitFile(FSFile* p_file);
BOOL FS_OpenFile(FSFile* p_file, const char* path);
BOOL FS_CloseFile(FSFile* p_file);
BOOL FS_WaitAsync(FSFile* p_file);
void FS_CancelFile(FSFile* p_file);
BOOL FS_SeekFile(FSFile* p_file, s32 offset, FSSeekFileMode origin);
s32 FS_ReadFile(FSFile* p_file, void* dst, s32 len);
s32 FS_ReadFileAsync(FSFile* p_file, void* dst, s32 len);

NNSFndHeapHandle NNS_FndCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag);
void NNS_FndInitAllocatorForExpHeap(NNSFndAllocator* pAllocator, NNSFndHeapHandle heap, int alignment);
}

// Inline no SDK (fs/file.h): o jogo as tem expandidas, sem chamada.
inline BOOL FS_IsBusy(const FSFile* p_file)
{
    return (p_file->stat & 1) ? TRUE : FALSE;
}

inline BOOL FS_IsSucceeded(const FSFile* p_file)
{
    return p_file->error == 0;
}

inline u32 FS_GetLength(const FSFile* p_file)
{
    return p_file->prop.file.bottom - p_file->prop.file.top;
}

#endif
