// CExoString: a string do motor Aurora. 0x02004e00-0x020055b8, 33 funções.
//
// É a parte do arquivo que dá para provar hoje. O arquivo de verdade também tem
// um __sinit (0x020edb7c) e alguns dados ("" em 0x020f5508, uma constante em
// 0x020f550c) que continuam no assembly: veja docs/DIARIO.md, seção 23.
//
// A ordem das funções é a do jogo, e é a do fonte: o compilador põe as funções
// no .o na ordem em que aparecem no arquivo. Para cada construtor ele gera dois
// (o C1, do objeto completo, e o C2, da parte "base" de uma classe derivada); o
// linker descarta os que ninguém chama. Só o de const char* tem os dois no jogo.
#include "Aurora/CExoString.h"
#include "msl.h"

#define NULL 0

// 0x02004e00, 0x02004e0c: os dois só fazem new[] e delete[].
char* CExoString::AllocBuffer(unsigned int size)
{
    return new char[size];
}

void CExoString::FreeBuffer(char* buffer)
{
    if (buffer != NULL)
        delete[] buffer;
}

// 0x02004e1c
CExoString::CExoString()
{
    m_sString = NULL;
    m_nBufferLength = 0;
}

// 0x02004e2c (C2) e 0x02004e68 (C1)
CExoString::CExoString(const char* source)
{
    m_sString = NULL;
    m_nBufferLength = 0;
    if (source != NULL) {
        int length = strlen(source);
        if (length > 0) {
            m_nBufferLength = length + 1;
            m_sString = AllocBuffer(m_nBufferLength);
            memcpy(m_sString, source, m_nBufferLength);
        }
    }
}

// 0x02004ea4: texto de 16 bits, guardando só o byte baixo de cada letra. Não
// grava m_nBufferLength (fica 0), como no jogo. O BOOL calculado uma vez, fora do
// laço, é o que dá o "r2 = 1 se source é NULL" do original.
CExoString::CExoString(const unsigned short* source)
{
    m_sString = NULL;
    m_nBufferLength = 0;
    int length = 0;
    const unsigned short* p = source;
    BOOL end = (source == NULL);
    while (!end && *p != 0) {
        p++;
        length++;
    }
    if (length > 0) {
        m_sString = new char[length + 1];
        m_sString[length] = 0;
        for (int i = 0; i < length; i++)
            m_sString[i] = *source++;
    }
}

// 0x02004efc
CExoString::CExoString(const CExoString& source)
{
    m_sString = NULL;
    m_nBufferLength = 0;
    if (source.m_sString != NULL) {
        int length = strlen(source.m_sString);
        if (length > 0) {
            m_nBufferLength = length + 1;
            m_sString = AllocBuffer(m_nBufferLength);
            memcpy(m_sString, source.m_sString, m_nBufferLength);
        }
    }
}

// 0x02004f38: o número em texto. O jogo reserva 0x24 bytes na pilha: o buffer tem
// de 33 a 36 bytes (todos dão o mesmo código).
CExoString::CExoString(int value)
{
    char buffer[36];
    int length = sprintf(buffer, "%i", value);
    m_nBufferLength = length + 1;
    m_sString = AllocBuffer(m_nBufferLength);
    memcpy(m_sString, buffer, m_nBufferLength);
}

// 0x02004f70 (D1) e 0x02004f88 (D0, o que também faz delete this)
CExoString::~CExoString()
{
    FreeBuffer(m_sString);
}

// 0x02004fa4. O strlen do texto antigo não é usado, mas o jogo chama.
CExoString& CExoString::operator=(const CExoString& string)
{
    if (this == &string)
        return *this;
    char* old = m_sString;
    int newLength = 0;
    int oldLength = 0;
    if (old != NULL)
        oldLength = strlen(old);
    if (string.m_sString != NULL)
        newLength = strlen(string.m_sString);
    if (old != NULL && (newLength == 0 || newLength + 1 > m_nBufferLength)) {
        FreeBuffer(old);
        m_nBufferLength = 0;
        m_sString = NULL;
    }
    if (newLength != 0) {
        if (m_sString == NULL) {
            m_nBufferLength = newLength + 1;
            m_sString = AllocBuffer(m_nBufferLength);
        }
        strcpy(m_sString, string.m_sString);
    } else if (m_sString != NULL) {
        FreeBuffer(m_sString);
        m_sString = NULL;
        m_nBufferLength = 0;
    }
    return *this;
}

