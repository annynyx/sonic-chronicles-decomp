// C2DA: as tabelas 2DA em texto. 0x02003fec-0x02004c1c.
#include "Aurora/Comum.h"
#include "Aurora/C2DA.h"
#include "Aurora/CErfMan.h"
#include "msl.h"

// 0x02003fec
C2DA::C2DA()
{
    InitClass();
}

// 0x02004018 e 0x02004044
C2DA::~C2DA()
{
    Unload2DArray();
}

// 0x02004074
void C2DA::InitClass()
{
    m_nNumRows = 0;
    m_nNumColumns = 0;
    m_sDefaultValue = " ";
    m_bLoaded = FALSE;
    m_bCompact = FALSE;
    m_unknown1c = 0;
    m_pArrayData = NULL;
    m_pRowLabel = NULL;
    m_pColumnLabel = NULL;
    m_unknown48 = "";
    m_unknown54 = "";
    m_unknown60 = -1;
    m_pRowLabels = NULL;
    m_pColumnLabels = NULL;
    m_pOffsets = NULL;
    m_pStrings = NULL;
    m_pBuffer = NULL;
}

// 0x020040c4
void C2DA::Unload2DArray()
{
    if (m_bLoaded) {
        if (!m_bCompact) {
            delete[] m_pArrayData;
            m_pArrayData = NULL;
            m_pRowLabel = NULL;
            m_pColumnLabel = NULL;
        } else if (m_bOwnsBuffer) {
            delete[] m_pBuffer;
            m_pBuffer = NULL;
            m_pRowLabels = NULL;
            m_pColumnLabels = NULL;
            m_pOffsets = NULL;
            m_pStrings = NULL;
        } else {
            delete[] m_pRowLabels;
            delete[] m_pColumnLabels;
            m_pRowLabels = NULL;
            m_pColumnLabels = NULL;
            m_pOffsets = NULL;
            m_pStrings = NULL;
        }
    }
    m_bLoaded = FALSE;
    m_sDefaultValue = "";
    m_nNumRows = 0;
    m_nNumColumns = 0;
    m_bCompact = FALSE;
    m_bOwnsBuffer = FALSE;
    m_unknown48 = "";
    m_unknown54 = "";
    m_unknown60 = -1;
}

// 0x02004154: copia uma tabela para o jeito compacto, num bloco só: os ponteiros dos nomes
// das linhas e das colunas, os textos desses nomes, a posição (u16) do texto de cada célula
// e os textos das células. A primeira passada só mede. Os contadores são os mesmos em todos
// os laços e a expressão labelText + position não tem variável: só assim as posições na
// pilha saem como no jogo (docs/COMPILADOR.md). O sizeof(char*) é 4 no DS e 8 no teste de
// 64 bits do PC.
void C2DA::CopyFrom(I2DAProducer* source)
{
    Unload2DArray();
    int rows = source->GetNumRows();
    int columns = source->GetNumColumns();
    CExoString string;
    int labels = 0;
    int text = 0;
    int i;
    int j;
    for (i = 0; i < rows; i++) {
        source->GetRowLabel(i, string);
        labels += string.GetLength() + 1;
    }
    for (i = 0; i < columns; i++) {
        source->GetColumnLabel(i, string);
        labels += string.GetLength() + 1;
    }
    for (i = 0; i < rows; i++) {
        for (j = 0; j < columns; j++) {
            source->GetCExoStringEntry(i, j, string);
            text += string.GetLength() + 1;
        }
    }
    m_pBuffer = new char[text + (labels + rows * sizeof(char*) + columns * sizeof(char*) +
                                 rows * columns * sizeof(u16))];
    if (m_pBuffer == NULL)
        return;
    m_bCompact = TRUE;
    m_bOwnsBuffer = TRUE;
    m_nNumRows = rows;
    m_nNumColumns = columns;
    int position = 0;
    m_pRowLabels = (char**)m_pBuffer;
    m_pColumnLabels = m_pRowLabels + m_nNumRows;
    char* labelText = (char*)(m_pColumnLabels + m_nNumColumns);
    m_pOffsets = (u16*)(labelText + labels);
    m_pStrings = (char*)(m_pOffsets + m_nNumRows * m_nNumColumns);
    for (i = 0; i < rows; i++) {
        source->GetRowLabel(i, string);
        int length = string.GetLength();
        memcpy(labelText + position, string.CStr(), length);
        labelText[position + length] = 0;
        m_pRowLabels[i] = labelText + position;
        position += length + 1;
    }
    for (i = 0; i < columns; i++) {
        source->GetColumnLabel(i, string);
        int length = string.GetLength();
        memcpy(labelText + position, string.CStr(), length);
        labelText[position + length] = 0;
        m_pColumnLabels[i] = labelText + position;
        position += length + 1;
    }
    u16 offset = 0;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < columns; j++) {
            source->GetCExoStringEntry(i, j, string);
            int length = string.GetLength();
            memcpy(m_pStrings + offset, string.CStr(), length);
            m_pStrings[offset + length] = 0;
            m_pOffsets[i * columns + j] = offset;
            offset += (u16)length + 1;
        }
    }
    m_bLoaded = TRUE;
}

