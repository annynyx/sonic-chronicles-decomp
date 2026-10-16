// Os pacotes de recursos HERF: test.herf (comum) e test_X.herf (o do idioma do
// console). Cada pacote tem um índice de 12 bytes por recurso (hash, tamanho,
// posição), ordenado pelo hash, e o jogo acha um recurso por busca binária.
// O formato está em docs/FORMATOS.md. Os nomes CErfMan e CErfTable vêm do jogo; os
// dos campos, dos métodos e de CErfResource são nossos.
#ifndef AURORA_CERFMAN_H
#define AURORA_CERFMAN_H

#include "Aurora/CDSFileSystem.h"

u32 HashResourceName(const CExoString& name);

// Um pacote aberto (0x10 bytes).
class CErfTable {
public:
    CErfTable() { m_entries = NULL; }
    virtual ~CErfTable();
    void Open(const char* name);
    int Find(u32 hash);
    u32 GetOffset(int index);
    int GetSize(int index);
    BOOL Read(int index, u8** dst, int* size, BOOL compressed);
    BOOL Read(int index, u8** dst, u32 offset, int size, BOOL compressed);

    u8* m_entries;   // +0x4: o índice, como veio do arquivo (sem alinhamento garantido)
    int m_count;     // +0x8
    u16 m_fileId;    // +0xc
};

// Os dois pacotes.
class CErfMan : public Singleton<CErfMan> {
public:
    virtual ~CErfMan();
    virtual void Update();
    void LoadTables();
    CErfTable* Find(u32 hash, int* index);

    CErfTable m_tables[2];   // +0x4: o comum e o do idioma
};

// Um recurso pedido pelo nome (0x10 bytes). O pacote e a posição são achados na
// primeira leitura.
class CErfResource {
public:
    CErfResource(const CExoString& name, int small);
    BOOL Read(u8** dst, int* size);
    BOOL Read(u8** dst, u32 offset, int size);
    int GetSize();

    u32 m_hash;            // +0x0
    int m_index;           // +0x4: posição no índice, ou -1
    CErfTable* m_table;    // +0x8
    int m_small;           // +0xc: 1 = procurar "nome.small" (comprimido)
};

#endif
