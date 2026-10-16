// G2DA: as tabelas GDA, e as funções de leitura de GFF4 que o jogo inteiro usa.
// 0x0201ae10-0x0201b918.
#include "Aurora/Comum.h"
#include "Aurora/G2DA.h"
#include "Aurora/CErfMan.h"

// Os rótulos dos campos de uma GDA.
#define GDA_COLUMN_HASH 10001   // numa coluna: o CRC32 do nome
#define GDA_COLUMNS 10002
#define GDA_ROWS 10003
#define GDA_CELL 10005          // numa linha: 10005 + o número da coluna
#define GDA_COLUMN_TYPE 10999   // numa coluna, nas tabelas "V0.2"

// 0x0201ae10
G2DA::G2DA(const CExoString& name)
{
    Init();
    CErfResource resource(name, 0);
    int size = -1;
    m_data = NULL;
    if (resource.Read(&m_data, &size))
        Load(m_data, size);
    m_size = size;
}

// 0x0201ae68
G2DA::G2DA()
{
    Init();
}

// 0x0201ae88 e 0x0201aea4
G2DA::~G2DA()
{
    Clear();
}

// 0x0201aec8: a coluna com este hash, ou -1.
int G2DA::GetColumnIndex(u32 hash)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List columns = root.GetList(GDA_COLUMNS);
    for (u32 i = 0; i < columns.GetCount(); i++) {
        GFF4Struct column = columns.GetStruct(i);
        if (hash == column.GetU32(GDA_COLUMN_HASH))
            return i;
    }
    return -1;
}

// 0x0201af80: a linha com este id (o valor da primeira coluna), ou -1.
int G2DA::GetRowIndex(int id)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    for (u32 i = 0; i < rows.GetCount(); i++) {
        GFF4Struct row = rows.GetStruct(i);
        if (id == row.GetInt(GDA_CELL))
            return i;
    }
    return -1;
}

// 0x0201b004
BOOL G2DA::Load(u8* data, u32 size)
{
    if (m_loaded)
        return TRUE;
    m_gff = new GFF4File(data, size);
    m_data = data;
    if (m_gff == NULL || !m_gff->IsValid())
        return FALSE;
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4List columns = root.GetList(GDA_COLUMNS);
    m_rowCount = rows.GetCount();
    m_columnCount = columns.GetCount();
    m_loaded = TRUE;
    return TRUE;
}

// 0x0201b0dc
BOOL G2DA::Unload()
{
    Clear();
    return TRUE;
}

// 0x0201b0e8
BOOL G2DA::GetStringByKey(int id, u32 hash, CExoString& out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetString(row, column, out);
    return FALSE;
}

// 0x0201b118
BOOL G2DA::GetFloatByKey(int id, u32 hash, float* out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetFloat(row, column, out);
    return FALSE;
}

// 0x0201b148
BOOL G2DA::GetIntByKey(int id, u32 hash, int* out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetInt(row, column, out);
    return FALSE;
}

// 0x0201b178
BOOL G2DA::GetBoolByKey(int id, u32 hash, BOOL* out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetBool(row, column, out);
    return FALSE;
}

// 0x0201b1a8
BOOL G2DA::GetFixedByKey(int id, u32 hash, CFixed* out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetFixed(row, column, out);
    return FALSE;
}

// 0x0201b1d8
BOOL G2DA::GetString(int row, int column, CExoString& out)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4Struct cells = rows.GetStruct(row);
    const GFF4FieldDef* field = cells.GetField(GDA_CELL + column);
    if (field != NULL) {
        if (field->type == 0x20000014)   // texto de 8 bits, por referência
            out = cells.GetString(GDA_CELL + column);
        else
            out = CExoString(cells.GetStringW(GDA_CELL + column));
    }
    return TRUE;
}

// 0x0201b324
BOOL G2DA::GetFloat(int row, int column, float* out)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4Struct cells = rows.GetStruct(row);
    *out = cells.GetFloat(GDA_CELL + column);
    return TRUE;
}

// 0x0201b3bc
BOOL G2DA::GetInt(int row, int column, int* out)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4Struct cells = rows.GetStruct(row);
    *out = cells.GetInt(GDA_CELL + column);
    return TRUE;
}

// 0x0201b420
BOOL G2DA::GetBool(int row, int column, BOOL* out)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4Struct cells = rows.GetStruct(row);
    *out = cells.GetU8(GDA_CELL + column) != 0;
    return TRUE;
}

// 0x0201b4c4: o ponto fixo (tipo 18) sai direto; um float é convertido.
BOOL G2DA::GetFixed(int row, int column, CFixed* out)
{
    GFF4Struct root = m_gff->GetRoot();
    GFF4List rows = root.GetList(GDA_ROWS);
    GFF4Struct cells = rows.GetStruct(row);
    const GFF4FieldDef* field = cells.GetField(GDA_CELL + column);
    if (field != NULL) {
        if (field->type == 18)
            *out = cells.GetFixed(GDA_CELL + column);
        else
            *out = CFixed(cells.GetFloat(GDA_CELL + column));
    }
    return TRUE;
}

// 0x0201b5ac
void G2DA::Init()
{
    m_loaded = FALSE;
    m_data = NULL;
    m_size = 0;
    m_rowCount = 0;
    m_columnCount = 0;
    m_gff = NULL;
}

// 0x0201b5bc
void G2DA::Clear()
{
    if (m_data != NULL) {
        delete[] m_data;
        m_data = NULL;
    }
    if (m_gff != NULL) {
        delete m_gff;
        m_gff = NULL;
    }
    m_loaded = FALSE;
    m_rowCount = 0;
    m_columnCount = 0;
}

