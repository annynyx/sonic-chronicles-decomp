// GFF4: o contêiner de dados da BioWare (docs/FORMATOS.md), lido direto da memória.
//
// As classes daqui não copiam nada do arquivo: cada uma é um jeito de olhar para um pedaço
// dele (o arquivo inteiro, um modelo de struct, uma struct, uma lista) e cabe em 16 bytes
// ou menos, então o jogo as passa e devolve por valor. Os nomes são nossos.
//
// As funções não são inline: o jogo as chama de dezenas de arquivos, e cada uma tem uma
// cópia só, no fim do arquivo da G2DA (src/Aurora/G2DA.cpp). Já os leitores de valor
// (Get<T>) são templates: cada arquivo gera as cópias dos tipos que usa, logo depois da
// primeira função que as pede, e o linker fica com uma.
#ifndef AURORA_GFF4_H
#define AURORA_GFF4_H

#include "Aurora/Comum.h"
#include "Aurora/CFixed.h"

#ifndef NULL
#define NULL 0
#endif

// Um campo de um modelo de struct (12 bytes).
struct GFF4FieldDef {
    u32 label;
    u32 type;     // o tipo nos 16 bits de baixo (numa struct ou lista de structs, o modelo
                  // dela); as flags em cima (0x8000 lista, 0x4000 struct, 0x2000 referência)
    u32 offset;   // onde o valor fica dentro da struct
};

// Um modelo de struct (16 bytes); a tabela deles começa em 0x1c no cabeçalho.
struct GFF4TemplateDef {
    u32 label;
    u32 fieldCount;
    u32 fieldsOffset;   // os campos, ordenados pelo rótulo
    u32 size;           // o tamanho de uma struct deste modelo
};

class GFF4Struct;

class GFF4Template {
public:
    GFF4Template(const GFF4TemplateDef* def, const GFF4FieldDef* fields) : m_def(def), m_fields(fields) {}
    // Busca binária pelo rótulo. NULL se a struct não tem o campo.
    const GFF4FieldDef* FindField(u32 label) const;
    u32 GetSize() const { return m_def->size; }

    const GFF4TemplateDef* m_def;
    const GFF4FieldDef* m_fields;
};

class GFF4File {
public:
    GFF4File() : m_data(NULL), m_size(0) {}
    GFF4File(u8* data, u32 size) : m_data(data), m_size(size) {}

    BOOL HasHeader() const { return m_data != NULL && m_size > 0x1c; }
    BOOL IsGFF() const { return HasHeader() && *(u32*)m_data == 0x20464647; }      // "GFF "
    BOOL IsValid() const { return IsGFF() && ((u32*)m_data)[1] == 0x302e3456; }   // "V4.0"
    u32 GetVersion() const { return ((u32*)m_data)[1]; }
    // Onde começam os dados (o cabeçalho diz, em 0x18).
    u8* GetDataStart() const { return m_data + *(u32*)(m_data + 0x18); }

    GFF4Struct GetRoot() const;
    GFF4Template GetTemplate(u32 index) const;

    u8* m_data;
    u32 m_size;
};

// Uma lista de structs: o u32 com o tamanho, e as structs, todas do mesmo modelo.
class GFF4List {
public:
    GFF4List() : m_ptr(NULL), m_template(0xffffffff) {}
    GFF4List(u8* ptr, const GFF4File& file, u32 index) : m_ptr(ptr), m_file(file), m_template((u16)index) {}

    u32 GetCount() const { return m_ptr != NULL ? *(u32*)m_ptr : 0; }
    GFF4Struct GetStruct(u32 index) const;
    GFF4Template GetTemplate() const;

    u8* m_ptr;
    GFF4File m_file;
    u32 m_template;
};

// Uma lista de valores de qualquer tipo: o u32 com o tamanho e, para cada um,
// {u32 tipo e flags, u32 posição}.
class GFF4Array {
public:
    GFF4Array() : m_ptr(NULL) {}
    GFF4Array(u8* ptr, const GFF4File& file) : m_ptr(ptr), m_file(file) {}

