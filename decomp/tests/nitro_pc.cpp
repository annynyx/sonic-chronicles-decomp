// As funções do NitroSDK e do MSL que o nosso C++ do jogo chama, feitas para o PC.
//
// Servem ao teste com dados reais (test_arquivos.cpp): compilado com o g++, o mesmo
// src/Aurora/*.cpp que gera os bytes do jogo roda aqui sobre os arquivos extraídos da
// ROM. Não é o código da Nintendo: é o comportamento que o jogo espera de cada função,
// o suficiente para o teste.
//
// O sistema de arquivos do cartucho (NitroFS) vira uma pasta: g_pastaPC, onde o
// montar_rom.sh extraiu os arquivos (work/extract/files). O NitroFS não diferencia
// maiúsculas, e o jogo pede os nomes em maiúsculas ("./TEST.HERF"), então o nome é
// procurado na pasta sem diferenciar.
#include <ctype.h>
#include <dirent.h>
#include "Aurora/Comum.h"
#include "Aurora/CExoString.h"
#include "Aurora/CFixed.h"
#include "msl.h"

const char* g_pastaPC = ".";
int g_idiomaPC = 1;              // OSOwnerInfo.language: 1 = inglês

// --- MSL --------------------------------------------------------------------------------
const char* const g_emptyString = "";
u8 __lower_map[128];

static struct IniciaTabela {
    IniciaTabela()
    {
        for (int i = 0; i < 128; i++)
            __lower_map[i] = (u8)tolower(i);
    }
} s_iniciaTabela;

extern "C" char* strupr(char* s)
{
    for (char* p = s; *p; p++)
        *p = (char)toupper((unsigned char)*p);
    return s;
}

extern "C" char* strlwr(char* s)
{
    for (char* p = s; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return s;
}

extern "C" int stricmp(const char* a, const char* b)
{
    return strcasecmp(a, b);
}

// 0x020b7dac (de outro arquivo do jogo): CRC-32 sem o XOR final, começando em 0, com a
// tabela de 0x021ad6d4 (o polinômio de sempre, 0xedb88320).
u32 Crc32(const CExoString& string, u32 length)
{
    static u32 tabela[256];
    if (tabela[1] == 0) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (c >> 1) ^ 0xedb88320 : c >> 1;
            tabela[i] = c;
        }
    }
    const u8* p = (const u8*)string.CStr();
    u32 crc = 0;
    for (u32 i = 0; i < length; i++)
        crc = tabela[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc;
}

// 0x020c0a0c (de outro arquivo do jogo): o ponto fixo a partir de um float, valor * 4096
// truncado (o _f_ftoi do jogo corta para zero, como o cast do C).
CFixed::CFixed(float value) : m_value((fx32)(value * 4096.0f)) {}

// --- NitroSDK: matemática, sistema e memória ---------------------------------------------
extern "C" fx32 FX_Div(fx32 numer, fx32 denom)
{
    return (fx32)(((long long)numer << 12) / denom);
}

extern "C" void OS_Sleep(u32) {}

extern "C" void OS_GetOwnerInfo(OSOwnerInfo* info)
{
    memset(info, 0, sizeof(*info));
    info->language = (u8)g_idiomaPC;
}

extern "C" void DC_FlushRange(const void*, u32) {}

extern "C" void MI_CpuCopy8(const void* src, void* dest, u32 size)
{
    memmove(dest, src, size);
}

// As três compressões da BIOS (GBATEK, "BIOS Decompression Functions"). O cabeçalho de
// 4 bytes tem o tipo no byte 0 e o tamanho descomprimido nos outros 3.
extern "C" void MI_UncompressLZ8(const void* srcp, void* destp)
{
    const u8* s = (const u8*)srcp;
    u8* d = (u8*)destp;
    u32 tamanho = (s[1] | s[2] << 8 | s[3] << 16);
    u32 feito = 0;
    s += 4;
    while (feito < tamanho) {
        u8 flags = *s++;
        for (int i = 0; i < 8 && feito < tamanho; i++, flags <<= 1) {
            if (!(flags & 0x80)) {
                d[feito++] = *s++;
                continue;
            }
            u32 n = (s[0] >> 4) + 3;
            u32 distancia = ((s[0] & 0xf) << 8 | s[1]) + 1;
            s += 2;
            while (n-- && feito < tamanho) {
                d[feito] = d[feito - distancia];
                feito++;
            }
        }
    }
}

