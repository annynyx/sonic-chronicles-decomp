// CExoString: a string do motor Aurora da BioWare (a mesma família da CExoString do
// Neverwinter Nights, de onde vêm os nomes dos campos).
//
// Layout (12 bytes), deduzido dos construtores (0x02004e1c...):
//   +0x0  vtable           a classe só tem o destrutor virtual
//   +0x4  m_sString        o texto, ou NULL
//   +0x8  m_nBufferLength  o tamanho alocado, contando o 0 do fim
#ifndef AURORA_CEXOSTRING_H
#define AURORA_CEXOSTRING_H

typedef int BOOL;

class CExoString {
public:
    CExoString();
    CExoString(const char* source);
    CExoString(const unsigned short* source);   // texto de 16 bits, um char por letra
    CExoString(const CExoString& source);
    CExoString(int value);
    virtual ~CExoString();

    CExoString& operator=(const CExoString& string);
    CExoString& operator=(const char* string);
    BOOL operator==(const CExoString& string) const;
    BOOL operator==(const char* string) const;
    BOOL operator!=(const CExoString& string) const;
    BOOL operator!=(const char* string) const;
    BOOL operator<(const CExoString& string) const;
    char operator[](int index) const;
    CExoString operator+(const CExoString& string) const;
    CExoString& operator+=(const CExoString& string);

    int AsINT() const;
    float AsFLOAT() const;
    const char* CStr() const;
    int Find(const CExoString& string) const;
    int Find(char ch) const;
    void Format(const char* format, ...);
    int GetLength() const;
    CExoString Left(int count) const;
    CExoString LowerCase() const;
    CExoString Right(int count) const;
    CExoString SubString(int start, int count) const;
    CExoString UpperCase() const;
    BOOL CompareNoCase(const CExoString& string) const;

    char* m_sString;                // +0x4
    unsigned int m_nBufferLength;   // +0x8

private:
    // Os nomes destes dois não são conhecidos: eles só fazem new[] e delete[].
    char* AllocBuffer(unsigned int size);
    void FreeBuffer(char* buffer);
};

// 0x020ef814: ponteiro global para "" (CStr devolve isto quando m_sString é NULL)
extern const char* const g_emptyString;

#endif
