// CTlkTable: os textos do jogo, lidos dos arquivos .tlk (strings.tlk e o do idioma). Os
// nomes CTlkTable e Singleton<CTlkTable> vêm do jogo; os demais são nossos.
//
// Um .tlk é um GFF4 (docs/FORMATOS.md): o byte 0x18 do cabeçalho diz onde começam os
// dados, e ali há uma tabela hash de entradas {id, posição do texto} com sondagem linear.
// O texto é um u32 com o tamanho e os caracteres (de 8 bits). A tabela não é lida inteira:
// cada busca lê 64 entradas por vez do cartucho.
#ifndef AURORA_CTLKTABLE_H
#define AURORA_CTLKTABLE_H

#include "Aurora/CDSFileSystem.h"

// Um arquivo .tlk (0x20 bytes).
class CTlkFile {
public:
    CTlkFile(CExoString name);
    // Inline, copiada dentro de CTlkTable::GetString (em assembly: DIARIO 28).
    BOOL GetString(u32 id, char* out, u32 outSize);

    CExoString m_name;   // +0x00
    u32 m_dataOffset;    // +0x0c: onde começam os dados do GFF4 (0 = cabeçalho ainda não lido)
    u32 m_count;         // +0x10: tamanho da tabela hash
    u32 m_bufferSize;    // +0x14
    u8* m_buffer;        // +0x18: o pedaço da tabela que está na memória
    u16 m_fileId;        // +0x1c
};

// O elemento da lista de arquivos: uma struct com o ponteiro, e não o ponteiro sozinho.
// O jogo mostra isso no CExoArrayList<T>::Allocate deste arquivo: das 39 cópias para
// elementos de 4 bytes, só esta copia como struct (o destino antes da origem).
struct CTlkTableEntry {
    CTlkFile* file;
};

class CTlkTable : public Singleton<CTlkTable> {
public:
    CTlkTable();
    virtual ~CTlkTable();
    virtual void Unknown2();
    virtual void Reset();
    // 0x020989fc: procura o texto em cada .tlk, na ordem em que foram acrescentados.
    // Ainda em assembly (DIARIO 28).
    BOOL GetString(u32 id, CExoString& out);
    BOOL AddFile(const CExoString& name);

    u8 m_unknown4;                       // +0x04
    u32 m_unknown8;                      // +0x08
    u32 m_textSize;                      // +0x0c
    char* m_text;                        // +0x10: o último texto achado
    CExoArrayList<CTlkTableEntry> m_files;   // +0x14
};

#endif