extern "C" void MI_UncompressRL8(const void* srcp, void* destp)
{
    const u8* s = (const u8*)srcp;
    u8* d = (u8*)destp;
    u32 tamanho = (s[1] | s[2] << 8 | s[3] << 16);
    u32 feito = 0;
    s += 4;
    while (feito < tamanho) {
        u8 flag = *s++;
        if (flag & 0x80) {
            u32 n = (flag & 0x7f) + 3;
            u8 b = *s++;
            while (n-- && feito < tamanho)
                d[feito++] = b;
        } else {
            u32 n = (flag & 0x7f) + 1;
            while (n-- && feito < tamanho)
                d[feito++] = *s++;
        }
    }
}

extern "C" void MI_UncompressHuffman(const void* srcp, void* destp)
{
    const u8* s = (const u8*)srcp;
    u8* d = (u8*)destp;
    u32 bits = s[0] & 0xf;
    u32 tamanho = (s[1] | s[2] << 8 | s[3] << 16);
    const u8* arvore = s + 4;
    const u8* fluxo = arvore + (arvore[0] + 1) * 2;
    u32 saida = 0, nsaida = 0, feito = 0;
    const u8* no = arvore + 1;
    while (feito < tamanho) {
        u32 palavra = fluxo[0] | fluxo[1] << 8 | fluxo[2] << 16 | (u32)fluxo[3] << 24;
        fluxo += 4;
        for (int i = 31; i >= 0 && feito < tamanho; i--) {
            int b = (palavra >> i) & 1;
            const u8* filhos = (const u8*)(((size_t)no & ~(size_t)1) + (*no & 0x3f) * 2 + 2);
            const u8* filho = filhos + b;
            if (*no & (b ? 0x40 : 0x80)) {
                saida |= (u32)*filho << nsaida;
                nsaida += bits;
                no = arvore + 1;
                if (nsaida == 8) {
                    d[feito++] = (u8)saida;
                    saida = nsaida = 0;
                }
            } else {
                no = filho;
            }
        }
    }
}

// --- NitroSDK: sistema de arquivos ---------------------------------------------------------
// O FILE* fica no campo arc do FSFile, porque o jogo copia o FSFile de um lado para o
// outro (do arquivo aberto para o pedido de leitura) e a cópia tem de continuar valendo.
static FILE* arquivo(const FSFile* f)
{
    return (FILE*)f->arc;
}

extern "C" void FS_Init(u32) {}
extern "C" BOOL FS_IsAvailable(void) { return TRUE; }
extern "C" void FS_End(void) {}
extern "C" u32 FS_SetDefaultDMA(u32 dma_no) { return dma_no; }

extern "C" u32 FS_TryLoadTable(void*, u32)
{
    return 4;
}

extern "C" void FS_InitFile(FSFile* p_file)
{
    memset(p_file, 0, sizeof(*p_file));
}

extern "C" BOOL FS_OpenFile(FSFile* p_file, const char* path)
{
    if (strncmp(path, "./", 2) == 0)
        path += 2;
    DIR* pasta = opendir(g_pastaPC);
    if (pasta == NULL)
        return FALSE;
    char caminho[1024] = "";
    for (struct dirent* e = readdir(pasta); e != NULL; e = readdir(pasta)) {
        if (strcasecmp(e->d_name, path) == 0) {
            snprintf(caminho, sizeof(caminho), "%s/%s", g_pastaPC, e->d_name);
            break;
        }
    }
    closedir(pasta);
    FILE* fp = caminho[0] ? fopen(caminho, "rb") : NULL;
    if (fp == NULL)
        return FALSE;
    fseek(fp, 0, SEEK_END);
    p_file->arc = fp;
    p_file->prop.file.top = 0;
    p_file->prop.file.bottom = (u32)ftell(fp);
    p_file->prop.file.pos = 0;
    return TRUE;
}

extern "C" BOOL FS_CloseFile(FSFile* p_file)
{
    if (arquivo(p_file) != NULL)
        fclose(arquivo(p_file));
    p_file->arc = NULL;
    return TRUE;
}