// 0x020043b0: as CExoString da tabela lida do arquivo: as células, depois os nomes das
// linhas e os das colunas. A conta escrita assim é a que bate; o compilador a fatora.
BOOL C2DA::AllocateArrays()
{
    if (m_nNumRows <= 0 || m_nNumColumns <= 0)
        return TRUE;
    m_pArrayData = new CExoString[m_nNumRows * m_nNumColumns + m_nNumColumns + m_nNumRows];
    if (m_pArrayData == NULL)
        return FALSE;
    m_pRowLabel = m_pArrayData + m_nNumRows * m_nNumColumns;
    m_pColumnLabel = m_pRowLabel + m_nNumRows;
    return TRUE;
}

// 0x02004410
void C2DA::GetColumnLabel(int column, CExoString& out)
{
    if (m_bCompact)
        out = m_pColumnLabels[column];
    else
        out = m_pColumnLabel[column];
}

// 0x02004438
void C2DA::GetRowLabel(int row, CExoString& out)
{
    if (m_bCompact)
        out = m_pRowLabels[row];
    else
        out = m_pRowLabel[row];
}

// 0x02004460
BOOL C2DA::GetEntry(int row, int column, CExoString& out)
{
    if (m_bCompact)
        out = m_pStrings + m_pOffsets[column + row * m_nNumColumns];
    else
        out = m_pArrayData[column + row * m_nNumColumns];
    return out.GetLength() > 0;
}

// 0x020044b0
const char* C2DA::GetEntryText(int row, int column)
{
    if (m_bCompact)
        return m_pStrings + m_pOffsets[column + row * m_nNumColumns];
    return m_pArrayData[column + row * m_nNumColumns].CStr();
}

// 0x020044e0
BOOL C2DA::GetCExoStringEntry(int row, int column, CExoString& out)
{
    if (row < 0 || row >= m_nNumRows || column < 0 || column >= m_nNumColumns) {
        out = m_sDefaultValue;
        return FALSE;
    }
    return GetEntry(row, column, out);
}

// 0x0200450c
BOOL C2DA::GetINTEntry(int row, int column, int* out)
{
    if (row < 0 || row >= m_nNumRows || column < 0 || column >= m_nNumColumns) {
        *out = m_sDefaultValue.AsINT();
        return FALSE;
    }
    const char* text = GetEntryText(row, column);
    if (*text == 0) {
        *out = 0;
        return FALSE;
    }
    if (sscanf(text, "%i", out) != 1)
        *out = 0;
    return TRUE;
}

// 0x0200455c: quantos caracteres até o fim da linha.
u32 C2DA::GetNextLineLength(char* text, u32 length)
{
    u32 count = 0;
    while (count < length && *text != '\n' && *text != '\r') {
        text++;
        count++;
    }
    return count;
}

