// G2DA: uma tabela GDA (o 2DA binário, um GFF4 do tipo "G2DA"; docs/FORMATOS.md). Os
// nomes G2DA e C2DASource vêm do jogo (do RTTI); os dos métodos e campos são nossos.
//
// A tabela é uma struct raiz com duas listas: as colunas (campo 10002), cada uma com o
// hash do nome (10001), e as linhas (10003), cada uma com um campo por coluna (10005 +
// número da coluna). O valor de uma célula sai pela posição (linha, coluna) ou pela chave:
// o id da linha (o valor da coluna 0) e o hash do nome da coluna.
#ifndef AURORA_G2DA_H
#define AURORA_G2DA_H

#include "Aurora/CExoString.h"
#include "Aurora/CFixed.h"
#include "Aurora/GFF4.h"

// A interface de uma tabela de dados (o jogo só tem a G2DA).
class C2DASource {
public:
    virtual ~C2DASource() {}
    virtual BOOL IsLoaded() = 0;
    virtual BOOL Load(u8* data, u32 size) = 0;
    virtual BOOL Unload() = 0;
    virtual int GetColumnCount() = 0;
    virtual int GetRowCount() = 0;
    virtual int GetColumnIndex(u32 hash) = 0;
    virtual int GetRowIndex(int id) = 0;
    virtual BOOL GetStringByKey(int id, u32 hash, CExoString& out) = 0;
    virtual BOOL GetFloatByKey(int id, u32 hash, float* out) = 0;
    virtual BOOL GetIntByKey(int id, u32 hash, int* out) = 0;
    virtual BOOL GetBoolByKey(int id, u32 hash, BOOL* out) = 0;
    virtual BOOL GetString(int row, int column, CExoString& out) = 0;
    virtual BOOL GetFloat(int row, int column, float* out) = 0;
    virtual BOOL GetInt(int row, int column, int* out) = 0;
    virtual BOOL GetBool(int row, int column, BOOL* out) = 0;
    virtual BOOL GetColumnTypeByKey(int id, u32 hash, int* out) = 0;
    virtual BOOL GetColumnType(int row, int column, int* out) = 0;
    virtual BOOL GetFixedByKey(int id, u32 hash, CFixed* out) = 0;
    virtual BOOL GetFixed(int row, int column, CFixed* out) = 0;
};

class G2DA : public C2DASource {
public:
    G2DA(const CExoString& name);
    G2DA();
    virtual ~G2DA();
    // IsLoaded e GetRowCount são inline: o jogo ficou com as cópias de outros arquivos
    // (0x0200f800 e 0x0200c458), que só a vtable da G2DA usa.
    virtual BOOL IsLoaded() { return m_loaded; }
    virtual BOOL Load(u8* data, u32 size);
    virtual BOOL Unload();
    virtual int GetColumnCount();
    virtual int GetRowCount() { return m_rowCount; }
    virtual int GetColumnIndex(u32 hash);
    virtual int GetRowIndex(int id);
    virtual BOOL GetStringByKey(int id, u32 hash, CExoString& out);
    virtual BOOL GetFloatByKey(int id, u32 hash, float* out);
    virtual BOOL GetIntByKey(int id, u32 hash, int* out);
    virtual BOOL GetBoolByKey(int id, u32 hash, BOOL* out);
    virtual BOOL GetString(int row, int column, CExoString& out);
    virtual BOOL GetFloat(int row, int column, float* out);
    virtual BOOL GetInt(int row, int column, int* out);
    virtual BOOL GetBool(int row, int column, BOOL* out);
    virtual BOOL GetColumnTypeByKey(int id, u32 hash, int* out);
    virtual BOOL GetColumnType(int row, int column, int* out);
    virtual BOOL GetFixedByKey(int id, u32 hash, CFixed* out);
    virtual BOOL GetFixed(int row, int column, CFixed* out);

    void Init();
    void Clear();
    BOOL Find(int id, u32 hash, int* row, int* column);

    BOOL m_loaded;         // +0x04
    int m_size;            // +0x08
    u8* m_data;            // +0x0c
    CExoString m_name;     // +0x10
    GFF4File* m_gff;       // +0x1c
    int m_columnCount;     // +0x20
    int m_rowCount;        // +0x24
};

#endif
