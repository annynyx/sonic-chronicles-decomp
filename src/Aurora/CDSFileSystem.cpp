// O sistema de arquivos: CDSFileSystem (arquivos do cartucho e leituras assíncronas),
// CErfTable e CErfMan (os pacotes HERF) e CErfResource (um recurso pelo nome).
// 0x02008bb0-0x0200a308. O build usa este fonte até 0x0200a1b0; as 7 cópias de
// template do fim continuam do assembly, porque duas delas (CExoArrayList<T>::DelIndex)
// ainda não batem (docs/DIARIO.md, seção 26). As que este .o gera cedem o lugar.
#include "Aurora/Comum.h"
#include "Aurora/CErfMan.h"
#include "msl.h"

// 0x020b7dac (outro arquivo): CRC-32 dos primeiros "length" bytes da string.
u32 Crc32(const CExoString& string, u32 length);

extern u8 __lower_map[];    // 0x020f4a50: tabela 'A'..'Z' -> 'a'..'z' (128 bytes, MSL)

// 0x02008bb0
ResName::ResName(const char* name) : m_name(name)
{
    strupr((char*)m_name.CStr());
    m_hash = Crc32(m_name, m_name.GetLength());
}

// 0x02008bd8
BOOL ResName::operator==(const ResName& other) const
{
    if (other.m_hash == m_hash)
        return TRUE;
    return FALSE;
}

// 0x02008be8: liga o sistema de arquivos do SDK e carrega a tabela de arquivos do
// cartucho (FS_TryLoadTable) para a memória.
BOOL CDSFileSystem::Init(int dma)
{
    m_dma = dma;
    FS_Init(m_dma);
    FS_SetDefaultDMA(m_dma);
    u32 size = FS_TryLoadTable(NULL, 0);
    FS_TryLoadTable(new u8[size], size);
    m_nextId = 0;
    m_current = -1;
    AddPath("/Override/");
    AddPath("/");
    AddPath("/DesignerExportData/");
    AddPath("/Background/");
    AddPath("/GUI/CommonGUI/");
    AddPath("/GUI/CombatGUI/");
    AddPath("/GUI/ExploreGUI/");
    AddPath("/GUI/Conversation/");
    AddPath("/GUI/StartUpGUI/");
    AddPath("/GUI/Admin/");
    AddPath("/GUI/QuestLogGUI/");
    AddPath("/Videos/");
    return FS_IsAvailable();
}

// 0x02008cb0
void CDSFileSystem::FreeCache()
{
    for (int i = 0; i < m_cache.num; i++) {
        delete[] m_cache[i].data;
        m_cache[i].data = NULL;
        CloseFile(&m_cache[i].id);
    }
    m_cache.Clear();
}

// 0x02008cf8
BOOL CDSFileSystem::Shutdown()
{
    FS_End();
    return TRUE;
}

// 0x02008d04: chamada a cada quadro. Conta o tempo do pedido em leitura, tira da fila
// os que terminaram e começa o próximo.
void CDSFileSystem::Update()
{
    if (m_current >= 0) {
        // Num campo de bits, o compilador expande o -- como "x = x - 1" repetindo a
        // expressão inteira: operator[] é chamado duas vezes, como no jogo.
        m_requests[m_current].timeout--;
        ProcessCurrent();
    }
    if (m_current < 0) {
        for (int i = m_requests.num - 1; i >= 0; i--) {
            if (m_requests[i].state == REQUEST_FREE || m_requests[i].state == REQUEST_DONE)
                m_requests.DelIndex(i);
        }
    }
    for (int i = 0; i < m_requests.num; i++) {
        if (m_requests[i].state == REQUEST_WAITING && StartRequest(&m_requests[i]))
            m_current = i;
    }
}

// 0x02008dd0
void CDSFileSystem::CancelRequests(u8* data, BOOL cancel)
{
    for (int i = m_requests.num - 1; i >= 0; i--) {
        if (*m_requests[i].dst == data)
            FinishRequest(&m_requests[i], cancel);
    }
}

// 0x02008e0c
void CDSFileSystem::CancelRequests(u16* id, BOOL cancel)
{
    for (int i = m_requests.num - 1; i >= 0; i--) {
        if (*id == m_requests[i].id)
            FinishRequest(&m_requests[i], cancel);
    }
}