// 0x0200457c: copia a próxima palavra (ou o texto entre aspas) para "token". A condição
// dos espaços ignora o tamanho quando acha um tab (o && liga antes do ||).
BOOL C2DA::GetNextToken(char** text, u32* length, char* token, u32* skipped)
{
    if (*length == 0) {
        *token = 0;
        return FALSE;
    }
    u32 count = 0;
    u32 spaces = 0;
    char* p = *text;
    if (*p == 0)
        p++;
    while (spaces < *length && *p == ' ' || *p == '\t') {
        p++;
        spaces++;
    }
    *skipped = spaces;
    *length = *length - spaces;
    if (*p == '"') {
        p++;
        while (count < *length && *p != 0 && *p != '"') {
            *token++ = *p++;
            count++;
        }
        if (*p == '"') {
            p++;
            count++;
        }
    } else {
        while (count < *length && *p != 0 && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            *token++ = *p++;
            count++;
        }
    }
    *text = p;
    *length = *length - count;
    *token = 0;
    return count != 0;
}

// 0x02004640: lê a tabela do pacote. O resultado de LoadData é ignorado.
BOOL C2DA::Load(const char* name)
{
    BOOL result = TRUE;
    u8* data = NULL;
    int size = -1;
    CErfResource resource(CExoString(name), 0);
    if (resource.Read(&data, &size)) {
        LoadData(data, size);
    } else {
        delete[] data;
        result = FALSE;
    }
    return result;
}

// 0x02004690
BOOL C2DA::LoadData(u8* data, int size)
{
    if (*(u32*)data != 0x20414432) {   // "2DA "
        delete[] data;
        return FALSE;
    }
    BOOL binary;
    if (((u32*)data)[1] == 0x622e3256) {          // "V2.b"
        binary = TRUE;
    } else if (((u32*)data)[1] == 0x302e3256) {   // "V2.0"
        binary = FALSE;
    } else {
        delete[] data;
        return FALSE;
    }
    C2DA table;
    BOOL result = table.Parse((char*)data + 8, size - 8, binary);
    if (result)
        CopyFrom(&table);
    table.Unload2DArray();
    delete[] data;
    return result;
}

