// CTlkTable: os textos do jogo (os .tlk). 0x02098930-0x02098f24.
//
// Ainda não é ligado (não está "complete" em delinks.txt): 12 das 13 funções do arquivo
// batem byte a byte e são conferidas uma a uma pelo casos.txt, mas a CTlkTable::GetString
// (0x020989fc, no meio do .text) não bate e continua do assembly. A busca que ela faz
// (o hash de Wang, a sondagem linear e a leitura do texto) e a tentativa de C++ estão no
// DIARIO 28. Quando ela bater, entra aqui, entre Reset e AddFile, e o arquivo é ligado.
#include "Aurora/Comum.h"
#include "Aurora/CTlkTable.h"
#include "msl.h"

// 0x02098930
CTlkTable::CTlkTable() : m_unknown4(0), m_unknown8(0), m_files(0)
{
    Reset();
    m_textSize = 0x800;
    m_text = new char[m_textSize];
}

// 0x02098964 e 0x020989ac. O buffer de cada arquivo não é apagado.
CTlkTable::~CTlkTable()
{
    for (int i = 0; i < m_files.num; i++)
        delete m_files[i].file;
}

// 0x020989f8
void CTlkTable::Reset()
{
}

// 0x02098d90
BOOL CTlkTable::AddFile(const CExoString& name)
{
    CTlkFile* file = new CTlkFile(name);
    CTlkTableEntry entry;
    entry.file = file;
    m_files.Add(entry);
    return TRUE;
}

// 0x02098dd4
CTlkFile::CTlkFile(CExoString name)
{
    m_name = name;
    m_dataOffset = 0;
    m_count = 0;
    m_bufferSize = 0x200;
    m_buffer = new u8[m_bufferSize];
    CDSFileSystem::Instance().OpenFile(m_name.CStr(), &m_fileId, FALSE);
}

// 0x02098e7c
void CTlkTable::Unknown2()
{
}