// 0x0201b5e8
BOOL G2DA::Find(int id, u32 hash, int* row, int* column)
{
    *row = GetRowIndex(id);
    *column = GetColumnIndex(hash);
    return *row != -1 && *column != -1;
}

// 0x0201b620
BOOL G2DA::GetColumnTypeByKey(int id, u32 hash, int* out)
{
    int row, column;
    if (Find(id, hash, &row, &column))
        return GetColumnType(row, column, out);
    return FALSE;
}

// 0x0201b650: 0 texto, 1 inteiro, 2 número com vírgula, 3 booleano.
// Aqui o modelo é pedido direto (GetTemplate().FindField), e não pelos acessores inline:
// com eles, o compilador põe o tipo e o "found" em outros registradores e na pilha os
// temporários em outra ordem. O "found" (1 antes do switch, 0 no default) é o padrão do
// parse_format do MSL: com ele, o 1 fica pronto em r0 antes do switch (DIARIO 29).
BOOL G2DA::GetColumnType(int row, int column, int* out)
{
    GFF4Struct root = m_gff->GetRoot();
    if (m_gff->GetVersion() == 0x322e3056) {   // "V0.2"
        GFF4List columns = root.GetList(root.GetTemplate().FindField(GDA_COLUMNS));
        GFF4Struct info = columns.GetStruct(column);
        *out = info.Get<u8>(info.GetTemplate().FindField(GDA_COLUMN_TYPE), 0);
        return TRUE;
    }
    GFF4List rows = root.GetList(root.GetTemplate().FindField(GDA_ROWS));
    GFF4Struct cells = rows.GetStruct(row);
    u32 type = cells.GetTemplate().FindField(GDA_CELL + column)->type;
    BOOL found = TRUE;
    switch (type) {
    case 14:
    case 20:
        *out = 0;
        break;
    case 5:
        *out = 1;
        break;
    case 8:
    case 18:
        *out = 2;
        break;
    case 0:
        *out = 3;
        break;
    default:
        found = FALSE;
    }
    return found;
}

// 0x0201b754
int G2DA::GetColumnCount()
{
    return m_columnCount;
}

// --- GFF4 -------------------------------------------------------------------------------
// Toda struct ou lista nova recebe o arquivo montado de novo, GFF4File(dados, tamanho), e
// não uma cópia do campo: o jogo lê o tamanho antes dos dados (os argumentos são
// calculados da direita para a esquerda), o que uma cópia de struct não faz.

// 0x0201b758
GFF4Struct GFF4Element::GetStruct() const
{
    return GFF4StructRef(GFF4File(m_file.m_data, m_file.m_size), m_offset, m_type).GetStruct();
}

// 0x0201b77c
GFF4List GFF4Struct::GetList(const GFF4FieldDef* field) const
{
    if (field != NULL) {
        u32 offset = *(u32*)(m_ptr + field->offset);
        if (offset != 0xffffffff)
            return GFF4List(m_file.GetDataStart() + offset, GFF4File(m_file.m_data, m_file.m_size), field->type);
    }
    return GFF4List();
}

// 0x0201b7bc
GFF4Array GFF4Struct::GetArray(const GFF4FieldDef* field) const
{
    if (field != NULL) {
        u32 offset = *(u32*)(m_ptr + field->offset);
        if (offset != 0xffffffff)
            return GFF4Array(m_file.GetDataStart() + offset, GFF4File(m_file.m_data, m_file.m_size));
    }
    return GFF4Array();
}

// 0x0201b7f0
const GFF4FieldDef* GFF4Template::FindField(u32 label) const
{
    int low = 0;
    int high = m_def->fieldCount - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        const GFF4FieldDef* field = &m_fields[middle];
        if (label == field->label)
            return field;
        if (label < field->label)
            high = middle - 1;
        else
            low = middle + 1;
    }
    return NULL;
}

// 0x0201b82c
GFF4Struct GFF4StructRef::GetStruct() const
{
    GFF4File file(m_file.m_data, m_file.m_size);
    return GFF4Struct(file, m_template, file.GetDataStart() + m_offset);
}

// 0x0201b848
GFF4Struct GFF4List::GetStruct(u32 index) const
{
    return GFF4Struct(GFF4File(m_file.m_data, m_file.m_size), m_template, m_ptr + 4 + index * GetTemplate().GetSize());
}

// 0x0201b878
GFF4Template GFF4List::GetTemplate() const
{
    return GFF4File(m_file.m_data, m_file.m_size).GetTemplate(m_template);
}

// 0x0201b894
GFF4Template GFF4Struct::GetTemplate() const
{
    return GetFile().GetTemplate(m_template);
}

// 0x0201b8b0
GFF4Struct GFF4Struct::GetStruct(const GFF4FieldDef* field) const
{
    if (field != NULL)
        return GFF4Struct(GFF4File(m_file.m_data, m_file.m_size), (int)field->type, m_ptr + field->offset);
    return GFF4Struct();
}

// 0x0201b8e0
GFF4File GFF4Struct::GetFile() const
{
    return GFF4File(m_file.m_data, m_file.m_size);
}

// 0x0201b8ec
GFF4Struct GFF4File::GetRoot() const
{
    return GFF4Struct(GFF4File(m_data, m_size), 0u, GetDataStart());
}

// 0x0201b900
GFF4Template GFF4File::GetTemplate(u32 index) const
{
    const GFF4TemplateDef* def = (GFF4TemplateDef*)(m_data + 0x1c) + index;
    return GFF4Template(def, (GFF4FieldDef*)(m_data + def->fieldsOffset));
}
