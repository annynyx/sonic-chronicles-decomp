// A memória do jogo: o new/delete da BioWare, com 5 "pools" de blocos pequenos, e o
// NitroStartUp, que monta os heaps antes do main. 0x0200325c-0x020034ec.
//
// Os nomes são nossos (o jogo não guarda nenhum), exceto os que o C++ e o NitroSDK
// impõem: operator new/delete e NitroStartUp, que o crt0 do SDK chama antes do main.
//
// Como funciona: um pedido pequeno vai para o primeiro pool cujo bloco caiba nele; o
// resto, e o que não couber num pool cheio, vai para o heap do NitroSDK.
#include "Aurora/Comum.h"

#define NULL 0

// Um pool de blocos do mesmo tamanho (0x18 bytes). m_pFree guarda os índices dos
// blocos livres; m_nUsed diz quantos já saíram.
class MemoryPool {
public:
    // Inline: o compilador o põe dentro do __sinit. A quantidade vem primeiro porque
    // é assim que o __sinit do jogo grava os campos.
    MemoryPool(u32 minSize, u32 blockSize, u32 count)
    {
        m_nCount = count;
        m_nMinSize = minSize;
        m_nBlockSize = blockSize;
        m_pData = NULL;
        m_pFree = NULL;
        m_nUsed = 0;
    }
    ~MemoryPool();
    void Init();
    BOOL CanAllocate(u32 size);
    void* Allocate();
    BOOL Owns(void* p);
    void Free(void* p);

    u32 m_nMinSize;     // +0x00: o menor pedido que este pool atende
    u32 m_nBlockSize;   // +0x04: o tamanho de cada bloco (o maior pedido)
    u32 m_nCount;       // +0x08: quantos blocos
    char* m_pData;      // +0x0c: a memória dos blocos
    u16* m_pFree;       // +0x10: índices dos blocos livres
    u32 m_nUsed;        // +0x14: quantos blocos já saíram
};

// 0x02160dfc: o heap expandido da NitroSystem e o "allocator" dele (de outro arquivo)
struct ExpHeap {
    NNSFndAllocator allocator;
    NNSFndHeapHandle heap;
};
extern ExpHeap g_expHeap;

// .bss do arquivo: 0x02110f34 (o sinalizador) e 0x02110f74 (os pools). Entre os dois
// ficam as 5 variáveis que o compilador cria para destruir cada pool no fim.
static BOOL s_bPoolsReady;
static MemoryPool s_pools[5] = {
    MemoryPool(0, 16, 2300),
    MemoryPool(16, 32, 1300),
    MemoryPool(32, 56, 1100),
    MemoryPool(64, 64, 70),
    MemoryPool(128, 128, 120),
};

// 0x0200325c
MemoryPool::~MemoryPool()
{
    if (m_pData != NULL) {
        delete[] m_pData;
        m_pData = NULL;
    }
    if (m_pFree != NULL) {
        delete[] m_pFree;
        m_pFree = NULL;
    }
}

// 0x02003280: aloca os blocos e põe todos na lista de livres.
void MemoryPool::Init()
{
    m_pData = new char[m_nCount * m_nBlockSize];
    m_pFree = new u16[m_nCount];
    for (u32 i = 0; i < m_nCount; i++)
        m_pFree[i] = i;
}

// 0x020032b4
BOOL MemoryPool::CanAllocate(u32 size)
{
    return size >= m_nMinSize && size <= m_nBlockSize && m_nUsed < m_nCount;
}

// 0x020032d0. O índice do bloco é lido numa linha própria: numa expressão só, o
// compilador lê m_pData antes, e o jogo lê depois.
void* MemoryPool::Allocate()
{
    u16 index = m_nUsed++;
    u16 block = m_pFree[index];
    return m_pData + block * m_nBlockSize;
}

// 0x020032ec
BOOL MemoryPool::Owns(void* p)
{
    if (p >= m_pData && p < m_pData + m_nCount * m_nBlockSize)
        return TRUE;
    return FALSE;
}

