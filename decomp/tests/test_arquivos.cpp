// Teste com dados reais da Fase 3: o C++ do sistema de arquivos que gera os bytes do
// jogo (src/Aurora/CDSFileSystem.cpp e CExoString.cpp, sem mudar nada), compilado no
// PC e rodando sobre os pacotes HERF extraídos da ROM.
//
// O que ele confere:
//  1. o erf.dict (os nomes oficiais de todos os recursos) sai pela cadeia inteira do
//     jogo: CErfResource -> CErfMan -> CErfTable -> CDSFileSystem;
//  2. para cada nome: HashResourceName dá o hash do erf.dict; CErfResource acha o
//     recurso e dá o tamanho; a leitura devolve os mesmos bytes que uma leitura
//     independente do índice (feita aqui, sem o código do jogo); um .small sai
//     descomprimido, com o tamanho do cabeçalho;
//  3. a leitura de um pedaço (CErfResource::Read com posição) e a fila de leitura em
//     segundo plano (ReadFileAsync + Update + callback);
//  4. o pacote do idioma: para cada idioma do console, LoadTables abre o test_X.herf
//     certo (inglês para japonês, inglês e qualquer outro).
// Tudo isso com o alocador do jogo (src/Aurora/Memory.cpp) atendendo cada new e delete.
//
// Uso: decomp/build/test_arquivos work/extract/files   (depois do montar_rom.sh)
#include <map>
#include <string>
#include <vector>
#include "Aurora/CErfMan.h"
#include "msl.h"

extern const char* g_pastaPC;
extern int g_idiomaPC;

static int falhas = 0;

#define CONFERE(cond, ...)                                     \
    do {                                                       \
        if (!(cond)) {                                         \
            if (falhas++ < 20) {                               \
                printf("FALHA (%s:%d): ", __FILE__, __LINE__); \
                printf(__VA_ARGS__);                           \
                printf("\n");                                  \
            }                                                  \
        }                                                      \
    } while (0)

// Leitura independente de um pacote: o arquivo inteiro e o índice num map.
struct Pacote {
    std::vector<u8> dados;
    std::map<u32, std::pair<u32, u32> > indice;   // hash -> (tamanho, posição)

    bool Abre(const char* nome)
    {
        std::string caminho = std::string(g_pastaPC) + "/" + nome;
        FILE* f = fopen(caminho.c_str(), "rb");
        if (f == NULL)
            return false;
        fseek(f, 0, SEEK_END);
        dados.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        if (fread(&dados[0], 1, dados.size(), f) != dados.size())
            return false;
        fclose(f);
        u32 n = Palavra(4);
        for (u32 i = 0; i < n; i++)
            indice[Palavra(8 + 12 * i)] = std::make_pair(Palavra(12 + 12 * i), Palavra(16 + 12 * i));
        return Palavra(0) == 0x00f1a5c0;
    }
    u32 Palavra(u32 p) const { return dados[p] | dados[p + 1] << 8 | dados[p + 2] << 16 | (u32)dados[p + 3] << 24; }
};

// O nome do arquivo aberto com este id (o ResName do CDSFileSystem, em maiúsculas).
static std::string NomeAberto(u16 id)
{
    CDSFileSystem& fs = CDSFileSystem::Instance();
    int i = fs.FindFile(&id);
    return i < 0 ? std::string("?") : std::string(fs.m_files[i].name.m_name.CStr());
}