// 0x02008e48: encerra um pedido. Cancelado, ele só sai da fila; senão, espera a
// leitura, descomprime se for o caso e chama o callback.
void CDSFileSystem::FinishRequest(CDSRequest* request, BOOL cancel)
{
    if (FindFile(&request->id) < 0)
        return;
    if (cancel) {
        if (request->state == REQUEST_READING) {
            FS_CancelFile(&request->file);
            OS_Sleep(1);
            FS_WaitAsync(&request->file);
        }
        request->state = REQUEST_FREE;
    } else if ((int)request->state == REQUEST_WAITING || (int)request->state == REQUEST_READING) {
        // O (int) faz o compilador testar "estado - 1 <= 1" em 32 bits, como o jogo;
        // sem ele, a conta sai em 16 bits (o tipo do campo).
        request->state = REQUEST_DONE;
        if (FS_WaitAsync(&request->file)) {
            if (request->compressed) {
                DC_FlushRange(*request->dst, request->size);
                request->size = Decompress(request->dst);
            }
            if (request->callback != NULL)
                request->callback(&request->id, *request->dst, request->size, request->userData);
        }
    }
    if (m_current >= 0 && request == &m_requests[m_current])
        m_current = -1;
}

// 0x02008f0c
void CDSFileSystem::AddPath(const char* path)
{
    m_paths.Add(CExoString(path));
}

// 0x02008f2c: os arquivos ficam na raiz do NitroFS ("./nome"). "path" não é usado.
BOOL CDSFileSystem::OpenPath(const char* name, CExoString& path, FSFile* file)
{
    char buffer[512];
    memcpy(buffer, "./", 3);
    strcat(buffer, name);
    if (FS_OpenFile(file, buffer))
        return TRUE;
    return FALSE;
}

// 0x02008f68
int CDSFileSystem::FindFile(u16* id)
{
    if (*id == 0xffff)
        return -1;
    for (int i = 0; i < m_files.num; i++) {
        if (*id == m_files[i].id)
            return i;
    }
    return -1;
}

// 0x02008fb0
int CDSFileSystem::FindFile(ResName& name)
{
    for (int i = 0; i < m_files.num; i++) {
        if (m_files[i].name == name)
            return i;
    }
    return -1;
}

// 0x02008fe8: abre um arquivo (ou só soma 1 no contador, se já estiver aberto) e
// devolve o id dele. "small": abrir a versão comprimida, "NOME.SMALL".
BOOL CDSFileSystem::OpenFile(const char* name, u16* id, BOOL small)
{
    if (m_files.num >= 0xffff)
        return FALSE;
    FSFile file;
    FS_InitFile(&file);
    ResName resName(name);
    CExoString path;
    int index = FindFile(resName);
    if (index >= 0) {
        m_files[index].count++;     // como o -- de Update: operator[] duas vezes
        if (id != NULL)
            *id = m_files[index].id;
        return TRUE;
    }
    if (small)
        resName.m_name += CExoString(".SMALL");
    if (!OpenPath(resName.m_name.CStr(), path, &file))
        return FALSE;
    CDSOpenFile entry;
    entry.small = small;
    entry.count = 1;
    entry.id = ++m_nextId;
    entry.name = resName;
    entry.file = file;
    m_files.Add(entry);
    if (id != NULL)
        *id = entry.id;
    return TRUE;
}

// 0x0200919c: devolve quantas vezes o arquivo ainda está aberto.
int CDSFileSystem::CloseFile(u16* id)
{
    if (*id == 0xffff)
        return -1;
    int count = 0;
    int index = FindFile(id);
    if (index >= 0) {
        count = --m_files[index].count;
        if (count <= 0) {
            m_files[index].count = 0;
            m_files[index].id = 0;
            CancelRequests(id, TRUE);
            FS_CloseFile(&m_files[index].file);
            *id = 0xffff;
            m_files.DelIndex(index);
        }
    }
    return count;
}

// 0x0200924c
u32 CDSFileSystem::GetLength(FSFile* file)
{
    return FS_GetLength(file);
}

// 0x02009254
u32 CDSFileSystem::GetFileSize(u16* id)
{
    u32 size = 0;
    int index = FindFile(id);
    if (index >= 0)
        size = GetLength(&m_files[index].file);
    return size;
}