// 0x0200501c
CExoString& CExoString::operator=(const char* string)
{
    char* old = m_sString;
    int newLength = 0;
    int oldLength = 0;
    if (old != NULL)
        oldLength = strlen(old);
    if (string != NULL)
        newLength = strlen(string);
    if (old != NULL && (string == NULL || newLength + 1 > m_nBufferLength)) {
        FreeBuffer(old);
        m_nBufferLength = 0;
        m_sString = NULL;
    }
    if (string != NULL && newLength != 0) {
        if (m_sString == NULL) {
            m_nBufferLength = newLength + 1;
            m_sString = AllocBuffer(m_nBufferLength);
        }
        strcpy(m_sString, string);
    } else if (m_sString != NULL) {
        FreeBuffer(m_sString);
        m_sString = NULL;
        m_nBufferLength = 0;
    }
    return *this;
}

// 0x02005094, 0x020050e8: NULL e "" contam como iguais.
BOOL CExoString::operator==(const CExoString& string) const
{
    if (m_sString != NULL && string.m_sString != NULL)
        return strcmp(m_sString, string.m_sString) == 0;
    return (m_sString == NULL && string.m_sString == NULL)
        || (m_sString != NULL && m_sString[0] == 0)
        || (string.m_sString != NULL && string.m_sString[0] == 0);
}

BOOL CExoString::operator==(const char* string) const
{
    if (m_sString != NULL && string != NULL)
        return strcmp(m_sString, string) == 0;
    return (m_sString == NULL && string == NULL)
        || (m_sString != NULL && m_sString[0] == 0)
        || (string != NULL && string[0] == 0);
}

// 0x0200512c, 0x02005174
BOOL CExoString::operator!=(const CExoString& string) const
{
    if (m_sString != NULL && string.m_sString != NULL)
        return strcmp(m_sString, string.m_sString) != 0;
    return (m_sString != NULL && m_sString[0] != 0)
        || (string.m_sString != NULL && string.m_sString[0] != 0);
}

BOOL CExoString::operator!=(const char* string) const
{
    if (m_sString != NULL && string != NULL)
        return strcmp(m_sString, string) != 0;
    return (m_sString != NULL && m_sString[0] != 0)
        || (string != NULL && string[0] != 0);
}

// 0x020051b0
BOOL CExoString::operator<(const CExoString& string) const
{
    if (m_sString != NULL && string.m_sString != NULL)
        return strcmp(m_sString, string.m_sString) < 0;
    return string.m_sString != NULL && string.m_sString[0] != 0;
}

// 0x020051e4. O jogo lê m_sString uma vez e usa o mesmo valor depois do strlen:
// daí a variável local.
char CExoString::operator[](int index) const
{
    char* string;
    if (index < 0 || (string = m_sString) == NULL || strlen(string) <= (unsigned int)index)
        return 0;
    return string[index];
}

// 0x02005204: devolve uma CExoString nova (o chamador passa o lugar dela em r0).
CExoString CExoString::operator+(const CExoString& string) const
{
    CExoString result;
    int otherLength = string.m_sString != NULL ? strlen(string.m_sString) : 0;
    int length = m_sString != NULL ? strlen(m_sString) : 0;
    if (otherLength != 0 || length != 0) {
        if (result.m_sString != NULL) {
            result.FreeBuffer(result.m_sString);
            result.m_sString = NULL;
            result.m_nBufferLength = 0;
        }
        if (otherLength == 0) {
            result.m_nBufferLength = length + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            strcpy(result.m_sString, m_sString);
        } else if (length == 0) {
            result.m_nBufferLength = otherLength + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            strcpy(result.m_sString, string.m_sString);
        } else {
            result.m_nBufferLength = length + otherLength + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            strcpy(result.m_sString, m_sString);
            strcat(result.m_sString, string.m_sString);
        }
    }
    return result;
}

// 0x020052a0
CExoString& CExoString::operator+=(const CExoString& string)
{
    *this = *this + string;
    return *this;
}

// 0x020052c4, 0x020052d8
int CExoString::AsINT() const
{
    if (m_sString != NULL)
        return atoi(m_sString);
    return 0;
}

float CExoString::AsFLOAT() const
{
    if (m_sString != NULL)
        return atof(m_sString);
    return 0.0f;
}

// 0x020052f0. O `ldr r0, [r0]` do original mostra que o "" vem de um ponteiro
// guardado numa variável global, e não do endereço da string direto.
const char* CExoString::CStr() const
{
    if (m_sString != 0)
        return m_sString;
    return g_emptyString;
}

// 0x02005300: a posição de string dentro desta, ou -1. Com
// strstr(m_sString, string.CStr()) numa linha só, o compilador lê m_sString antes
// de chamar CStr; o jogo lê depois. A variável `sub` acerta a ordem.
int CExoString::Find(const CExoString& string) const
{
    if (m_sString == NULL || string.m_sString == NULL)
        return -1;
    const char* sub = string.CStr();
    char* s = m_sString;
    char* found = strstr(s, sub);
    if (found == NULL)
        return -1;
    return found - s;
}