extern "C" BOOL FS_SeekFile(FSFile* p_file, s32 offset, FSSeekFileMode origin)
{
    u32 base = origin == FS_SEEK_SET ? 0 : origin == FS_SEEK_CUR ? p_file->prop.file.pos
                                                                 : p_file->prop.file.bottom;
    p_file->prop.file.pos = base + offset;
    return p_file->prop.file.pos <= p_file->prop.file.bottom;
}

extern "C" s32 FS_ReadFile(FSFile* p_file, void* dst, s32 len)
{
    fseek(arquivo(p_file), p_file->prop.file.top + p_file->prop.file.pos, SEEK_SET);
    s32 lidos = (s32)fread(dst, 1, len, arquivo(p_file));
    p_file->prop.file.pos += lidos;
    return lidos;
}

// No PC a leitura "assíncrona" termina na hora: o arquivo fica livre (stat bit 0 = 0) e
// com sucesso (error = 0), como o jogo vê quando o cartucho acaba de ler.
extern "C" s32 FS_ReadFileAsync(FSFile* p_file, void* dst, s32 len)
{
    s32 lidos = FS_ReadFile(p_file, dst, len);
    p_file->stat = 0;
    p_file->error = 0;
    return lidos;
}

extern "C" BOOL FS_WaitAsync(FSFile*) { return TRUE; }
extern "C" void FS_CancelFile(FSFile*) {}

// --- NitroSDK: heap (o alocador do jogo, src/Aurora/Memory.cpp, pede ao heap do SDK o que
// não cabe nos pools) -------------------------------------------------------------------------
// O heap do NitroSDK arredonda cada bloco para um múltiplo de 32 bytes, e o alocador do
// jogo depende disso sem saber: quando um pool esgota, devolver um bloco dele grava 2 bytes
// logo depois do fim da lista de livres, que cai nessa folga (DIARIO 27). Com o malloc
// sem folga, essa gravação estraga o heap do PC.
extern "C" void* OS_AllocFromHeap(OSArenaId, OSHeapHandle, u32 size)
{
    return malloc((size + 31) & ~31u);
}

extern "C" void OS_FreeToHeap(OSArenaId, OSHeapHandle, void* ptr)
{
    free(ptr);
}

// O NitroStartUp (que monta os heaps no DS) não é chamado no PC; estas só existem para
// ele ligar.
struct ExpHeap {
    NNSFndAllocator allocator;
    NNSFndHeapHandle heap;
};
ExpHeap g_expHeap;
static void NaoNoPC(const char* nome)
{
    fprintf(stderr, "%s não existe no PC\n", nome);
    abort();
}
extern "C" void OS_Init(void) { NaoNoPC("OS_Init"); }
extern "C" void* OS_GetArenaLo(OSArenaId) { NaoNoPC("OS_GetArenaLo"); return NULL; }
extern "C" void* OS_GetArenaHi(OSArenaId) { NaoNoPC("OS_GetArenaHi"); return NULL; }
extern "C" void OS_SetArenaLo(OSArenaId, void*) { NaoNoPC("OS_SetArenaLo"); }
extern "C" void* OS_AllocFromArenaHi(OSArenaId, u32, u32) { NaoNoPC("OS_AllocFromArenaHi"); return NULL; }
extern "C" void* OS_InitAlloc(OSArenaId, void*, void*, int) { NaoNoPC("OS_InitAlloc"); return NULL; }
extern "C" OSHeapHandle OS_CreateHeap(OSArenaId, void*, void*) { NaoNoPC("OS_CreateHeap"); return 0; }
extern "C" OSHeapHandle OS_SetCurrentHeap(OSArenaId, OSHeapHandle) { NaoNoPC("OS_SetCurrentHeap"); return 0; }
extern "C" NNSFndHeapHandle NNS_FndCreateExpHeapEx(void*, u32, u16) { NaoNoPC("NNS_FndCreateExpHeapEx"); return NULL; }
extern "C" void NNS_FndInitAllocatorForExpHeap(NNSFndAllocator*, NNSFndHeapHandle, int) { NaoNoPC("NNS_FndInitAllocatorForExpHeap"); }