static bool s_chegou;
static void AoLer(u16* id, u8* data, u32 size, void* userData)
{
    std::vector<u8>* esperado = (std::vector<u8>*)userData;
    s_chegou = true;
    CONFERE(size == esperado->size() && memcmp(data, &(*esperado)[0], size) == 0,
            "a leitura em segundo plano devolveu outros bytes (id %d)", *id);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "uso: %s PASTA_COM_test.herf (work/extract/files)\n", argv[0]);
        return 2;
    }
    g_pastaPC = argv[1];

    // 0. o alocador do jogo (src/Aurora/Memory.cpp) atende todos os new deste teste. Um
    // pedido de até 16 bytes sai do pool de blocos de 16. Devolver um bloco o põe na
    // vaga seguinte da lista de livres, sem voltar o contador: ele é o próximo a sair, e
    // o bloco que ocupava a vaga (o vizinho de cima) nunca mais sai.
    char* a = new char[10];
    delete[] a;
    char* b = new char[10];
    char* c = new char[10];
    CONFERE(b == a, "o bloco devolvido devia ser o próximo a sair");
    CONFERE(c == a + 32, "o vizinho de cima do bloco devolvido devia ter se perdido");
    delete[] b;
    delete[] c;
    printf("alocador do jogo: o bloco devolvido sai de novo, o vizinho se perde\n");

    Pacote comum, ingles;
    if (!comum.Abre("test.herf") || !ingles.Abre("test_e.herf")) {
        fprintf(stderr, "não achei test.herf e test_e.herf em %s\n", g_pastaPC);
        return 2;
    }

    // 1. o erf.dict pela cadeia do jogo
    g_idiomaPC = 1;
    CErfMan::Instance().LoadTables();
    CErfResource dict(CExoString("erf.dict"), 0);
    u8* bytes = NULL;
    int tamanho = 0;
    CONFERE(dict.Read(&bytes, &tamanho), "o erf.dict não foi lido");
    if (bytes == NULL)
        return 1;
    u32 n = bytes[4] | bytes[5] << 8 | bytes[6] << 16 | (u32)bytes[7] << 24;
    printf("erf.dict: %d bytes, %u nomes\n", tamanho, n);
    CONFERE(tamanho >= 8 && 8 + 132ull * n <= (u32)tamanho, "o erf.dict não tem o tamanho que diz ter");
    if (falhas)
        return 1;

    // 2. cada recurso pelo nome
    u32 lidos = 0, comprimidos = 0, crus = 0, fora = 0, vazias = 0;
    for (u32 i = 0; i < n; i++) {
        const u8* e = bytes + 8 + 132 * i;
        u32 hash = e[0] | e[1] << 8 | e[2] << 16 | (u32)e[3] << 24;
        std::string nome((const char*)e + 4);
        if (hash == 0 && nome.empty()) {
            vazias++;                // o erf.dict termina com entradas zeradas
            continue;
        }
        CONFERE(HashResourceName(CExoString(nome.c_str())) == hash, "hash de %s", nome.c_str());

        const Pacote* p = comum.indice.count(hash) ? &comum : ingles.indice.count(hash) ? &ingles : NULL;
        if (p == NULL) {
            fora++;                  // está num pacote de outro idioma
            continue;
        }
        u32 tam = p->indice.find(hash)->second.first, pos = p->indice.find(hash)->second.second;
        bool small = nome.size() > 6 && nome.compare(nome.size() - 6, 6, ".small") == 0;
        CErfResource r(CExoString(small ? nome.substr(0, nome.size() - 6).c_str() : nome.c_str()), small);
        CONFERE(r.m_hash == hash, "CErfResource(%s) calculou outro hash", nome.c_str());
        CONFERE(r.GetSize() == (int)tam, "tamanho de %s: %d, o índice diz %u", nome.c_str(), r.GetSize(), tam);

        u8* buffer = NULL;
        int obtido = 0;
        CONFERE(r.Read(&buffer, &obtido), "%s não foi lido", nome.c_str());
        if (buffer == NULL)
            continue;
        const u8* cru = &p->dados[pos];
        if (small) {
            u32 cabecalho = cru[0] | cru[1] << 8 | cru[2] << 16 | (u32)cru[3] << 24;
            CONFERE((u32)obtido == cabecalho >> 8, "%s: %d bytes descomprimidos, o cabeçalho diz %u",
                    nome.c_str(), obtido, cabecalho >> 8);
            if ((cabecalho & 0xf0) == 0) {
                CONFERE(memcmp(buffer, cru + 4, obtido) == 0, "%s (sem compressão) mudou", nome.c_str());
                crus++;
            } else {
                comprimidos++;
            }
        } else {
            CONFERE((u32)obtido == tam && memcmp(buffer, cru, tam) == 0, "%s: bytes diferentes", nome.c_str());
        }
        delete[] buffer;
        lidos++;

        // 3a. um pedaço do meio, de um recurso normal
        if (!small && tam >= 12) {
            u8 pedaco[8];
            u8* destino = pedaco;
            CONFERE(r.Read(&destino, 4, 8), "pedaço de %s", nome.c_str());
            CONFERE(memcmp(pedaco, cru + 4, 8) == 0, "pedaço de %s: bytes diferentes", nome.c_str());
        }
    }
    printf("recursos lidos pelo nome: %u (%u .small em LZ, %u .small sem compressão); "
           "%u em pacotes de outros idiomas; %u entradas vazias no fim\n", lidos, comprimidos, crus, fora, vazias);
    CONFERE(fora == 0 && lidos + vazias == n, "%u nomes sem recurso", n - lidos - vazias);
    delete[] bytes;

    // 3b. a fila de leitura em segundo plano, num arquivo que ninguém abriu ainda (o
    // test_g.herf): ao fechar, o contador chega a 0 e o id volta a 0xffff
    CDSFileSystem& fs = CDSFileSystem::Instance();
    Pacote alemao;
    CONFERE(alemao.Abre("test_g.herf"), "test_g.herf");
    u16 id = 0xffff;
    CONFERE(fs.OpenFile("test_g.herf", &id, FALSE), "OpenFile(test_g.herf)");
    std::vector<u8> esperado(alemao.dados.begin() + 100, alemao.dados.begin() + 100 + 4000);
    u8* destino = NULL;
    s_chegou = false;
    CONFERE(fs.ReadFileAsync(&id, &destino, 100, 4000, AoLer, &esperado), "ReadFileAsync");
    for (int quadro = 0; quadro < 10 && !s_chegou; quadro++)
        fs.Update();
    CONFERE(s_chegou, "a leitura em segundo plano não terminou");
    CONFERE(fs.CloseFile(&id) == 0 && id == 0xffff, "CloseFile");
    delete[] destino;

    // 4. o pacote do idioma
    const char* esperados[] = {"TEST_E.HERF", "TEST_E.HERF", "TEST_F.HERF", "TEST_G.HERF", "TEST_I.HERF",
                               "TEST_S.HERF", "TEST_E.HERF"};
    for (int idioma = 0; idioma <= 6; idioma++) {
        g_idiomaPC = idioma;
        CErfMan::Instance().LoadTables();
        std::string aberto = NomeAberto(CErfMan::Instance().m_tables[1].m_fileId);
        CONFERE(aberto == esperados[idioma], "idioma %d abriu %s", idioma, aberto.c_str());
    }
    printf("idiomas 0 a 6: o pacote certo\n");

    printf(falhas ? "%d FALHA(S)\n" : "OK\n", falhas);
    return falhas ? 1 : 0;
}