    u8* m_ptr;
    GFF4File m_file;
};

// Um elemento de uma GFF4Array.
class GFF4Element {
public:
    GFF4Struct GetStruct() const;

    GFF4File m_file;
    u32 m_offset;
    u32 m_type;
};

// Uma struct dada pela posição a partir do começo dos dados.
class GFF4StructRef {
public:
    GFF4StructRef(const GFF4File& file, u32 offset, u16 index) : m_file(file), m_offset(offset), m_template(index) {}
    GFF4Struct GetStruct() const;

    GFF4File m_file;
    u32 m_offset;
    u32 m_template;
};

// Uma struct: o modelo dela e onde ela está.
class GFF4Struct {
public:
    GFF4Struct() : m_ptr(NULL) {}
    GFF4Struct(const GFF4File& file, u32 index, u8* ptr) : m_file(file), m_template(index), m_ptr(ptr) {}
    // A partir do tipo de um campo (com as flags): o modelo são os 16 bits de baixo.
    GFF4Struct(const GFF4File& file, int type, u8* ptr) : m_file(file), m_template((u16)type), m_ptr(ptr) {}

    GFF4File GetFile() const;
    GFF4Template GetTemplate() const;
    GFF4List GetList(const GFF4FieldDef* field) const;
    GFF4Array GetArray(const GFF4FieldDef* field) const;
    GFF4Struct GetStruct(const GFF4FieldDef* field) const;
    const GFF4FieldDef* GetField(u32 label) const { return GetTemplate().FindField(label); }

    // O valor de um campo, ou "def" se a struct não tem o campo.
    template <class T> T Get(const GFF4FieldDef* field, T def) const;

    // Os valores pelo rótulo do campo, com 0 (ou NULL) se a struct não tem o campo.
    const char* GetString(u32 label) const { return Get<const char*>(GetField(label), NULL); }
    const u16* GetStringW(u32 label) const { return Get<const u16*>(GetField(label), NULL); }
    u32 GetU32(u32 label) const { return Get<u32>(GetField(label), 0); }
    int GetInt(u32 label) const { return Get<int>(GetField(label), 0); }
    u8 GetU8(u32 label) const { return Get<u8>(GetField(label), 0); }
    float GetFloat(u32 label) const { return Get<float>(GetField(label), 0); }
    CFixed GetFixed(u32 label) const { return Get<CFixed>(GetField(label), CFixed(0)); }
    GFF4List GetList(u32 label) const { return GetList(GetField(label)); }

    GFF4File m_file;
    u32 m_template;
    u8* m_ptr;
};

// Lê um valor de tipo T em base + offset. Um texto é diferente: o campo guarda a posição
// dele (ou 0xffffffff), e lá estão o u32 com o tamanho e as letras; sem texto, sai um texto
// vazio. O vazio está numa função inline, então o compilador lhe dá um nome e o jogo fica
// com uma cópia só (a de 16 bits é de outro arquivo, 0x020f585c).
template <class T> struct GFF4Reader {
    static T Read(const u8* base, u32 offset) { return *(T*)(base + offset); }
};

template <> struct GFF4Reader<const char*> {
    static const char* Read(const u8* base, u32 offset)
    {
        u32 position = *(u32*)(base + offset);
        if (position != 0xffffffff)
            return (const char*)(base + position + 4);
        return "";
    }
};

template <> struct GFF4Reader<const u16*> {
    static const u16* Read(const u8* base, u32 offset)
    {
        u32 position = *(u32*)(base + offset);
        if (position != 0xffffffff)
            return (const u16*)(base + position + 4);
        return (const u16*)L"";
    }
};

template <class T> T GFF4Read(const u8* base, u32 offset)
{
    return GFF4Reader<T>::Read(base, offset);
}

template <class T> T GFF4Struct::Get(const GFF4FieldDef* field, T def) const
{
    if (field != NULL) {
        u8* base = GetFile().GetDataStart();
        u32 start = m_ptr - base;
        return GFF4Read<T>(base, start + field->offset);
    }
    return def;
}

#endif