// 0x02009278: lê o arquivo do começo. size == 0: o arquivo inteiro.
BOOL CDSFileSystem::ReadFile(u16* id, void* dst, u32 size)
{
    if (dst == NULL)
        return FALSE;
    int index = FindFile(id);
    if (index < 0)
        return FALSE;
    if (size == 0)
        size = GetLength(&m_files[index].file);
    s32 read;
    // lido uma vez só: o jogo reusa o valor para achar a entrada do cache
    int cache = m_files[index].cache;
    if (cache > 0) {
        read = size;
        MI_CpuCopy8(m_cache[cache - 1].data, dst, size);
        DC_FlushRange(dst, size);
    } else {
        FS_SeekFile(&m_files[index].file, 0, FS_SEEK_SET);
        read = FS_ReadFile(&m_files[index].file, dst, size);
        DC_FlushRange(dst, size);
    }
    if (read == size)
        return TRUE;
    return FALSE;
}

// 0x02009314
BOOL CDSFileSystem::ReadFileAt(u16* id, void* dst, u32 offset, u32 size)
{
    if (dst == NULL)
        return FALSE;
    int index = FindFile(id);
    if (index < 0)
        return FALSE;
    s32 read;
    int cache = m_files[index].cache;
    if (cache > 0) {
        read = size;
        MI_CpuCopy8(m_cache[cache - 1].data + offset, dst, size);
        DC_FlushRange(dst, size);
    } else {
        FS_SeekFile(&m_files[index].file, offset, FS_SEEK_SET);
        read = FS_ReadFile(&m_files[index].file, dst, size);
        DC_FlushRange(dst, size);
    }
    if (read == size)
        return TRUE;
    return FALSE;
}

// 0x020093ac: põe um pedido na fila. A leitura começa em Update.
BOOL CDSFileSystem::ReadFileAsync(u16* id, u8** dst, u32 offset, u32 size, CDSReadCallback callback,
                                  void* userData)
{
    if (dst == NULL)
        return FALSE;
    int index = FindFile(id);
    if (index < 0)
        return FALSE;
    if (size == 0)
        size = GetLength(&m_files[index].file);
    if (*dst == NULL)
        *dst = new u8[size];
    if (*dst == NULL)
        return FALSE;
    m_requests.AddEmpty();
    CDSRequest* request = &m_requests[m_requests.num - 1];
    request->compressed = m_files[index].small;
    request->cached = m_files[index].cache != 0;
    request->id = *id;
    request->file = m_files[index].file;
    request->callback = callback;
    request->userData = userData;
    request->offset = offset;
    request->size = size;
    request->dst = dst;
    request->state = REQUEST_WAITING;
    return TRUE;
}

// 0x020094f4: confere o pedido em leitura. Devolve TRUE enquanto houver um.
BOOL CDSFileSystem::ProcessCurrent()
{
    if (m_current >= 0) {
        BOOL done;
        if (m_requests[m_current].cached) {
            done = TRUE;
        } else {
            done = FALSE;
            if (!FS_IsBusy(&m_requests[m_current].file) && FS_IsSucceeded(&m_requests[m_current].file) == TRUE)
                done = TRUE;
        }
        if (done) {
            FinishRequest(&m_requests[m_current], FALSE);
        } else if (m_requests[m_current].timeout == 0) {
            // demorou demais: cancela e põe de novo na fila
            CDSRequest* request = &m_requests[m_current];
            FinishRequest(&m_requests[m_current], TRUE);
            request->state = REQUEST_WAITING;
        }
    }
    if (m_current >= 0)
        return TRUE;
    return FALSE;
}

// 0x020095b8
BOOL CDSFileSystem::StartRequest(CDSRequest* request)
{
    s32 read = 0;
    if (ProcessCurrent())
        return read;
    if (request->cached) {
        int index = FindFile(&request->id);
        int cache;
        if (index >= 0 && (cache = m_files[index].cache) > 0) {
            u32 size = request->size;
            MI_CpuCopy8(m_cache[cache - 1].data, *request->dst, size);
            DC_FlushRange(*request->dst, size);
            request->compressed = 0;
            request->state = REQUEST_DONE;
            request->timeout = 60;
            return TRUE;
        }
    }
    FS_WaitAsync(&request->file);
    if (FS_SeekFile(&request->file, request->offset, FS_SEEK_SET))
        read = FS_ReadFileAsync(&request->file, *request->dst, request->size);
    if (read == request->size) {
        request->state = REQUEST_READING;
        request->timeout = 60;
        return TRUE;
    }
    FinishRequest(request, TRUE);
    return FALSE;
}