// 0x02004718: lê o texto de uma tabela (depois de "2DA V2.0" ou "2DA V2.b"). A primeira
// linha pode trazer o valor padrão ("DEFAULT: x"); a seguinte tem os nomes das colunas. No
// formato binário, os nomes e as células já estão prontos no arquivo: a tabela só aponta
// para eles. No texto, cada linha começa com o nome da linha, e "****" é uma célula vazia.
// As variáveis estão no topo, no estilo C, na ordem que dá as posições do jogo na pilha.
// Defeito do jogo: com "DEFAULT" sem os dois-pontos colados, o tamanho da linha é
// descontado, mas o texto não avança (nenhum arquivo da ROM usa DEFAULT).
BOOL C2DA::Parse(char* text, u32 length, BOOL binary)
{
    int columns = 0;
    int column;
    int i;
    CExoString defaultValue("");
    char* cursor;
    char* line;
    u32 lineLength;
    u32 skipped;
    u32 remaining;
    char token[256];
    SkipNewLines(&text, &length);
    line = text;
    lineLength = GetNextLineLength(text, length);
    remaining = lineLength;
    GetNextToken(&line, &lineLength, token, &skipped);
    strupr(token);
    if (strcmp(token, "DEFAULT:") == 0) {
        GetNextToken(&line, &lineLength, token, &skipped);
        defaultValue = token;
        text = line;
        length = length - remaining;
    } else if (strcmp(token, "DEFAULT") == 0) {
        GetNextToken(&line, &lineLength, token, &skipped);
        if (token[0] == ':') {
            if (strlen(token + 1) < 1) {
                GetNextToken(&line, &lineLength, token, &skipped);
                defaultValue = token;
            } else {
                defaultValue = token + 1;
            }
        } else {
            defaultValue = token;
        }
        length = length - remaining;
    }
    SkipNewLines(&text, &length);
    line = text;
    lineLength = GetNextLineLength(text, length);
    while (GetNextToken(&line, &lineLength, token, &skipped))
        columns++;
    if (columns <= 0)
        return FALSE;
    m_nNumColumns = columns;
    m_sDefaultValue = defaultValue;
    if (binary) {
        m_bCompact = TRUE;
        m_bOwnsBuffer = FALSE;
        m_pColumnLabels = new char*[m_nNumColumns];
        u16 position = 0;
        for (i = 0; i < m_nNumColumns; i++) {
            m_pColumnLabels[i] = text + position;
            while (text[position] != '\t' && text[position] != 0)
                position++;
            text[position] = 0;
            position++;
        }
        position++;
        m_nNumRows = *(int*)(text + position);
        position += 4;
        m_pRowLabels = new char*[m_nNumRows];
        for (i = 0; i < m_nNumRows; i++) {
            m_pRowLabels[i] = text + position;
            while (text[position] != '\t' && text[position] != 0)
                position++;
            text[position] = 0;
            position++;
        }
        text += position;
        m_pOffsets = (u16*)text;
        text += sizeof(u16) * m_nNumRows * m_nNumColumns;
        text += 2;
        m_pStrings = text;
    } else {
        m_bCompact = FALSE;
        CExoString* columnLabels = new CExoString[m_nNumColumns];
        if (columnLabels == NULL) {
            m_nNumColumns = 0;
            m_sDefaultValue = "";
            return FALSE;
        }
        column = 0;
        remaining = 0;
        while (GetNextToken(&text, &length, token, &skipped)) {
            columnLabels[column] = token;
            strlwr((char*)columnLabels[column].CStr());
            column++;
        }
        SkipNewLines(&text, &length);
        m_nNumRows = 0;
        cursor = text;
        remaining = length;
        while (length != 0) {
            lineLength = GetNextLineLength(text, length);
            text += lineLength;
            length -= lineLength;
            SkipNewLines(&text, &length);
            m_nNumRows++;
        }
        text = cursor;
        length = remaining;
        if (m_nNumRows == 0) {
            delete[] columnLabels;
            m_nNumColumns = 0;
            m_sDefaultValue = "";
            return FALSE;
        }
        if (!AllocateArrays()) {
            delete[] columnLabels;
            m_nNumRows = 0;
            m_nNumColumns = 0;
            m_sDefaultValue = "";
            return FALSE;
        }
        for (i = 0; i < m_nNumColumns; i++)
            m_pColumnLabel[i] = columnLabels[i];
        delete[] columnLabels;
        for (int row = 0; length != 0; row++) {
            lineLength = GetNextLineLength(text, length);
            cursor = text;
            remaining = length;
            GetNextToken(&cursor, &remaining, token, &skipped);
            m_pRowLabel[row] = token;
            strlwr((char*)m_pRowLabel[row].CStr());
            for (column = 0; column < m_nNumColumns; column++) {
                if (GetNextToken(&cursor, &remaining, token, &skipped)) {
                    if (strcmp(token, "****") == 0)
                        m_pArrayData[column + row * m_nNumColumns] = "";
                    else
                        m_pArrayData[column + row * m_nNumColumns] = token;
                } else {
                    m_pArrayData[column + row * m_nNumColumns] = m_sDefaultValue;
                }
            }
            text += lineLength;
            length -= lineLength;
            SkipNewLines(&text, &length);
        }
    }
    m_bLoaded = TRUE;
    return TRUE;
}

// 0x02004b9c: pula quebras de linha e os espaços entre elas.
BOOL C2DA::SkipNewLines(char** text, u32* length)
{
    char* p;
    u32 skipped = 0;
    u32 count = 0;
    p = *text;
    u32 total = *length;
    while (skipped < total && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) {
        skipped = count;
        while (skipped < total && (*p == '\n' || *p == '\r')) {
            p++;
            skipped++;
        }
        count = skipped;
        while (count < total && (*p == ' ' || *p == '\t')) {
            p++;
            count++;
        }
        if (count == total) {
            skipped = count;
            break;
        }
    }
    *text += skipped;
    *length -= skipped;
    return skipped != 0;
}