// 0x02005334: a posição da letra, ou -1.
int CExoString::Find(char ch) const
{
    char* p = m_sString;
    if (p == NULL)
        return -1;
    int i = 0;
    while (*p != 0) {
        if (*p == ch)
            return i;
        p++;
        i++;
    }
    return -1;
}

// 0x02005364: como sprintf. Tenta com 256 bytes, e com mais 256 a cada vez que o
// vsnprintf diz que não coube.
void CExoString::Format(const char* format, ...)
{
    char* buffer = NULL;
    va_list args;
    va_start(args, format);
    int size = 0;
    int length;
    do {
        if (buffer != NULL)
            FreeBuffer(buffer);
        size += 256;
        buffer = AllocBuffer(size);
        length = vsnprintf(buffer, size - 1, format, args);
    } while (length < 0);
    if (m_sString == NULL || length + 1 > m_nBufferLength) {
        if (m_sString != NULL) {
            FreeBuffer(m_sString);
            m_nBufferLength = 0;
            m_sString = NULL;
        }
        m_nBufferLength = length + 1;
        m_sString = AllocBuffer(m_nBufferLength);
    }
    memcpy(m_sString, buffer, length);
    m_sString[length] = 0;
    FreeBuffer(buffer);
    va_end(args);
}

// 0x020053f0
int CExoString::GetLength() const
{
    if (m_sString != NULL)
        return strlen(m_sString);
    return 0;
}

// 0x02005404: os primeiros `count` caracteres.
CExoString CExoString::Left(int count) const
{
    CExoString result;
    if (m_sString != NULL) {
        unsigned int length = strlen(m_sString);
        if (count >= 0 && length != 0) {
            if (count > length)
                count = length;
            if (result.m_sString != NULL)
                result.FreeBuffer(result.m_sString);
            result.m_nBufferLength = count + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            memcpy(result.m_sString, m_sString, count);
            result.m_sString[count] = 0;
        }
    }
    return result;
}

// 0x02005450: uma cópia em minúsculas (strlwr, do MSL_Extras).
CExoString CExoString::LowerCase() const
{
    CExoString result;
    if (m_sString != NULL) {
        if (result.m_sString != NULL)
            result.FreeBuffer(result.m_sString);
        result.m_nBufferLength = strlen(m_sString) + 1;
        result.m_sString = result.AllocBuffer(result.m_nBufferLength);
        memcpy(result.m_sString, m_sString, result.m_nBufferLength);
        strlwr(result.m_sString);
    }
    return result;
}

// 0x02005490: os últimos `count` caracteres.
CExoString CExoString::Right(int count) const
{
    CExoString result;
    if (m_sString != NULL) {
        unsigned int length = strlen(m_sString);
        if (count >= 0 && length != 0) {
            if (count > length)
                count = length;
            if (result.m_sString != NULL)
                result.FreeBuffer(result.m_sString);
            result.m_nBufferLength = count + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            memcpy(result.m_sString, m_sString + (length - count), count);
            result.m_sString[count] = 0;
        }
    }
    return result;
}

// 0x020054e4: `count` caracteres a partir de `start` (count < 0: até o fim).
CExoString CExoString::SubString(int start, int count) const
{
    CExoString result;
    if (m_sString != NULL) {
        unsigned int length = strlen(m_sString);
        if (start >= 0 && start < length) {
            if (count < 0)
                count = length - start;
            if (start + count > length)
                count = length - start;
            if (result.m_sString != NULL)
                result.FreeBuffer(result.m_sString);
            result.m_nBufferLength = count + 1;
            result.m_sString = result.AllocBuffer(result.m_nBufferLength);
            memcpy(result.m_sString, m_sString + start, count);
            result.m_sString[count] = 0;
        }
    }
    return result;
}

// 0x0200553c: uma cópia em maiúsculas (strupr, do MSL_Extras).
CExoString CExoString::UpperCase() const
{
    CExoString result;
    if (m_sString != NULL) {
        if (result.m_sString != NULL)
            result.FreeBuffer(result.m_sString);
        result.m_nBufferLength = strlen(m_sString) + 1;
        result.m_sString = result.AllocBuffer(result.m_nBufferLength);
        memcpy(result.m_sString, m_sString, result.m_nBufferLength);
        strupr(result.m_sString);
    }
    return result;
}

// 0x0200557c: igual sem diferenciar maiúsculas. Como no Find, o CStr() numa linha
// própria faz o compilador ler m_sString só depois da chamada, como no jogo.
BOOL CExoString::CompareNoCase(const CExoString& string) const
{
    if (string.m_sString == NULL && m_sString == NULL)
        return 1;
    if (string.m_sString == NULL || m_sString == NULL)
        return 0;
    const char* other = string.CStr();
    return stricmp(other, m_sString) == 0;
}