// 0x02003308. Um erro do jogo: m_nUsed não diminui. O bloco devolvido nunca volta a
// ser usado, e quando um pool enche os pedidos seguintes vão para o heap.
void MemoryPool::Free(void* p)
{
    m_pFree[(u16)m_nUsed] = ((char*)p - m_pData) / m_nBlockSize;
}

// 0x02003324
static void InitPools()
{
    s_bPoolsReady = TRUE;
    for (int i = 0; i < 5; i++)
        s_pools[i].Init();
}

// 0x02003348
static void* AllocSmall(u32 size)
{
    for (int i = 0; i < 5; i++) {
        if (s_pools[i].CanAllocate(size))
            return s_pools[i].Allocate();
    }
    return NULL;
}

// 0x0200337c
static BOOL FreeSmall(void* p)
{
    for (int i = 0; i < 5; i++) {
        if (s_pools[i].Owns(p)) {
            s_pools[i].Free(p);
            return TRUE;
        }
    }
    return FALSE;
}

// 0x020033b4 e 0x020033e8: os pools são preparados no primeiro new. O caso
// size == 0 fica no fim da função, como no jogo, por causa do if (size != 0).
void* operator new(unsigned long size)
{
    if (!s_bPoolsReady)
        InitPools();
    if (size != 0) {
        void* p = AllocSmall(size);
        if (p != NULL)
            return p;
        return OS_AllocFromHeap(OS_ARENA_MAIN, 0, size);
    }
    return NULL;
}

void* operator new[](unsigned long size)
{
    if (!s_bPoolsReady)
        InitPools();
    if (size != 0) {
        void* p = AllocSmall(size);
        if (p != NULL)
            return p;
        return OS_AllocFromHeap(OS_ARENA_MAIN, 0, size);
    }
    return NULL;
}

// 0x0200341c e 0x02003438
void operator delete(void* p)
{
    if (p != NULL && !FreeSmall(p))
        OS_FreeToHeap(OS_ARENA_MAIN, 0, p);
}

void operator delete[](void* p)
{
    if (p != NULL && !FreeSmall(p))
        OS_FreeToHeap(OS_ARENA_MAIN, 0, p);
}

// 0x02003454: o crt0 do NitroSDK chama esta função antes do main. Ela cria o heap
// principal com a memória livre, menos os últimos 40 KB, e com esses 40 KB um heap
// expandido da NitroSystem. A subtração em ponteiro ((char*)top - 0xa000) impede o
// compilador de juntar o - 0xa000 com o + 31, como o jogo.
extern "C" void NitroStartUp(void)
{
    OS_Init();
    void* lo = OS_GetArenaLo(OS_ARENA_MAIN);
    void* hi = (char*)OS_GetArenaHi(OS_ARENA_MAIN) - 0xa000;
    lo = OS_InitAlloc(OS_ARENA_MAIN, lo, hi, 1);
    OS_SetArenaLo(OS_ARENA_MAIN, lo);
    lo = (void*)(((u32)lo + 31) & ~31);
    hi = (void*)(((u32)hi + 31) & ~31);
    OSHeapHandle heap = OS_CreateHeap(OS_ARENA_MAIN, lo, hi);
    OS_SetCurrentHeap(OS_ARENA_MAIN, heap);
    OS_SetArenaLo(OS_ARENA_MAIN, hi);
    void* top = OS_GetArenaHi(OS_ARENA_MAIN);
    u32 size = ((u32)OS_GetArenaHi(OS_ARENA_MAIN) & ~31) - (((u32)((char*)top - 0xa000) + 31) & ~31);
    void* mem = OS_AllocFromArenaHi(OS_ARENA_MAIN, size, 32);
    g_expHeap.heap = NNS_FndCreateExpHeapEx(mem, size, 0);
    NNS_FndInitAllocatorForExpHeap(&g_expHeap.allocator, g_expHeap.heap, 32);
}
