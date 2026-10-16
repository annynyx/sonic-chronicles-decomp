// C2DA: uma tabela 2DA em texto ("2DA V2.0"; docs/FORMATOS.md), o formato clássico da
// BioWare. Os nomes C2DA e I2DAProducer vêm do jogo (do RTTI); os dos métodos seguem os da
// C2DA do Neverwinter Nights, do mesmo motor, e os dos campos são nossos.
//
// A tabela tem dois jeitos de guardar os textos. Lida do arquivo, numa C2DA temporária, cada
// célula é uma CExoString. Depois, CopyFrom passa tudo para o jeito "compacto": um bloco só
// com os ponteiros dos nomes de linha e de coluna, a posição de cada célula (u16) e os textos.
#ifndef AURORA_C2DA_H
#define AURORA_C2DA_H

#include "Aurora/CExoString.h"

// Quem fornece uma tabela: o jogo só tem a C2DA.
class I2DAProducer {
public:
    virtual ~I2DAProducer() {}
    virtual int GetNumColumns() = 0;
    virtual int GetNumRows() = 0;
    virtual BOOL GetCExoStringEntry(int row, int column, CExoString& out) = 0;
    virtual void GetColumnLabel(int column, CExoString& out) = 0;
    virtual void GetRowLabel(int row, CExoString& out) = 0;
};

class C2DA : public I2DAProducer {
public:
    C2DA();
    virtual ~C2DA();
    // as duas saem no fim do .o na ordem inversa desta; na vtable vale a ordem da base
    virtual int GetNumRows() { return m_nNumRows; }
    virtual int GetNumColumns() { return m_nNumColumns; }
    virtual BOOL GetCExoStringEntry(int row, int column, CExoString& out);
    virtual void GetColumnLabel(int column, CExoString& out);
    virtual void GetRowLabel(int row, CExoString& out);

    void InitClass();
    void Unload2DArray();
    void CopyFrom(I2DAProducer* source);
    BOOL AllocateArrays();
    BOOL GetEntry(int row, int column, CExoString& out);
    const char* GetEntryText(int row, int column);
    BOOL GetINTEntry(int row, int column, int* out);
    u32 GetNextLineLength(char* text, u32 length);
    BOOL GetNextToken(char** text, u32* length, char* token, u32* skipped);
    BOOL Load(const char* name);
    BOOL LoadData(u8* data, int size);
    BOOL Parse(char* text, u32 length, BOOL binary);
    BOOL SkipNewLines(char** text, u32* length);

    CExoString m_sDefaultValue;    // +0x04: o valor de uma célula fora da tabela
    int m_nNumRows;                // +0x10
    int m_nNumColumns;             // +0x14
    BOOL m_bLoaded;                // +0x18
    int m_unknown1c;               // +0x1c
    CExoString* m_pRowLabel;       // +0x20: (lida do arquivo) os nomes das linhas
    CExoString* m_pColumnLabel;    // +0x24: os nomes das colunas
    CExoString* m_pArrayData;      // +0x28: as células, linha por linha
    BOOL m_bCompact;               // +0x2c
    BOOL m_bOwnsBuffer;            // +0x30: o bloco compacto foi alocado aqui
    char** m_pRowLabels;           // +0x34: (compacta) os nomes das linhas
    char** m_pColumnLabels;        // +0x38: os nomes das colunas
    u16* m_pOffsets;               // +0x3c: a posição do texto de cada célula
    char* m_pStrings;              // +0x40: os textos das células
    char* m_pBuffer;               // +0x44: o bloco
    CExoString m_unknown48;        // +0x48
    CExoString m_unknown54;        // +0x54
    int m_unknown60;               // +0x60
    int m_unknown64;               // +0x64: só o tamanho (0x68) aparece no jogo
};

#endif