// 0x020096b8: descomprime um bloco no formato do BIOS (cabeçalho de 4 bytes: tipo no
// byte 0, tamanho descomprimido nos outros 3). Tipo 0: sem compressão.
u32 CDSFileSystem::Decompress(const void* src, u8** dst)
{
    if (dst == NULL)
        return 0;
    u32 size = *(const u32*)src >> 8;
    u32 result = size;
    if (*dst == NULL)
        *dst = new u8[size];
    u32 type = *(const u32*)src & 0xf0;
    if (type == 0) {
        memcpy(*dst, (const u8*)src + 4, size);
    } else {
        switch (type) {
        case 0x10:
            MI_UncompressLZ8(src, *dst);
            break;
        case 0x30:
            MI_UncompressRL8(src, *dst);
            break;
        case 0x20:
            MI_UncompressHuffman(src, *dst);
            break;
        default:
            result = 0;
            break;
        }
    }
    if (result != 0)
        DC_FlushRange(*dst, size);
    return result;
}

// 0x0200972c: troca o bloco comprimido pelo descomprimido.
u32 CDSFileSystem::Decompress(u8** data)
{
    u8* out = NULL;
    u32 size = Decompress(*data, &out);
    if (size != 0) {
        delete[] *data;
        *data = out;
    }
    return size;
}

// 0x02009750 e 0x02009768
CErfMan::~CErfMan()
{
}

// 0x02009788
void CErfMan::Update()
{
}

// 0x0200978c: o pacote comum e o do idioma do console (inglês para os outros).
void CErfMan::LoadTables()
{
    m_tables[0].Open("test.herf");
    OSOwnerInfo info;
    OS_GetOwnerInfo(&info);
    CExoString unused;
    switch (info.language) {
    case 2:
        m_tables[1].Open("test_f.herf");
        break;
    case 4:
        m_tables[1].Open("test_i.herf");
        break;
    case 3:
        m_tables[1].Open("test_g.herf");
        break;
    case 5:
        m_tables[1].Open("test_s.herf");
        break;
    default:
        m_tables[1].Open("test_e.herf");
        break;
    }
}

// 0x02009824
CErfTable* CErfMan::Find(u32 hash, int* index)
{
    for (int i = 0; i < 2; i++) {
        *index = m_tables[i].Find(hash);
        if (*index != -1)
            return &m_tables[i];
    }
    *index = -1;
    return NULL;
}

// 0x0200985c e 0x020098f0. Um erro do jogo: o índice só é apagado quando é NULL, ou
// seja, nunca (sem efeito: o pacote vive até o fim).
CErfTable::~CErfTable()
{
    if (m_entries == NULL) {
        delete[] m_entries;
        m_entries = NULL;
    }
    CDSFileSystem::Instance().CloseFile(&m_fileId);
}

// 0x02009988: abre o pacote e lê o índice. O cabeçalho (8 bytes) não é apagado.
void CErfTable::Open(const char* name)
{
    CDSFileSystem::Instance().OpenFile(name, &m_fileId, FALSE);
    u8* header = new u8[8];
    CDSFileSystem::Instance().ReadFileAt(&m_fileId, header, 0, 8);
    u32 version = 0;
    u32 count = 0;
    memcpy(&version, header, 4);
    memcpy(&count, header + 4, 4);
    m_entries = new u8[count * 12];
    m_count = count;
    CDSFileSystem::Instance().ReadFileAt(&m_fileId, m_entries, 8, count * 12);
}

// 0x02009b1c: busca binária pelo hash. Devolve a posição ou -1.
int CErfTable::Find(u32 hash)
{
    int low = 0;
    int high = m_count - 1;
    while (low <= high) {
        int middle = (high + low) / 2;
        u32 entry;
        memcpy(&entry, m_entries + middle * 12, 4);
        if (entry > hash)
            high = middle - 1;
        else if (entry < hash)
            low = middle + 1;
        else
            return middle;
    }
    return -1;
}

