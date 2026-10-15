// CDSFileSystem: o acesso aos arquivos do cartucho (NitroFS) e a fila de leituras
// assíncronas. Os nomes da classe e dos typeinfos vêm do jogo ("13CDSFileSystem"); os
// dos campos e métodos são nossos.
//
// Cada arquivo aberto ganha um número (id). Abrir de novo um arquivo já aberto só
// soma 1 no contador; fechar subtrai, e o arquivo fecha de verdade no zero.
#ifndef AURORA_CDSFILESYSTEM_H
#define AURORA_CDSFILESYSTEM_H

#include "nitro.h"
#include "Aurora/CExoString.h"
#include "Aurora/CExoArrayList.h"
#include "Aurora/Singleton.h"

// Nome de arquivo em maiúsculas com o seu CRC-32 (16 bytes). Dois nomes são iguais
// quando o CRC é igual.
class ResName {
public:
    ResName() { m_hash = 0; }
    ResName(const char* name);
    BOOL operator==(const ResName& other) const;

    CExoString m_name;   // +0x0
    u32 m_hash;          // +0xc
};

// Um arquivo aberto (0x60 bytes).
struct CDSOpenFile {
    // A lista de inicialização faz o nome ser construído por último (ele é o último
    // campo), como no jogo (0x02002f3c).
    CDSOpenFile() : id(0), count(0), small(0), cache(0) {}

    FSFile file;      // +0x00
    u16 id;           // +0x48: o número que o resto do jogo usa
    u16 count : 15;   // +0x4a: quantas vezes foi aberto
    u16 small : 1;    //        aberto como .SMALL (comprimido)
    int cache;        // +0x4c: posição + 1 em m_cache, ou 0
    ResName name;     // +0x50
};

// Um pedido de leitura assíncrona (0x60 bytes).
typedef void (*CDSReadCallback)(u16* id, u8* data, u32 size, void* userData);

enum {
    REQUEST_FREE = 0,       // cancelado: sai da fila
    REQUEST_WAITING = 1,    // na fila
    REQUEST_READING = 2,    // FS_ReadFileAsync em andamento
    REQUEST_DONE = 3        // pronto: sai da fila
};

struct CDSRequest {
    CDSRequest()
    {
        id = 0;
        compressed = 0;
        state = 0;
        timeout = 0;
        cached = 0;
        offset = 0;
        size = 0;
        dst = NULL;
        userData = NULL;
        callback = NULL;
    }

    FSFile file;               // +0x00: cópia do FSFile do arquivo aberto
    u16 id;                    // +0x48
    u16 compressed : 1;        // +0x4a: descomprimir ao terminar
    u16 state : 3;             //        REQUEST_...
    u16 timeout : 10;          //        quadros até desistir e recomeçar
    u16 cached : 1;            //        o arquivo está na memória (m_cache)
    u32 offset;                // +0x4c
    u32 size;                  // +0x50
    u8** dst;                  // +0x54
    void* userData;            // +0x58
    CDSReadCallback callback;  // +0x5c
};

// Um arquivo guardado inteiro na memória (8 bytes).
struct CDSCacheEntry {
    CDSCacheEntry()
    {
        data = NULL;
        unknown = 0;
        id = 0;
    }

    u8* data;      // +0x0
    u16 unknown;   // +0x4
    u16 id;        // +0x6
};

class CDSFileSystem : public Singleton<CDSFileSystem> {
public:
    CDSFileSystem() { Init(3); }
    virtual ~CDSFileSystem() { Shutdown(); }
    virtual void FreeCache();

    BOOL Init(int dma);
    BOOL Shutdown();
    void Update();
    void CancelRequests(u8* data, BOOL cancel);
    void CancelRequests(u16* id, BOOL cancel);
    void FinishRequest(CDSRequest* request, BOOL cancel);
    void AddPath(const char* path);
    BOOL OpenPath(const char* name, CExoString& path, FSFile* file);
    int FindFile(u16* id);
    int FindFile(ResName& name);
    BOOL OpenFile(const char* name, u16* id, BOOL small);
    int CloseFile(u16* id);
    u32 GetLength(FSFile* file);
    u32 GetFileSize(u16* id);
    BOOL ReadFile(u16* id, void* dst, u32 size);
    BOOL ReadFileAt(u16* id, void* dst, u32 offset, u32 size);
    BOOL ReadFileAsync(u16* id, u8** dst, u32 offset, u32 size, CDSReadCallback callback, void* userData);
    BOOL ProcessCurrent();
    BOOL StartRequest(CDSRequest* request);
    u32 Decompress(const void* src, u8** dst);
    u32 Decompress(u8** data);

    CExoArrayList<CDSCacheEntry> m_cache;   // +0x04
    CExoArrayList<CDSRequest> m_requests;   // +0x10
    CExoArrayList<CDSOpenFile> m_files;     // +0x1c
    CExoArrayList<CExoString> m_paths;      // +0x28: guardados, nunca usados
    int m_current;                          // +0x34: o pedido em leitura, ou -1
    s16 m_nextId;                           // +0x38
    s16 m_dma;                              // +0x3a
};

#endif