// 0x02009b78: DJB2 sem diferenciar maiúsculas. Com tudo numa expressão só, a última
// soma sai com os operandos trocados; a variável "l" resolve.
inline int tolower_ascii(int c)
{
    return ((c < 0) || (c >= 0x80)) ? c : (int)__lower_map[c];
}

u32 HashResourceName(const CExoString& name)
{
    u32 hash = 5381;
    const char* s = name.CStr();
    for (char c = *s++; c != 0; c = *s++) {
        int l = tolower_ascii(c);
        hash = (hash << 5) + hash + l;
    }
    return hash;
}

// 0x02009bbc
u32 CErfTable::GetOffset(int index)
{
    u32 offset = 0;
    if (index >= 0 && index < m_count)
        memcpy(&offset, m_entries + index * 12 + 8, 4);
    return offset;
}

// 0x02009bf4
int CErfTable::GetSize(int index)
{
    int size = 0;
    if (index >= 0 && index < m_count)
        memcpy(&size, m_entries + index * 12 + 4, 4);
    return size;
}

// 0x02009c28: lê um recurso inteiro. "compressed": o recurso é um .small, e sai
// descomprimido (o mesmo código de CDSFileSystem::Decompress, repetido).
BOOL CErfTable::Read(int index, u8** dst, int* size, BOOL compressed)
{
    if (index >= 0 && index < m_count && (*size = GetSize(index)) > 0) {
        if (!compressed) {
            *dst = new u8[*size];
            CDSFileSystem::Instance().ReadFileAt(&m_fileId, *dst, GetOffset(index), *size);
        } else {
            u32 header = 0;
            CDSFileSystem::Instance().ReadFileAt(&m_fileId, &header, GetOffset(index), 4);
            header >>= 8;
            u32 result = header;
            u8* out = new u8[header];
            *dst = new u8[*size];
            CDSFileSystem::Instance().ReadFileAt(&m_fileId, *dst, GetOffset(index), *size);
            u32 type = *(u32*)*dst & 0xf0;
            if (type == 0) {
                memcpy(out, *dst + 4, header);
            } else {
                switch (type) {
                case 0x10:
                    MI_UncompressLZ8(*dst, out);
                    break;
                case 0x30:
                    MI_UncompressRL8(*dst, out);
                    break;
                case 0x20:
                    MI_UncompressHuffman(*dst, out);
                    break;
                default:
                    result = 0;
                    break;
                }
            }
            if (result != 0)
                DC_FlushRange(out, header);
            *size = result;
            delete[] *dst;
            *dst = out;
        }
        return TRUE;
    }
    return FALSE;
}

// 0x02009e3c: lê um pedaço de um recurso (sem descomprimir).
BOOL CErfTable::Read(int index, u8** dst, u32 offset, int size, BOOL compressed)
{
    if (index >= 0 && index < m_count && size > 0) {
        CDSFileSystem::Instance().ReadFileAt(&m_fileId, *dst, offset + GetOffset(index), size);
        return TRUE;
    }
    return FALSE;
}

// 0x02009ee4
CErfResource::CErfResource(const CExoString& name, int small)
{
    CExoString fullName(name);
    if (small == 1)
        fullName += CExoString(".small");
    m_hash = HashResourceName(fullName);
    m_index = -1;
    m_small = small;
}

// 0x02009f2c
BOOL CErfResource::Read(u8** dst, int* size)
{
    if (m_table == NULL || m_index == -1) {
        m_table = CErfMan::Instance().Find(m_hash, &m_index);
        if (m_table == NULL || m_index == -1)
            return FALSE;
    }
    return m_table->Read(m_index, dst, size, m_small);
}

// 0x02009fcc
BOOL CErfResource::Read(u8** dst, u32 offset, int size)
{
    if (m_table == NULL || m_index == -1) {
        m_table = CErfMan::Instance().Find(m_hash, &m_index);
        if (m_table == NULL || m_index == -1)
            return FALSE;
    }
    return m_table->Read(m_index, dst, offset, size, m_small);
}

// 0x0200a070
int CErfResource::GetSize()
{
    if (m_table == NULL || m_index == -1) {
        m_table = CErfMan::Instance().Find(m_hash, &m_index);
        if (m_table == NULL || m_index == -1)
            return 0;
    }
    return m_table->GetSize(m_index);
}
